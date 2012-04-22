/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGenSQLResultsLayer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
#include "ogr_p.h"
#include "ogr_gensql.h"
#include "cpl_string.h"
#include <vector>

CPL_CVSID("$Id$");


/************************************************************************/
/*               OGRGenSQLResultsLayerHasSpecialField()                 */
/************************************************************************/

static
int OGRGenSQLResultsLayerHasSpecialField(swq_expr_node* expr,
                                         int nMinIndexForSpecialField)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if (expr->table_index == 0)
        {
            return expr->field_index >= nMinIndexForSpecialField;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for( int i = 0; i < expr->nSubExprCount; i++ )
        {
            if (OGRGenSQLResultsLayerHasSpecialField(expr->papoSubExpr[i],
                                                     nMinIndexForSpecialField))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                       OGRGenSQLResultsLayer()                        */
/************************************************************************/

OGRGenSQLResultsLayer::OGRGenSQLResultsLayer( OGRDataSource *poSrcDS,
                                              void *pSelectInfo, 
                                              OGRGeometry *poSpatFilter,
                                              const char *pszWHERE,
                                              const char *pszDialect )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    this->poSrcDS = poSrcDS;
    this->pSelectInfo = pSelectInfo;
    poDefn = NULL;
    poSummaryFeature = NULL;
    panFIDIndex = NULL;
    bOrderByValid = FALSE;
    nIndexSize = 0;
    nNextIndexFID = 0;
    nExtraDSCount = 0;
    papoExtraDS = NULL;

/* -------------------------------------------------------------------- */
/*      Identify all the layers involved in the SELECT.                 */
/* -------------------------------------------------------------------- */
    int iTable;

    papoTableLayers = (OGRLayer **) 
        CPLCalloc( sizeof(OGRLayer *), psSelectInfo->table_count );

    for( iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        OGRDataSource *poTableDS = poSrcDS;

        if( psTableDef->data_source != NULL )
        {
            OGRSFDriverRegistrar *poReg=OGRSFDriverRegistrar::GetRegistrar();

            poTableDS = 
                poReg->OpenShared( psTableDef->data_source, FALSE, NULL );
            if( poTableDS == NULL )
            {
                if( strlen(CPLGetLastErrorMsg()) == 0 )
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Unable to open secondary datasource\n"
                              "`%s' required by JOIN.",
                              psTableDef->data_source );
                return;
            }

            papoExtraDS = (OGRDataSource **)
                CPLRealloc( papoExtraDS, sizeof(void*) * ++nExtraDSCount );

            papoExtraDS[nExtraDSCount-1] = poTableDS;
        }

        papoTableLayers[iTable] = 
            poTableDS->GetLayerByName( psTableDef->table_name );
        
        CPLAssert( papoTableLayers[iTable] != NULL );

        if( papoTableLayers[iTable] == NULL )
            return;
    }
    
    poSrcLayer = papoTableLayers[0];

/* -------------------------------------------------------------------- */
/*      If the user has explicitely requested a OGRSQL dialect, then    */
/*      we should avoid to forward the where clause to the source layer */
/*      when there is a risk it cannot understand it (#4022)            */
/* -------------------------------------------------------------------- */
    int bForwardWhereToSourceLayer = TRUE;
    if( pszWHERE )
    {
        if( psSelectInfo->where_expr && pszDialect != NULL &&
            EQUAL(pszDialect, "OGRSQL") )
        {
            int nMinIndexForSpecialField = poSrcLayer->GetLayerDefn()->GetFieldCount();
            bForwardWhereToSourceLayer = !OGRGenSQLResultsLayerHasSpecialField
                            (psSelectInfo->where_expr, nMinIndexForSpecialField);
        }
        if (bForwardWhereToSourceLayer)
            this->pszWHERE = CPLStrdup(pszWHERE);
        else
            this->pszWHERE = NULL;
    }
    else
        this->pszWHERE = NULL;

/* -------------------------------------------------------------------- */
/*      Now that we have poSrcLayer, we can install a spatial filter    */
/*      if there is one.                                                */
/* -------------------------------------------------------------------- */
    if( poSpatFilter != NULL )
        SetSpatialFilter( poSpatFilter );

/* -------------------------------------------------------------------- */
/*      Prepare a feature definition based on the query.                */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();

    poDefn = new OGRFeatureDefn( psSelectInfo->table_defs[0].table_alias );
    poDefn->Reference();

    iFIDFieldIndex = poSrcDefn->GetFieldCount();

    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        OGRFieldDefn oFDefn( "", OFTInteger );
        OGRFieldDefn *poSrcFDefn = NULL;
        OGRFeatureDefn *poLayerDefn = NULL;

        if( psColDef->table_index != -1 )
            poLayerDefn = 
                papoTableLayers[psColDef->table_index]->GetLayerDefn();

        if( psColDef->field_index > -1 
            && poLayerDefn != NULL
            && psColDef->field_index < poLayerDefn->GetFieldCount() )
            poSrcFDefn = poLayerDefn->GetFieldDefn(psColDef->field_index);

        if( strlen(psColDef->field_name) == 0 )
        {
            CPLFree( psColDef->field_name );
            psColDef->field_name = (char *) CPLMalloc(40);
            sprintf( psColDef->field_name, "FIELD_%d", iField+1 );
        }

        if( psColDef->field_alias != NULL )
        {
            oFDefn.SetName(psColDef->field_alias);
        }
        else if( psColDef->col_func != SWQCF_NONE )
        {
            const swq_operation *op = swq_op_registrar::GetOperator( 
                (swq_op) psColDef->col_func );

            oFDefn.SetName( CPLSPrintf( "%s_%s",
                                        op->osName.c_str(),
                                        psColDef->field_name ) );
        }
        else
            oFDefn.SetName( psColDef->field_name );

        if( psColDef->col_func == SWQCF_COUNT )
            oFDefn.SetType( OFTInteger );
        else if( poSrcFDefn != NULL )
        {
            oFDefn.SetType( poSrcFDefn->GetType() );
            if( psColDef->col_func != SWQCF_AVG )
            {
                oFDefn.SetWidth( poSrcFDefn->GetWidth() );
                oFDefn.SetPrecision( poSrcFDefn->GetPrecision() );
            }
        }
        else if ( psColDef->field_index >= iFIDFieldIndex )
        {
            switch ( SpecialFieldTypes[psColDef->field_index-iFIDFieldIndex] )
            {
              case SWQ_INTEGER:
                oFDefn.SetType( OFTInteger );
                break;
              case SWQ_FLOAT:
                oFDefn.SetType( OFTReal );
                break;
              default:
                oFDefn.SetType( OFTString );
                break;
            }
        }
        else
        {
            switch( psColDef->field_type )
            {
              case SWQ_INTEGER:
              case SWQ_BOOLEAN:
                oFDefn.SetType( OFTInteger );
                break;
                
              case SWQ_FLOAT:
                oFDefn.SetType( OFTReal );
                break;

              default:
                oFDefn.SetType( OFTString );
                break;
            }
        }

        /* setting up the target_type */
        switch (psColDef->target_type)
        {
          case SWQ_OTHER:
            break;
          case SWQ_INTEGER:
          case SWQ_BOOLEAN:
            oFDefn.SetType( OFTInteger );
            break;
          case SWQ_FLOAT:
            oFDefn.SetType( OFTReal );
            break;
          case SWQ_STRING:
            oFDefn.SetType( OFTString );
            break;
          case SWQ_TIMESTAMP:
            oFDefn.SetType( OFTDateTime );
            break;
          case SWQ_DATE:
            oFDefn.SetType( OFTDate );
            break;
          case SWQ_TIME:
            oFDefn.SetType( OFTTime );
            break;

          default:
            CPLAssert( FALSE );
            oFDefn.SetType( OFTString );
            break;
        }

        if (psColDef->field_length > 0)
        {
            oFDefn.SetWidth( psColDef->field_length );
        }

        if (psColDef->field_precision >= 0)
        {
            oFDefn.SetPrecision( psColDef->field_precision );
        }

        poDefn->AddFieldDefn( &oFDefn );
    }

    poDefn->SetGeomType( poSrcLayer->GetLayerDefn()->GetGeomType() );

    ResetReading();

    FindAndSetIgnoredFields();

    if( !bForwardWhereToSourceLayer )
        SetAttributeFilter( pszWHERE );
}

/************************************************************************/
/*                       ~OGRGenSQLResultsLayer()                       */
/************************************************************************/

OGRGenSQLResultsLayer::~OGRGenSQLResultsLayer()

{
    if( m_nFeaturesRead > 0 && poDefn != NULL )
    {
        CPLDebug( "GenSQL", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poDefn->GetName() );
    }

    ClearFilters();

/* -------------------------------------------------------------------- */
/*      Free various datastructures.                                    */
/* -------------------------------------------------------------------- */
    CPLFree( papoTableLayers );
    papoTableLayers = NULL;
             
    CPLFree( panFIDIndex );

    delete poSummaryFeature;
    delete (swq_select *) pSelectInfo;

    if( poDefn != NULL )
    {
        poDefn->Release();
    }

/* -------------------------------------------------------------------- */
/*      Release any additional datasources being used in joins.         */
/* -------------------------------------------------------------------- */
    OGRSFDriverRegistrar *poReg=OGRSFDriverRegistrar::GetRegistrar();

    for( int iEDS = 0; iEDS < nExtraDSCount; iEDS++ )
        poReg->ReleaseDataSource( papoExtraDS[iEDS] );

    CPLFree( papoExtraDS );
    CPLFree( pszWHERE );
}

/************************************************************************/
/*                            ClearFilters()                            */
/*                                                                      */
/*      Clear up all filters currently in place on the target layer,    */
/*      and joined layers.  We try not to leave them installed          */
/*      except when actively fetching features.                         */
/************************************************************************/

void OGRGenSQLResultsLayer::ClearFilters()

{
/* -------------------------------------------------------------------- */
/*      Clear any filters installed on the target layer.                */
/* -------------------------------------------------------------------- */
    if( poSrcLayer != NULL )
    {
        poSrcLayer->SetAttributeFilter( "" );
        poSrcLayer->SetSpatialFilter( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Clear any attribute filter installed on the joined layers.      */
/* -------------------------------------------------------------------- */
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    int iJoin;

    if( psSelectInfo != NULL )
    {
        for( iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
        {
            swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
            OGRLayer *poJoinLayer = 
                papoTableLayers[psJoinInfo->secondary_table];
            
            poJoinLayer->SetAttributeFilter( "" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Clear any ignored field lists installed on source layers        */
/* -------------------------------------------------------------------- */
    if( psSelectInfo != NULL )
    {
        for( int iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
        {
            OGRLayer* poLayer = papoTableLayers[iTable];
            poLayer->SetIgnoredFields(NULL);
        }
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::ResetReading() 

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( psSelectInfo->query_mode == SWQM_RECORDSET )
    {
        poSrcLayer->SetAttributeFilter( pszWHERE );
        poSrcLayer->SetSpatialFilter( m_poFilterGeom );
        
        poSrcLayer->ResetReading();
    }

    nNextIndexFID = 0;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily resposition       */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::SetNextByIndex( long nIndex )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    CreateOrderByIndex();

    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD 
        || psSelectInfo->query_mode == SWQM_DISTINCT_LIST 
        || panFIDIndex != NULL )
    {
        nNextIndexFID = nIndex;
        return OGRERR_NONE;
    }
    else
    {
        return poSrcLayer->SetNextByIndex( nIndex );
    }
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::GetExtent( OGREnvelope *psExtent, 
                                         int bForce )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( psSelectInfo->query_mode == SWQM_RECORDSET )
        return poSrcLayer->GetExtent( psExtent, bForce );
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGenSQLResultsLayer::GetSpatialRef() 

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( psSelectInfo->query_mode != SWQM_RECORDSET )
        return NULL;
    else
        return poSrcLayer->GetSpatialRef();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRGenSQLResultsLayer::GetFeatureCount( int bForce )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    CreateOrderByIndex();

    if( psSelectInfo->query_mode == SWQM_DISTINCT_LIST )
    {
        if( !PrepareSummary() )
            return 0;

        swq_summary *psSummary = psSelectInfo->column_summary + 0;

        if( psSummary == NULL )
            return 0;

        return psSummary->count;
    }
    else if( psSelectInfo->query_mode != SWQM_RECORDSET )
        return 1;
    else if( m_poAttrQuery == NULL )
        return poSrcLayer->GetFeatureCount( bForce );
    else
        return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGenSQLResultsLayer::TestCapability( const char *pszCap )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( EQUAL(pszCap,OLCFastSetNextByIndex) )
    {
        if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD 
            || psSelectInfo->query_mode == SWQM_DISTINCT_LIST 
            || panFIDIndex != NULL )
            return TRUE;
        else 
            return poSrcLayer->TestCapability( pszCap );
    }

    if( psSelectInfo->query_mode == SWQM_RECORDSET
        && (EQUAL(pszCap,OLCFastFeatureCount) 
            || EQUAL(pszCap,OLCRandomRead) 
            || EQUAL(pszCap,OLCFastGetExtent)) )
        return poSrcLayer->TestCapability( pszCap );

    else if( psSelectInfo->query_mode != SWQM_RECORDSET )
    {
        if( EQUAL(pszCap,OLCFastFeatureCount) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                        ContainGeomSpecialField()                     */
/************************************************************************/

int OGRGenSQLResultsLayer::ContainGeomSpecialField(swq_expr_node* expr)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if( expr->table_index != -1 && expr->field_index != -1 )
        {
            OGRLayer* poLayer = papoTableLayers[expr->table_index];
            int nSpecialFieldIdx = expr->field_index -
                            poLayer->GetLayerDefn()->GetFieldCount();
            return nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_AREA;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for( int i = 0; i < expr->nSubExprCount; i++ )
        {
            if (ContainGeomSpecialField(expr->papoSubExpr[i]))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                           PrepareSummary()                           */
/************************************************************************/

int OGRGenSQLResultsLayer::PrepareSummary()

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( poSummaryFeature != NULL )
        return TRUE;

    poSummaryFeature = new OGRFeature( poDefn );
    poSummaryFeature->SetFID( 0 );

/* -------------------------------------------------------------------- */
/*      Ensure our query parameters are in place on the source          */
/*      layer.  And initialize reading.                                 */
/* -------------------------------------------------------------------- */
    poSrcLayer->SetAttributeFilter(pszWHERE);
    poSrcLayer->SetSpatialFilter( m_poFilterGeom );
        
    poSrcLayer->ResetReading();

/* -------------------------------------------------------------------- */
/*      Ignore geometry reading if no spatial filter in place and that  */
/*      the where clause and no column references OGR_GEOMETRY,         */
/*      OGR_GEOM_WKT or OGR_GEOM_AREA special fields.                   */
/* -------------------------------------------------------------------- */
    int bSaveIsGeomIgnored = poSrcLayer->GetLayerDefn()->IsGeometryIgnored();
    if ( m_poFilterGeom == NULL && ( psSelectInfo->where_expr == NULL ||
                !ContainGeomSpecialField(psSelectInfo->where_expr) ) )
    {
        int bFoundGeomExpr = FALSE;
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            if (psColDef->table_index != -1 && psColDef->field_index != -1)
            {
                OGRLayer* poLayer = papoTableLayers[psColDef->table_index];
                int nSpecialFieldIdx = psColDef->field_index -
                                poLayer->GetLayerDefn()->GetFieldCount();
                if (nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                    nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                    nSpecialFieldIdx == SPF_OGR_GEOM_AREA)
                {
                    bFoundGeomExpr = TRUE;
                    break;
                }
            }
            if (psColDef->expr != NULL && ContainGeomSpecialField(psColDef->expr))
            {
                bFoundGeomExpr = TRUE;
                break;
            }
        }
        if (!bFoundGeomExpr)
            poSrcLayer->GetLayerDefn()->SetGeometryIgnored(TRUE);
    }

/* -------------------------------------------------------------------- */
/*      We treat COUNT(*) as a special case, and fill with              */
/*      GetFeatureCount().                                              */
/* -------------------------------------------------------------------- */

    if( psSelectInfo->result_columns == 1 
        && psSelectInfo->column_defs[0].col_func == SWQCF_COUNT
        && psSelectInfo->column_defs[0].field_index < 0 )
    {
        poSummaryFeature->SetField( 0, poSrcLayer->GetFeatureCount( TRUE ) );
        poSrcLayer->GetLayerDefn()->SetGeometryIgnored(bSaveIsGeomIgnored);
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, process all source feature through the summary       */
/*      building facilities of SWQ.                                     */
/* -------------------------------------------------------------------- */
    const char *pszError;
    OGRFeature *poSrcFeature;
    int iField;

    while( (poSrcFeature = poSrcLayer->GetNextFeature()) != NULL )
    {
        for( iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;

            if (psColDef->col_func == SWQCF_COUNT)
            {
                /* psColDef->field_index can be -1 in the case of a COUNT(*) */
                if (psColDef->field_index < 0)
                    pszError = swq_select_summarize( psSelectInfo, iField, "" );
                else if (poSrcFeature->IsFieldSet(psColDef->field_index))
                    pszError = swq_select_summarize( psSelectInfo, iField, poSrcFeature->GetFieldAsString(
                                                psColDef->field_index ) );
                else
                    pszError = NULL;
            }
            else
            {
                const char* pszVal = NULL;
                if (poSrcFeature->IsFieldSet(psColDef->field_index))
                    pszVal = poSrcFeature->GetFieldAsString(
                                                psColDef->field_index );
                pszError = swq_select_summarize( psSelectInfo, iField, pszVal );
            }
            
            if( pszError != NULL )
            {
                delete poSrcFeature;
                delete poSummaryFeature;
                poSummaryFeature = NULL;

                poSrcLayer->GetLayerDefn()->SetGeometryIgnored(bSaveIsGeomIgnored);

                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
                return FALSE;
            }
        }

        delete poSrcFeature;
    }

    poSrcLayer->GetLayerDefn()->SetGeometryIgnored(bSaveIsGeomIgnored);

    pszError = swq_select_finish_summarize( psSelectInfo );
    if( pszError != NULL )
    {
        delete poSummaryFeature;
        poSummaryFeature = NULL;
        
        CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we have run out of features on the source layer, clear       */
/*      away the filters we have installed till a next run through      */
/*      the features.                                                   */
/* -------------------------------------------------------------------- */
    if( poSrcFeature == NULL )
        ClearFilters();

/* -------------------------------------------------------------------- */
/*      Now apply the values to the summary feature.  If we are in      */
/*      DISTINCT_LIST mode we don't do this step.                       */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD )
    {
        for( iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            if (psSelectInfo->column_summary != NULL)
            {
                swq_summary *psSummary = psSelectInfo->column_summary + iField;

                if( psColDef->col_func == SWQCF_AVG )
                    poSummaryFeature->SetField( iField,
                                            psSummary->sum / psSummary->count );
                else if( psColDef->col_func == SWQCF_MIN )
                    poSummaryFeature->SetField( iField, psSummary->min );
                else if( psColDef->col_func == SWQCF_MAX )
                    poSummaryFeature->SetField( iField, psSummary->max );
                else if( psColDef->col_func == SWQCF_COUNT )
                    poSummaryFeature->SetField( iField, psSummary->count );
                else if( psColDef->col_func == SWQCF_SUM )
                    poSummaryFeature->SetField( iField, psSummary->sum );
            }
            else if ( psColDef->col_func == SWQCF_COUNT )
                poSummaryFeature->SetField( iField, 0 );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                       OGRMultiFeatureFetcher()                       */
/************************************************************************/

static swq_expr_node *OGRMultiFeatureFetcher( swq_expr_node *op, 
                                              void *pFeatureList )

{
    std::vector<OGRFeature*> *papoFeatures = 
        (std::vector<OGRFeature*> *) pFeatureList;
    OGRFeature *poFeature;
    swq_expr_node *poRetNode = NULL;

    CPLAssert( op->eNodeType == SNT_COLUMN );

/* -------------------------------------------------------------------- */
/*      What feature are we using?  The primary or one of the joined ones?*/
/* -------------------------------------------------------------------- */
    if( op->table_index < 0 || op->table_index >= (int)papoFeatures->size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Request for unexpected table_index (%d) in field fetcher.",
                  op->table_index );
        return NULL;
    }
    
    poFeature = (*papoFeatures)[op->table_index];

/* -------------------------------------------------------------------- */
/*      Fetch the value.                                                */
/* -------------------------------------------------------------------- */
    switch( op->field_type )
    {
      case SWQ_INTEGER:
      case SWQ_BOOLEAN:
        if( poFeature == NULL 
            || !poFeature->IsFieldSet(op->field_index) )
        {
            poRetNode = new swq_expr_node(0);
            poRetNode->is_null = TRUE;
        }
        else
            poRetNode = new swq_expr_node( 
                poFeature->GetFieldAsInteger(op->field_index) );
        break;

      case SWQ_FLOAT:
        if( poFeature == NULL 
            || !poFeature->IsFieldSet(op->field_index) )
        {
            poRetNode = new swq_expr_node( 0.0 );
            poRetNode->is_null = TRUE;
        }
        else
            poRetNode = new swq_expr_node( 
                poFeature->GetFieldAsDouble(op->field_index) );
        break;
        
      default:
        if( poFeature == NULL 
            || !poFeature->IsFieldSet(op->field_index) )
        {
            poRetNode = new swq_expr_node("");
            poRetNode->is_null = TRUE;
        }
        else
            poRetNode = new swq_expr_node( 
                poFeature->GetFieldAsString(op->field_index) );
        break;
    }

    return poRetNode;
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::TranslateFeature( OGRFeature *poSrcFeat )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    OGRFeature *poDstFeat;
    std::vector<OGRFeature*> apoFeatures;

    if( poSrcFeat == NULL )
        return NULL;

    m_nFeaturesRead++;

    apoFeatures.push_back( poSrcFeat );

/* -------------------------------------------------------------------- */
/*      Fetch the corresponding features from any jointed tables.       */
/* -------------------------------------------------------------------- */
    int iJoin;

    for( iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        CPLString osFilter;

        swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;

        /* OGRMultiFeatureFetcher assumes that the features are pushed in */
        /* apoFeatures with increasing secondary_table, so make sure */
        /* we have taken care of this */
        CPLAssert(psJoinInfo->secondary_table == iJoin + 1);

        OGRLayer *poJoinLayer = papoTableLayers[psJoinInfo->secondary_table];
        
        // if source key is null, we can't do join.
        if( !poSrcFeat->IsFieldSet( psJoinInfo->primary_field ) )
        {
            apoFeatures.push_back( NULL );
            continue;
        }
        
        OGRFieldDefn* poSecondaryFieldDefn =
            poJoinLayer->GetLayerDefn()->GetFieldDefn( 
                     psJoinInfo->secondary_field );
        OGRFieldType ePrimaryFieldType = poSrcLayer->GetLayerDefn()->
                    GetFieldDefn(psJoinInfo->primary_field )->GetType();
        OGRFieldType eSecondaryFieldType = poSecondaryFieldDefn->GetType();

        // Prepare attribute query to express fetching on the joined variable
        
        // If joining a (primary) numeric column with a (secondary) string column
        // then add implicit casting of the secondary column to numeric. This behaviour
        // worked in GDAL < 1.8, and it is consistant with how sqlite behaves too. See #4321
        // For the reverse case, joining a string column with a numeric column, the
        // string constant will be cast to float by SWQAutoConvertStringToNumeric (#4259)
        if( eSecondaryFieldType == OFTString &&
            (ePrimaryFieldType == OFTInteger || ePrimaryFieldType == OFTReal) )
            osFilter.Printf("CAST(%s AS FLOAT) = ", poSecondaryFieldDefn->GetNameRef() );
        else
            osFilter.Printf("%s = ", poSecondaryFieldDefn->GetNameRef() );

        OGRField *psSrcField = 
            poSrcFeat->GetRawFieldRef(psJoinInfo->primary_field);

        switch( ePrimaryFieldType )
        {
          case OFTInteger:
            osFilter += CPLString().Printf("%d", psSrcField->Integer );
            break;

          case OFTReal:
            osFilter += CPLString().Printf("%.16g", psSrcField->Real );
            break;

          case OFTString:
          {
              char *pszEscaped = CPLEscapeString( psSrcField->String, 
                                                  strlen(psSrcField->String),
                                                  CPLES_SQL );
              osFilter += "'";
              osFilter += pszEscaped;
              osFilter += "'";
              CPLFree( pszEscaped );
          }
          break;

          default:
            CPLAssert( FALSE );
            continue;
        }

        OGRFeature *poJoinFeature = NULL;

        poJoinLayer->ResetReading();
        if( poJoinLayer->SetAttributeFilter( osFilter.c_str() ) == OGRERR_NONE )
            poJoinFeature = poJoinLayer->GetNextFeature();

        apoFeatures.push_back( poJoinFeature );
    }

/* -------------------------------------------------------------------- */
/*      Create destination feature.                                     */
/* -------------------------------------------------------------------- */
    poDstFeat = new OGRFeature( poDefn );

    poDstFeat->SetFID( poSrcFeat->GetFID() );

    poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );

    poDstFeat->SetStyleString( poSrcFeat->GetStyleString() );

/* -------------------------------------------------------------------- */
/*      Evaluate fields that are complex expressions.                   */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        swq_expr_node *poResult;

        if( psColDef->field_index != -1 )
            continue;

        poResult = psColDef->expr->Evaluate( OGRMultiFeatureFetcher, 
                                             (void *) &apoFeatures );
        
        if( poResult == NULL )
        {
            delete poDstFeat;
            return NULL;
        }

        if( poResult->is_null )
        {
            delete poResult;
            continue;
        }

        switch( poResult->field_type )
        {
          case SWQ_INTEGER:
            poDstFeat->SetField( iField, poResult->int_value );
            break;
            
          case SWQ_FLOAT:
            poDstFeat->SetField( iField, poResult->float_value );
            break;
            
          default:
            poDstFeat->SetField( iField, poResult->string_value );
            break;
        }

        delete poResult;
    }
    
/* -------------------------------------------------------------------- */
/*      Copy fields from primary record to the destination feature.     */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;

        if( psColDef->table_index != 0 )
            continue;

        if ( psColDef->field_index >= iFIDFieldIndex &&
            psColDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
        {
            switch (SpecialFieldTypes[psColDef->field_index - iFIDFieldIndex])
            {
              case SWQ_INTEGER:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsInteger(psColDef->field_index) );
                break;
              case SWQ_FLOAT:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsDouble(psColDef->field_index) );
                break;
              default:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsString(psColDef->field_index) );
            }
        }
        else
        {
            switch (psColDef->target_type)
            {
              case SWQ_INTEGER:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsInteger(psColDef->field_index) );
                break;

              case SWQ_FLOAT:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsDouble(psColDef->field_index) );
                break;
              
              case SWQ_STRING:
              case SWQ_TIMESTAMP:
              case SWQ_DATE:
              case SWQ_TIME:
                poDstFeat->SetField( iField, poSrcFeat->GetFieldAsString(psColDef->field_index) );
                break;

              default:
                poDstFeat->SetField( iField,
                         poSrcFeat->GetRawFieldRef( psColDef->field_index ) );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy values from any joined tables.                             */
/* -------------------------------------------------------------------- */
    for( iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        CPLString osFilter;

        swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
        OGRFeature *poJoinFeature = apoFeatures[iJoin+1];

        if( poJoinFeature == NULL )
            continue;

        // Copy over selected field values. 
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            
            if( psColDef->table_index == psJoinInfo->secondary_table )
                poDstFeat->SetField( iField,
                                     poJoinFeature->GetRawFieldRef( 
                                         psColDef->field_index ) );
        }

        delete poJoinFeature;
    }

    return poDstFeat;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::GetNextFeature()

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    CreateOrderByIndex();

/* -------------------------------------------------------------------- */
/*      Handle summary sets.                                            */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD 
        || psSelectInfo->query_mode == SWQM_DISTINCT_LIST )
        return GetFeature( nNextIndexFID++ );

/* -------------------------------------------------------------------- */
/*      Handle ordered sets.                                            */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        OGRFeature *poFeature;

        if( panFIDIndex != NULL )
            poFeature =  GetFeature( nNextIndexFID++ );
        else
        {
            OGRFeature *poSrcFeat = poSrcLayer->GetNextFeature();

            if( poSrcFeat == NULL )
                return NULL;
            
            poFeature = TranslateFeature( poSrcFeat );
            delete poSrcFeat;
        }

        if( poFeature == NULL )
            return NULL;

        if( m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature ) )
            return poFeature;

        delete poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::GetFeature( long nFID )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    CreateOrderByIndex();

/* -------------------------------------------------------------------- */
/*      Handle request for summary record.                              */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD )
    {
        if( !PrepareSummary() || nFID != 0 || poSummaryFeature == NULL )
            return NULL;
        else
            return poSummaryFeature->Clone();
    }

/* -------------------------------------------------------------------- */
/*      Handle request for distinct list record.                        */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_DISTINCT_LIST )
    {
        if( !PrepareSummary() )
            return NULL;

        swq_summary *psSummary = psSelectInfo->column_summary + 0;

        if( psSummary == NULL )
            return NULL;

        if( nFID < 0 || nFID >= psSummary->count )
            return NULL;

        if( psSummary->distinct_list[nFID] != NULL )
            poSummaryFeature->SetField( 0, psSummary->distinct_list[nFID] );
        else
            poSummaryFeature->UnsetField( 0 );
        poSummaryFeature->SetFID( nFID );

        return poSummaryFeature->Clone();
    }

/* -------------------------------------------------------------------- */
/*      Are we running in sorted mode?  If so, run the fid through      */
/*      the index.                                                      */
/* -------------------------------------------------------------------- */
    if( panFIDIndex != NULL )
    {
        if( nFID < 0 || nFID >= nIndexSize )
            return NULL;
        else
            nFID = panFIDIndex[nFID];
    }

/* -------------------------------------------------------------------- */
/*      Handle request for random record.                               */
/* -------------------------------------------------------------------- */
    OGRFeature *poSrcFeature = poSrcLayer->GetFeature( nFID );
    OGRFeature *poResult;

    if( poSrcFeature == NULL )
        return NULL;

    poResult = TranslateFeature( poSrcFeature );
    poResult->SetFID( nFID );
    
    delete poSrcFeature;

    return poResult;
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

OGRGeometry *OGRGenSQLResultsLayer::GetSpatialFilter() 

{
    return NULL;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRGenSQLResultsLayer::GetLayerDefn()

{
    return poDefn;
}

/************************************************************************/
/*                         CreateOrderByIndex()                         */
/*                                                                      */
/*      This method is responsible for creating an index providing      */
/*      ordered access to the features according to the supplied        */
/*      ORDER BY clauses.                                               */
/*                                                                      */
/*      This is accomplished by making one pass through all the         */
/*      eligible source features, and capturing the order by fields     */
/*      of all records in memory.  A quick sort is then applied to      */
/*      this in memory copy of the order-by fields to create the        */
/*      required index.                                                 */
/*                                                                      */
/*      Keeping all the key values in memory will *not* scale up to     */
/*      very large input datasets.                                      */
/************************************************************************/

void OGRGenSQLResultsLayer::CreateOrderByIndex()

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    OGRField *pasIndexFields;
    int      i, nOrderItems = psSelectInfo->order_specs;
    long     *panFIDList;

    if( ! (psSelectInfo->order_specs > 0
           && psSelectInfo->query_mode == SWQM_RECORDSET
           && nOrderItems != 0 ) )
        return;

    if( bOrderByValid )
        return;

    bOrderByValid = TRUE;

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Allocate set of key values, and the output index.               */
/* -------------------------------------------------------------------- */
    int nFeaturesAlloc = 100;

    panFIDIndex = NULL;
    pasIndexFields = (OGRField *) 
        CPLCalloc(sizeof(OGRField), nOrderItems * nFeaturesAlloc);
    panFIDList = (long *) CPLMalloc(sizeof(long) * nFeaturesAlloc);

/* -------------------------------------------------------------------- */
/*      Read in all the key values.                                     */
/* -------------------------------------------------------------------- */
    OGRFeature *poSrcFeat;
    nIndexSize = 0;

    while( (poSrcFeat = poSrcLayer->GetNextFeature()) != NULL )
    {
        int iKey;

        if (nIndexSize == nFeaturesAlloc)
        {
            int nNewFeaturesAlloc = (nFeaturesAlloc * 4) / 3;
            OGRField* pasNewIndexFields = (OGRField *)
                VSIRealloc(pasIndexFields,
                           sizeof(OGRField) * nOrderItems * nNewFeaturesAlloc);
            if (pasNewIndexFields == NULL)
            {
                VSIFree(pasIndexFields);
                VSIFree(panFIDList);
                nIndexSize = 0;
                return;
            }
            pasIndexFields = pasNewIndexFields;

            long* panNewFIDList = (long *)
                VSIRealloc(panFIDList, sizeof(long) *  nNewFeaturesAlloc);
            if (panNewFIDList == NULL)
            {
                VSIFree(pasIndexFields);
                VSIFree(panFIDList);
                nIndexSize = 0;
                return;
            }
            panFIDList = panNewFIDList;

            memset(pasIndexFields + nFeaturesAlloc, 0,
                   sizeof(OGRField) * nOrderItems * (nNewFeaturesAlloc - nFeaturesAlloc));

            nFeaturesAlloc = nNewFeaturesAlloc;
        }

        for( iKey = 0; iKey < nOrderItems; iKey++ )
        {
            swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
            OGRFieldDefn *poFDefn;
            OGRField *psSrcField, *psDstField;

            psDstField = pasIndexFields + nIndexSize * nOrderItems + iKey;

            if ( psKeyDef->field_index >= iFIDFieldIndex)
            {
                if ( psKeyDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
                {
                    switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
                    {
                      case SWQ_INTEGER:
                        psDstField->Integer = poSrcFeat->GetFieldAsInteger(psKeyDef->field_index);
                        break;

                      case SWQ_FLOAT:
                        psDstField->Real = poSrcFeat->GetFieldAsDouble(psKeyDef->field_index);
                        break;

                      default:
                        psDstField->String = CPLStrdup( poSrcFeat->GetFieldAsString(psKeyDef->field_index) );
                        break;
                    }
                }
                continue;
            }
            
            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                psKeyDef->field_index );

            psSrcField = poSrcFeat->GetRawFieldRef( psKeyDef->field_index );

            if( poFDefn->GetType() == OFTInteger 
                || poFDefn->GetType() == OFTReal
                || poFDefn->GetType() == OFTDate
                || poFDefn->GetType() == OFTTime
                || poFDefn->GetType() == OFTDateTime)
                memcpy( psDstField, psSrcField, sizeof(OGRField) );
            else if( poFDefn->GetType() == OFTString )
            {
                if( poSrcFeat->IsFieldSet( psKeyDef->field_index ) )
                    psDstField->String = CPLStrdup( psSrcField->String );
                else
                    memcpy( psDstField, psSrcField, sizeof(OGRField) );
            }
        }

        panFIDList[nIndexSize] = poSrcFeat->GetFID();
        delete poSrcFeat;

        nIndexSize++;
    }

    //CPLDebug("GenSQL", "CreateOrderByIndex() = %d features", nIndexSize);

/* -------------------------------------------------------------------- */
/*      Initialize panFIDIndex                                          */
/* -------------------------------------------------------------------- */
    panFIDIndex = (long *) CPLMalloc(sizeof(long) * nIndexSize);
    for( i = 0; i < nIndexSize; i++ )
        panFIDIndex[i] = i;

/* -------------------------------------------------------------------- */
/*      Quick sort the records.                                         */
/* -------------------------------------------------------------------- */
    SortIndexSection( pasIndexFields, 0, nIndexSize );

/* -------------------------------------------------------------------- */
/*      Rework the FID map to map to real FIDs.                         */
/* -------------------------------------------------------------------- */
    int bAlreadySorted = TRUE;
    for( i = 0; i < nIndexSize; i++ )
    {
        if (panFIDIndex[i] != i)
            bAlreadySorted = FALSE;
        panFIDIndex[i] = panFIDList[panFIDIndex[i]];
    }

    CPLFree( panFIDList );

/* -------------------------------------------------------------------- */
/*      Free the key field values.                                      */
/* -------------------------------------------------------------------- */
    for( int iKey = 0; iKey < nOrderItems; iKey++ )
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRFieldDefn *poFDefn;

        if ( psKeyDef->field_index >= iFIDFieldIndex &&
            psKeyDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
        {
            /* warning: only special fields of type string should be deallocated */
            if (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex] == SWQ_STRING)
            {
                for( i = 0; i < nIndexSize; i++ )
                {
                    OGRField *psField = pasIndexFields + iKey + i * nOrderItems;
                    CPLFree( psField->String );
                }
            }
            continue;
        }

        poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
            psKeyDef->field_index );

        if( poFDefn->GetType() == OFTString )
        {
            for( i = 0; i < nIndexSize; i++ )
            {
                OGRField *psField = pasIndexFields + iKey + i * nOrderItems;
                
                if( psField->Set.nMarker1 != OGRUnsetMarker 
                    || psField->Set.nMarker2 != OGRUnsetMarker )
                    CPLFree( psField->String );
            }
        }
    }

    CPLFree( pasIndexFields );

    /* If it is already sorted, then free than panFIDIndex array */
    /* so that GetNextFeature() can call a sequential GetNextFeature() */
    /* on the source array. Very usefull for layers where random access */
    /* is slow. */
    /* Use case: the GML result of a WFS GetFeature with a SORTBY */
    if (bAlreadySorted)
    {
        CPLFree( panFIDIndex );
        panFIDIndex = NULL;

        nIndexSize = 0;
    }

    ResetReading();
}

/************************************************************************/
/*                          SortIndexSection()                          */
/*                                                                      */
/*      Sort the records in a section of the index.                     */
/************************************************************************/

void OGRGenSQLResultsLayer::SortIndexSection( OGRField *pasIndexFields, 
                                              int nStart, int nEntries )

{
    if( nEntries < 2 )
        return;

    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    int      nOrderItems = psSelectInfo->order_specs;

    int nFirstGroup = nEntries / 2;
    int nFirstStart = nStart;
    int nSecondGroup = nEntries - nFirstGroup;
    int nSecondStart = nStart + nFirstGroup;
    int iMerge = 0;
    long *panMerged;

    SortIndexSection( pasIndexFields, nFirstStart, nFirstGroup );
    SortIndexSection( pasIndexFields, nSecondStart, nSecondGroup );

    panMerged = (long *) CPLMalloc( sizeof(long) * nEntries );
        
    while( iMerge < nEntries )
    {
        int  nResult;

        if( nFirstGroup == 0 )
            nResult = -1;
        else if( nSecondGroup == 0 )
            nResult = 1;
        else
            nResult = Compare( pasIndexFields 
                               + panFIDIndex[nFirstStart] * nOrderItems, 
                               pasIndexFields 
                               + panFIDIndex[nSecondStart] * nOrderItems );

        if( nResult < 0 )
        {
            panMerged[iMerge++] = panFIDIndex[nSecondStart++];
            nSecondGroup--;
        }
        else
        {
            panMerged[iMerge++] = panFIDIndex[nFirstStart++];
            nFirstGroup--;
        }
    }

    /* Copy the merge list back into the main index */

    memcpy( panFIDIndex + nStart, panMerged, sizeof(long) * nEntries );
    CPLFree( panMerged );
}

/************************************************************************/
/*                              Compare()                               */
/************************************************************************/

int OGRGenSQLResultsLayer::Compare( OGRField *pasFirstTuple,
                                    OGRField *pasSecondTuple )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    int  nResult = 0, iKey;

    for( iKey = 0; nResult == 0 && iKey < psSelectInfo->order_specs; iKey++ )
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRFieldDefn *poFDefn;

        if( psKeyDef->field_index >= iFIDFieldIndex )
            poFDefn = NULL;
        else
            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                psKeyDef->field_index );
        
        if( (pasFirstTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
             && pasFirstTuple[iKey].Set.nMarker2 == OGRUnsetMarker)
            || (pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
                && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker) )
            nResult = 0;
        else if ( poFDefn == NULL )
        {
            switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
            {
              case SWQ_INTEGER:
                if( pasFirstTuple[iKey].Integer < pasSecondTuple[iKey].Integer )
                    nResult = -1;
                else if( pasFirstTuple[iKey].Integer > pasSecondTuple[iKey].Integer )
                    nResult = 1;
                break;
              case SWQ_FLOAT:
                if( pasFirstTuple[iKey].Real < pasSecondTuple[iKey].Real )
                    nResult = -1;
                else if( pasFirstTuple[iKey].Real > pasSecondTuple[iKey].Real )
                    nResult = 1;
                break;
              case SWQ_STRING:
                nResult = strcmp(pasFirstTuple[iKey].String,
                                 pasSecondTuple[iKey].String);
                break;

              default:
                CPLAssert( FALSE );
                nResult = 0;
            }
        }
        else if( poFDefn->GetType() == OFTInteger )
        {
            if( pasFirstTuple[iKey].Integer < pasSecondTuple[iKey].Integer )
                nResult = -1;
            else if( pasFirstTuple[iKey].Integer 
                     > pasSecondTuple[iKey].Integer )
                nResult = 1;
        }
        else if( poFDefn->GetType() == OFTString )
            nResult = strcmp(pasFirstTuple[iKey].String,
                             pasSecondTuple[iKey].String);
        else if( poFDefn->GetType() == OFTReal )
        {
            if( pasFirstTuple[iKey].Real < pasSecondTuple[iKey].Real )
                nResult = -1;
            else if( pasFirstTuple[iKey].Real > pasSecondTuple[iKey].Real )
                nResult = 1;
        }
        else if( poFDefn->GetType() == OFTDate ||
                 poFDefn->GetType() == OFTTime ||
                 poFDefn->GetType() == OFTDateTime)
        {
            nResult = OGRCompareDate(&pasFirstTuple[iKey],
                                     &pasSecondTuple[iKey]);
        }

        if( psKeyDef->ascending_flag )
            nResult *= -1;
    }

    return nResult;
}


/************************************************************************/
/*                         AddFieldDefnToSet()                          */
/************************************************************************/

void OGRGenSQLResultsLayer::AddFieldDefnToSet(int iTable, int iColumn,
                                              CPLHashSet* hSet)
{
    if (iTable != -1 && iColumn != -1)
    {
        OGRLayer* poLayer = papoTableLayers[iTable];
        if (iColumn < poLayer->GetLayerDefn()->GetFieldCount())
        {
            OGRFieldDefn* poFDefn =
                poLayer->GetLayerDefn()->GetFieldDefn(iColumn);
            CPLHashSetInsert(hSet, poFDefn);
        }
    }
}

/************************************************************************/
/*                   ExploreExprForIgnoredFields()                      */
/************************************************************************/

void OGRGenSQLResultsLayer::ExploreExprForIgnoredFields(swq_expr_node* expr,
                                                        CPLHashSet* hSet)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        AddFieldDefnToSet(expr->table_index, expr->field_index, hSet);
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for( int i = 0; i < expr->nSubExprCount; i++ )
            ExploreExprForIgnoredFields(expr->papoSubExpr[i], hSet);
    }
}

/************************************************************************/
/*                     FindAndSetIgnoredFields()                        */
/************************************************************************/

void OGRGenSQLResultsLayer::FindAndSetIgnoredFields()
{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    CPLHashSet* hSet = CPLHashSetNew(CPLHashSetHashPointer,
                                     CPLHashSetEqualPointer,
                                     NULL);

/* -------------------------------------------------------------------- */
/*      1st phase : explore the whole select infos to determine which   */
/*      source fields are used                                          */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        AddFieldDefnToSet(psColDef->table_index, psColDef->field_index, hSet);
        if (psColDef->expr)
            ExploreExprForIgnoredFields(psColDef->expr, hSet);
    }

    if (psSelectInfo->where_expr)
        ExploreExprForIgnoredFields(psSelectInfo->where_expr, hSet);

    for( int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        swq_join_def *psJoinDef = psSelectInfo->join_defs + iJoin;
        AddFieldDefnToSet(0, psJoinDef->primary_field, hSet);
        AddFieldDefnToSet(psJoinDef->secondary_table, psJoinDef->secondary_field, hSet);
    }

    for( int iOrder = 0; iOrder < psSelectInfo->order_specs; iOrder++ )
    {
        swq_order_def *psOrderDef = psSelectInfo->order_defs + iOrder;
        AddFieldDefnToSet(psOrderDef->table_index, psOrderDef->field_index, hSet);
    }

/* -------------------------------------------------------------------- */
/*      2nd phase : now, we can exclude the unused fields               */
/* -------------------------------------------------------------------- */
    for( int iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        OGRLayer* poLayer = papoTableLayers[iTable];
        OGRFeatureDefn *poSrcFDefn = poLayer->GetLayerDefn();
        int iSrcField;
        char** papszIgnoredFields = NULL;
        for(iSrcField=0;iSrcField<poSrcFDefn->GetFieldCount();iSrcField++)
        {
            OGRFieldDefn* poFDefn = poSrcFDefn->GetFieldDefn(iSrcField);
            if (CPLHashSetLookup(hSet,poFDefn) == NULL)
            {
                papszIgnoredFields = CSLAddString(papszIgnoredFields, poFDefn->GetNameRef());
                //CPLDebug("OGR", "Adding %s to the list of ignored fields of layer %s",
                //         poFDefn->GetNameRef(), poLayer->GetName());
            }
        }
        poLayer->SetIgnoredFields((const char**)papszIgnoredFields);
        CSLDestroy(papszIgnoredFields);
    }

    CPLHashSetDestroy(hSet);
}

/************************************************************************/
/*                       InvalidateOrderByIndex()                       */
/************************************************************************/

void OGRGenSQLResultsLayer::InvalidateOrderByIndex()
{
    CPLFree( panFIDIndex );
    panFIDIndex = NULL;

    nIndexSize = 0;
    bOrderByValid = FALSE;
}

/************************************************************************/
/*                       SetAttributeFilter()                           */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::SetAttributeFilter( const char* pszAttributeFilter )
{
    InvalidateOrderByIndex();
    return OGRLayer::SetAttributeFilter(pszAttributeFilter);
}

/************************************************************************/
/*                       SetSpatialFilter()                             */
/************************************************************************/

void OGRGenSQLResultsLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    InvalidateOrderByIndex();
    OGRLayer::SetSpatialFilter(poGeom);
}
