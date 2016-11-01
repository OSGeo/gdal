/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGenSQLResultsLayer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#include "swq.h"
#include "ogr_p.h"
#include "ogr_gensql.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "cpl_time.h"
#include <vector>

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$");

class OGRGenSQLGeomFieldDefn: public OGRGeomFieldDefn
{
    public:
        OGRGenSQLGeomFieldDefn(OGRGeomFieldDefn* poGeomFieldDefn) :
            OGRGeomFieldDefn(poGeomFieldDefn->GetNameRef(),
                             poGeomFieldDefn->GetType()), bForceGeomType(FALSE)
        {
            SetSpatialRef(poGeomFieldDefn->GetSpatialRef());
        }

        int bForceGeomType;
};

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
            return expr->field_index >= nMinIndexForSpecialField &&
                   expr->field_index < nMinIndexForSpecialField + SPECIAL_FIELD_COUNT;
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

OGRGenSQLResultsLayer::OGRGenSQLResultsLayer( GDALDataset *poSrcDSIn,
                                              void *pSelectInfoIn,
                                              OGRGeometry *poSpatFilter,
                                              const char *pszWHEREIn,
                                              const char *pszDialect ) :
    poSrcDS(poSrcDSIn),
    poSrcLayer(NULL),
    pSelectInfo(pSelectInfoIn),
    pszWHERE(NULL),
    papoTableLayers(NULL),
    poDefn(NULL),
    panGeomFieldToSrcGeomField(NULL),
    nIndexSize(0),
    panFIDIndex(NULL),
    bOrderByValid(FALSE),
    nNextIndexFID(0),
    poSummaryFeature(NULL),
    iFIDFieldIndex(),
    nExtraDSCount(0),
    papoExtraDS(NULL)
{
    swq_select *psSelectInfo = (swq_select *) pSelectInfoIn;

/* -------------------------------------------------------------------- */
/*      Identify all the layers involved in the SELECT.                 */
/* -------------------------------------------------------------------- */
    papoTableLayers = (OGRLayer **)
        CPLCalloc( sizeof(OGRLayer *), psSelectInfo->table_count );

    for( int iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        GDALDataset *poTableDS = poSrcDS;

        if( psTableDef->data_source != NULL )
        {
            poTableDS = (GDALDataset*) GDALOpenEx( psTableDef->data_source,
                            GDAL_OF_VECTOR | GDAL_OF_SHARED, NULL, NULL, NULL );
            if( poTableDS == NULL )
            {
                if( strlen(CPLGetLastErrorMsg()) == 0 )
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Unable to open secondary datasource\n"
                              "`%s' required by JOIN.",
                              psTableDef->data_source );
                return;
            }

            papoExtraDS = (GDALDataset **)
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
    SetMetadata( poSrcLayer->GetMetadata( "NATIVE_DATA" ), "NATIVE_DATA" );

/* -------------------------------------------------------------------- */
/*      If the user has explicitly requested a OGRSQL dialect, then    */
/*      we should avoid to forward the where clause to the source layer */
/*      when there is a risk it cannot understand it (#4022)            */
/* -------------------------------------------------------------------- */
    int bForwardWhereToSourceLayer = TRUE;
    if( pszWHEREIn )
    {
        if( psSelectInfo->where_expr && pszDialect != NULL &&
            EQUAL(pszDialect, "OGRSQL") )
        {
            int nMinIndexForSpecialField = poSrcLayer->GetLayerDefn()->GetFieldCount();
            bForwardWhereToSourceLayer = !OGRGenSQLResultsLayerHasSpecialField
                            (psSelectInfo->where_expr, nMinIndexForSpecialField);
        }
        if (bForwardWhereToSourceLayer)
            pszWHERE = CPLStrdup(pszWHEREIn);
        else
            pszWHERE = NULL;
    }
    else
        pszWHERE = NULL;

/* -------------------------------------------------------------------- */
/*      Prepare a feature definition based on the query.                */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();

    poDefn = new OGRFeatureDefn( psSelectInfo->table_defs[0].table_alias );
    SetDescription( poDefn->GetName() );
    poDefn->SetGeomType(wkbNone);
    poDefn->Reference();

    iFIDFieldIndex = poSrcDefn->GetFieldCount();

    /* + 1 since we can add an implicit geometry field */
    panGeomFieldToSrcGeomField = static_cast<int *>(
        CPLMalloc(sizeof(int) * (1 + psSelectInfo->result_columns)));

    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        OGRFieldDefn oFDefn( "", OFTInteger );
        OGRGeomFieldDefn oGFDefn( "", wkbUnknown );
        OGRFieldDefn *poSrcFDefn = NULL;
        OGRGeomFieldDefn *poSrcGFDefn = NULL;
        int bIsGeometry = FALSE;
        OGRFeatureDefn *poLayerDefn = NULL;
        int iSrcGeomField = -1;

        if( psColDef->table_index != -1 )
            poLayerDefn =
                papoTableLayers[psColDef->table_index]->GetLayerDefn();

        if( psColDef->field_index > -1
            && poLayerDefn != NULL
            && psColDef->field_index < poLayerDefn->GetFieldCount() )
        {
            poSrcFDefn = poLayerDefn->GetFieldDefn(psColDef->field_index);
        }

        if( poLayerDefn != NULL &&
            IS_GEOM_FIELD_INDEX(poLayerDefn, psColDef->field_index) )
        {
            bIsGeometry = TRUE;
            iSrcGeomField =
                ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(poLayerDefn, psColDef->field_index);
            poSrcGFDefn = poLayerDefn->GetGeomFieldDefn(iSrcGeomField);
        }

        if( psColDef->target_type == SWQ_GEOMETRY )
            bIsGeometry = TRUE;

        if( psColDef->col_func == SWQCF_COUNT )
            bIsGeometry = FALSE;

        if( strlen(psColDef->field_name) == 0 && !bIsGeometry )
        {
            CPLFree( psColDef->field_name );
            psColDef->field_name = static_cast<char *>(CPLMalloc(40));
            snprintf( psColDef->field_name, 40, "FIELD_%d", poDefn->GetFieldCount()+1 );
        }

        if( psColDef->field_alias != NULL )
        {
            if( bIsGeometry )
                oGFDefn.SetName(psColDef->field_alias);
            else
                oFDefn.SetName(psColDef->field_alias);
        }
        else if( psColDef->col_func != SWQCF_NONE )
        {
            const swq_operation *op = swq_op_registrar::GetOperator(
                (swq_op) psColDef->col_func );

            oFDefn.SetName( CPLSPrintf( "%s_%s",
                                        op->pszName,
                                        psColDef->field_name ) );
        }
        else
        {
            CPLString osName;
            if( psColDef->table_name[0] )
            {
                osName = psColDef->table_name;
                osName += ".";
            }
            osName += psColDef->field_name;

            if( bIsGeometry )
                oGFDefn.SetName(osName);
            else
                oFDefn.SetName(osName);
        }

        if( psColDef->col_func == SWQCF_COUNT )
            oFDefn.SetType( OFTInteger64 );
        else if( poSrcFDefn != NULL )
        {
            if( psColDef->col_func != SWQCF_AVG ||
                psColDef->field_type == SWQ_DATE ||
                psColDef->field_type == SWQ_TIME ||
                psColDef->field_type == SWQ_TIMESTAMP )
            {
                oFDefn.SetType( poSrcFDefn->GetType() );
                if( psColDef->col_func == SWQCF_NONE ||
                    psColDef->col_func == SWQCF_MIN ||
                    psColDef->col_func == SWQCF_MAX )
                {
                    oFDefn.SetSubType( poSrcFDefn->GetSubType() );
                }
            }
            else
                oFDefn.SetType( OFTReal );
            if( psColDef->col_func != SWQCF_AVG &&
                psColDef->col_func != SWQCF_SUM )
            {
                oFDefn.SetWidth( poSrcFDefn->GetWidth() );
                oFDefn.SetPrecision( poSrcFDefn->GetPrecision() );
            }
        }
        else if( poSrcGFDefn != NULL )
        {
            oGFDefn.SetType( poSrcGFDefn->GetType() );
            oGFDefn.SetSpatialRef( poSrcGFDefn->GetSpatialRef() );
        }
        else if ( psColDef->field_index >= iFIDFieldIndex )
        {
            switch ( SpecialFieldTypes[psColDef->field_index-iFIDFieldIndex] )
            {
              case SWQ_INTEGER:
                oFDefn.SetType( OFTInteger );
                break;
              case SWQ_INTEGER64:
                oFDefn.SetType( OFTInteger64 );
                break;
              case SWQ_FLOAT:
                oFDefn.SetType( OFTReal );
                break;
              default:
                oFDefn.SetType( OFTString );
                break;
            }
            if( psColDef->field_index-iFIDFieldIndex == SPF_FID &&
                poSrcLayer->GetMetadataItem(OLMD_FID64) != NULL &&
                EQUAL(poSrcLayer->GetMetadataItem(OLMD_FID64), "YES") )
            {
                oFDefn.SetType( OFTInteger64 );
            }
        }
        else
        {
            switch( psColDef->field_type )
            {
              case SWQ_INTEGER:
                oFDefn.SetType( OFTInteger );
                break;

              case SWQ_INTEGER64:
                oFDefn.SetType( OFTInteger64 );
                break;

              case SWQ_BOOLEAN:
                oFDefn.SetType( OFTInteger );
                oFDefn.SetSubType( OFSTBoolean );
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
            oFDefn.SetType( OFTInteger );
            break;
          case SWQ_INTEGER64:
            oFDefn.SetType( OFTInteger64 );
            break;
          case SWQ_BOOLEAN:
            oFDefn.SetType( OFTInteger );
            oFDefn.SetSubType( OFSTBoolean );
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
          case SWQ_GEOMETRY:
            break;

          default:
            CPLAssert( false );
            oFDefn.SetType( OFTString );
            break;
        }
        if( psColDef->target_subtype != OFSTNone )
            oFDefn.SetSubType( psColDef->target_subtype );

        if (psColDef->field_length > 0)
        {
            oFDefn.SetWidth( psColDef->field_length );
        }

        if (psColDef->field_precision >= 0)
        {
            oFDefn.SetPrecision( psColDef->field_precision );
        }

        if( bIsGeometry )
        {
            panGeomFieldToSrcGeomField[poDefn->GetGeomFieldCount()] = iSrcGeomField;
            /* Hack while drivers haven't been updated so that */
            /* poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() == poSrcLayer->GetSpatialRef() */
            if( iSrcGeomField == 0 &&
                poSrcDefn->GetGeomFieldCount() == 1 &&
                oGFDefn.GetSpatialRef() == NULL )
            {
                oGFDefn.SetSpatialRef(poSrcLayer->GetSpatialRef());
            }
            int bForceGeomType = FALSE;
            if( psColDef->eGeomType != wkbUnknown )
            {
                oGFDefn.SetType( psColDef->eGeomType );
                bForceGeomType = TRUE;
            }
            if( psColDef->nSRID > 0 )
            {
                OGRSpatialReference* poSRS = new OGRSpatialReference();
                if( poSRS->importFromEPSG( psColDef->nSRID ) == OGRERR_NONE )
                {
                    oGFDefn.SetSpatialRef( poSRS );
                }
                poSRS->Release();
            }

            OGRGenSQLGeomFieldDefn* poMyGeomFieldDefn =
                new OGRGenSQLGeomFieldDefn(&oGFDefn);
            poMyGeomFieldDefn->bForceGeomType = bForceGeomType;
            poDefn->AddGeomFieldDefn( poMyGeomFieldDefn, FALSE );
        }
        else
            poDefn->AddFieldDefn( &oFDefn );
    }

/* -------------------------------------------------------------------- */
/*      Add implicit geometry field.                                    */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_RECORDSET &&
        poDefn->GetGeomFieldCount() == 0 &&
        poSrcDefn->GetGeomFieldCount() == 1 )
    {
        psSelectInfo->result_columns++;

        psSelectInfo->column_defs = (swq_col_def *)
            CPLRealloc( psSelectInfo->column_defs, sizeof(swq_col_def) * psSelectInfo->result_columns );

        swq_col_def *col_def = psSelectInfo->column_defs + psSelectInfo->result_columns - 1;

        memset( col_def, 0, sizeof(swq_col_def) );
        const char* pszName = poSrcDefn->GetGeomFieldDefn(0)->GetNameRef();
        if( *pszName != '\0' )
            col_def->field_name = CPLStrdup( pszName );
        else
            col_def->field_name = CPLStrdup( OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME );
        col_def->field_alias = NULL;
        col_def->table_index = 0;
        col_def->field_index = GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poSrcDefn, 0);
        col_def->field_type = SWQ_GEOMETRY;
        col_def->target_type = SWQ_GEOMETRY;

        panGeomFieldToSrcGeomField[poDefn->GetGeomFieldCount()] = 0;

        OGRGenSQLGeomFieldDefn* poMyGeomFieldDefn =
            new OGRGenSQLGeomFieldDefn(poSrcDefn->GetGeomFieldDefn(0));
        poDefn->AddGeomFieldDefn( poMyGeomFieldDefn, FALSE );

        /* Hack while drivers haven't been updated so that */
        /* poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() == poSrcLayer->GetSpatialRef() */
        if( poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() == NULL )
        {
            poDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSrcLayer->GetSpatialRef());
        }
    }

/* -------------------------------------------------------------------- */
/*      Now that we have poSrcLayer, we can install a spatial filter    */
/*      if there is one.                                                */
/* -------------------------------------------------------------------- */
    if( poSpatFilter != NULL )
        SetSpatialFilter( 0, poSpatFilter );

    ResetReading();

    FindAndSetIgnoredFields();

    if( !bForwardWhereToSourceLayer )
        SetAttributeFilter( pszWHEREIn );
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
    CPLFree( panGeomFieldToSrcGeomField );

    delete poSummaryFeature;
    delete (swq_select *) pSelectInfo;

    if( poDefn != NULL )
    {
        poDefn->Release();
    }

/* -------------------------------------------------------------------- */
/*      Release any additional datasources being used in joins.         */
/* -------------------------------------------------------------------- */
    for( int iEDS = 0; iEDS < nExtraDSCount; iEDS++ )
        GDALClose( (GDALDatasetH)papoExtraDS[iEDS] );

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

    if( psSelectInfo != NULL )
    {
        for( int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
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
/*                    MustEvaluateSpatialFilterOnGenSQL()               */
/************************************************************************/

int OGRGenSQLResultsLayer::MustEvaluateSpatialFilterOnGenSQL()
{
    int bEvaluateSpatialFilter = FALSE;
    if( m_poFilterGeom != NULL &&
        m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount() )
    {
        int iSrcGeomField = panGeomFieldToSrcGeomField[m_iGeomFieldFilter];
        if( iSrcGeomField < 0 )
            bEvaluateSpatialFilter = TRUE;
    }
    return bEvaluateSpatialFilter;
}

/************************************************************************/
/*                       ApplyFiltersToSource()                         */
/************************************************************************/

void OGRGenSQLResultsLayer::ApplyFiltersToSource()
{
    poSrcLayer->SetAttributeFilter( pszWHERE );
    if( m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount() )
    {
        int iSrcGeomField = panGeomFieldToSrcGeomField[m_iGeomFieldFilter];
        if( iSrcGeomField >= 0 )
            poSrcLayer->SetSpatialFilter( iSrcGeomField, m_poFilterGeom );
    }

    poSrcLayer->ResetReading();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::ResetReading()

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( psSelectInfo->query_mode == SWQM_RECORDSET )
    {
        ApplyFiltersToSource();
    }

    nNextIndexFID = 0;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily reposition        */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::SetNextByIndex( GIntBig nIndex )

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

OGRErr OGRGenSQLResultsLayer::GetExtent( int iGeomField,
                                         OGREnvelope *psExtent,
                                         int bForce )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    if( psSelectInfo->query_mode == SWQM_RECORDSET )
    {
        int iSrcGeomField = panGeomFieldToSrcGeomField[iGeomField];
        if( iSrcGeomField >= 0 )
            return poSrcLayer->GetExtent( iSrcGeomField, psExtent, bForce );
        else if( iGeomField == 0 )
            return OGRLayer::GetExtent( psExtent, bForce );
        else
            return OGRLayer::GetExtent( iGeomField, psExtent, bForce );
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGenSQLResultsLayer::GetFeatureCount( int bForce )

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
    else if( m_poAttrQuery == NULL && !MustEvaluateSpatialFilterOnGenSQL() )
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
        if( expr->table_index == 0 && expr->field_index != -1 )
        {
            OGRLayer* poLayer = papoTableLayers[expr->table_index];
            int nSpecialFieldIdx = expr->field_index -
                            poLayer->GetLayerDefn()->GetFieldCount();
            if( nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                nSpecialFieldIdx == SPF_OGR_GEOM_AREA )
                return TRUE;
            if( expr->field_index ==
                    GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poLayer->GetLayerDefn(), 0) )
                return TRUE;
            return FALSE;
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
    ApplyFiltersToSource();

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
            if (psColDef->table_index == 0 && psColDef->field_index != -1)
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
                if( psColDef->field_index ==
                        GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poLayer->GetLayerDefn(), 0) )
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
/*      GetFeatureCount().                                            */
/* -------------------------------------------------------------------- */

    if( psSelectInfo->result_columns == 1
        && psSelectInfo->column_defs[0].col_func == SWQCF_COUNT
        && psSelectInfo->column_defs[0].field_index < 0 )
    {
        GIntBig nRes = poSrcLayer->GetFeatureCount( TRUE );
        poSummaryFeature->SetField( 0, nRes );

        if( CPL_INT64_FITS_ON_INT32(nRes) )
        {
            poDefn->GetFieldDefn(0)->SetType(OFTInteger);
            delete poSummaryFeature;
            poSummaryFeature = new OGRFeature( poDefn );
            poSummaryFeature->SetFID( 0 );
            poSummaryFeature->SetField( 0, (int)nRes );
        }

        poSrcLayer->GetLayerDefn()->SetGeometryIgnored(bSaveIsGeomIgnored);
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, process all source feature through the summary       */
/*      building facilities of SWQ.                                     */
/* -------------------------------------------------------------------- */
    const char *pszError = NULL;
    OGRFeature *poSrcFeature = NULL;

    while( (poSrcFeature = poSrcLayer->GetNextFeature()) != NULL )
    {
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;

            if (psColDef->col_func == SWQCF_COUNT)
            {
                /* psColDef->field_index can be -1 in the case of a COUNT(*) */
                if (psColDef->field_index < 0)
                    pszError = swq_select_summarize( psSelectInfo, iField, "" );
                else if (IS_GEOM_FIELD_INDEX(poSrcLayer->GetLayerDefn(), psColDef->field_index) )
                {
                    int iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                            poSrcLayer->GetLayerDefn(), psColDef->field_index);
                    OGRGeometry* poGeom = poSrcFeature->GetGeomFieldRef(iSrcGeomField);
                    if( poGeom != NULL )
                        pszError = swq_select_summarize( psSelectInfo, iField, "" );
                    else
                        pszError = NULL;
                }
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
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            if (psSelectInfo->column_summary != NULL)
            {
                swq_summary *psSummary = psSelectInfo->column_summary + iField;
                if( psColDef->col_func == SWQCF_COUNT )
                {
                    if( CPL_INT64_FITS_ON_INT32(psSummary->count) )
                    {
                        delete poSummaryFeature;
                        poSummaryFeature = NULL;
                        poDefn->GetFieldDefn(iField)->SetType(OFTInteger);
                    }
                }
            }
        }

        if( poSummaryFeature == NULL )
        {
            poSummaryFeature = new OGRFeature( poDefn );
            poSummaryFeature->SetFID( 0 );
        }

        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            if (psSelectInfo->column_summary != NULL)
            {
                swq_summary *psSummary = psSelectInfo->column_summary + iField;

                if( psColDef->col_func == SWQCF_AVG && psSummary->count > 0 )
                {
                    if( psColDef->field_type == SWQ_DATE ||
                        psColDef->field_type == SWQ_TIME ||
                        psColDef->field_type == SWQ_TIMESTAMP)
                    {
                        struct tm brokendowntime;
                        double dfAvg = psSummary->sum / psSummary->count;
                        CPLUnixTimeToYMDHMS((GIntBig)dfAvg, &brokendowntime);
                        poSummaryFeature->SetField( iField,
                                                    brokendowntime.tm_year + 1900,
                                                    brokendowntime.tm_mon + 1,
                                                    brokendowntime.tm_mday,
                                                    brokendowntime.tm_hour,
                                                    brokendowntime.tm_min,
                                                    static_cast<float>(brokendowntime.tm_sec + fmod(dfAvg, 1)),
                                                    0);
                    }
                    else
                        poSummaryFeature->SetField( iField,
                                                    psSummary->sum / psSummary->count );
                }
                else if( psColDef->col_func == SWQCF_MIN && psSummary->count > 0 )
                {
                    if( psColDef->field_type == SWQ_DATE ||
                        psColDef->field_type == SWQ_TIME ||
                        psColDef->field_type == SWQ_TIMESTAMP)
                        poSummaryFeature->SetField( iField, psSummary->szMin );
                    else
                        poSummaryFeature->SetField( iField, psSummary->min );
                }
                else if( psColDef->col_func == SWQCF_MAX && psSummary->count > 0 )
                {
                    if( psColDef->field_type == SWQ_DATE ||
                        psColDef->field_type == SWQ_TIME ||
                        psColDef->field_type == SWQ_TIMESTAMP)
                        poSummaryFeature->SetField( iField, psSummary->szMax );
                    else
                        poSummaryFeature->SetField( iField, psSummary->max );
                }
                else if( psColDef->col_func == SWQCF_COUNT )
                    poSummaryFeature->SetField( iField, psSummary->count );
                else if( psColDef->col_func == SWQCF_SUM && psSummary->count > 0 )
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

    OGRFeature *poFeature = (*papoFeatures)[op->table_index];

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

      case SWQ_INTEGER64:
        if( poFeature == NULL
            || !poFeature->IsFieldSet(op->field_index) )
        {
            poRetNode = new swq_expr_node((GIntBig)0);
            poRetNode->is_null = TRUE;
        }
        else
            poRetNode = new swq_expr_node(
                poFeature->GetFieldAsInteger64(op->field_index) );
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

      case SWQ_GEOMETRY:
        if( poFeature == NULL )
        {
            poRetNode = new swq_expr_node( (OGRGeometry*) NULL );
        }
        else
        {
            int iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                poFeature->GetDefnRef(), op->field_index);
            poRetNode = new swq_expr_node(
                poFeature->GetGeomFieldRef(iSrcGeomField) );
        }
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
/*                          GetFilterForJoin()                          */
/************************************************************************/

static CPLString GetFilterForJoin(swq_expr_node* poExpr, OGRFeature* poSrcFeat,
                                  OGRLayer* poJoinLayer, int secondary_table)
{
    if( poExpr->eNodeType == SNT_CONSTANT )
    {
        char* pszRes = poExpr->Unparse(NULL, '"');
        CPLString osRes = pszRes;
        CPLFree(pszRes);
        return osRes;
    }

    if( poExpr->eNodeType == SNT_COLUMN )
    {
        CPLAssert( poExpr->field_index != -1 );
        CPLAssert( poExpr->table_index == 0 || poExpr->table_index == secondary_table );

        if( poExpr->table_index == 0 )
        {
            // if source key is null, we can't do join.
            if( !poSrcFeat->IsFieldSet( poExpr->field_index ) )
            {
                return "";
            }
            OGRFieldType ePrimaryFieldType =
                    poSrcFeat->GetFieldDefnRef(poExpr->field_index)->GetType();
            OGRField *psSrcField =
                    poSrcFeat->GetRawFieldRef(poExpr->field_index);

            switch( ePrimaryFieldType )
            {
            case OFTInteger:
                return CPLString().Printf("%d", psSrcField->Integer );
                break;

            case OFTInteger64:
                return CPLString().Printf(CPL_FRMT_GIB, psSrcField->Integer64 );
                break;

            case OFTReal:
                return CPLString().Printf("%.16g", psSrcField->Real );
                break;

            case OFTString:
            {
                char *pszEscaped = CPLEscapeString( psSrcField->String,
                                                    static_cast<int>(strlen(psSrcField->String)),
                                                    CPLES_SQL );
                CPLString osRes = "'";
                osRes += pszEscaped;
                osRes += "'";
                CPLFree( pszEscaped );
                return osRes;
            }
            break;

            default:
                CPLAssert( false );
                return "";
            }
        }

        if(  poExpr->table_index == secondary_table )
        {
            OGRFieldDefn* poSecondaryFieldDefn =
                poJoinLayer->GetLayerDefn()->GetFieldDefn(poExpr->field_index);
            return CPLSPrintf("\"%s\"", poSecondaryFieldDefn->GetNameRef());
        }

        CPLAssert(false);
        return "";
    }

    if( poExpr->eNodeType == SNT_OPERATION )
    {
        /* ----------------------------------------------------------------- */
        /*      Operation - start by unparsing all the subexpressions.       */
        /* ----------------------------------------------------------------- */
        std::vector<char*> apszSubExpr;
        for( int i = 0; i < poExpr->nSubExprCount; i++ )
        {
            CPLString osSubExpr = GetFilterForJoin(poExpr->papoSubExpr[i], poSrcFeat,
                                                   poJoinLayer, secondary_table);
            if( osSubExpr.size() == 0 )
            {
                for( --i; i >=0; i-- )
                    CPLFree( apszSubExpr[i] );
                return "";
            }
            apszSubExpr.push_back( CPLStrdup(osSubExpr) );
        }

        CPLString osExpr = poExpr->UnparseOperationFromUnparsedSubExpr(&apszSubExpr[0]);

        /* ----------------------------------------------------------------- */
        /*      cleanup subexpressions.                                      */
        /* ----------------------------------------------------------------- */
        for( int i = 0; i < poExpr->nSubExprCount; i++ )
            CPLFree( apszSubExpr[i] );

        return osExpr;
    }

    return "";
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::TranslateFeature( OGRFeature *poSrcFeat )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    std::vector<OGRFeature*> apoFeatures;

    if( poSrcFeat == NULL )
        return NULL;

    m_nFeaturesRead++;

    apoFeatures.push_back( poSrcFeat );

/* -------------------------------------------------------------------- */
/*      Fetch the corresponding features from any jointed tables.       */
/* -------------------------------------------------------------------- */
    for( int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        CPLString osFilter;

        swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;

        /* OGRMultiFeatureFetcher assumes that the features are pushed in */
        /* apoFeatures with increasing secondary_table, so make sure */
        /* we have taken care of this */
        CPLAssert(psJoinInfo->secondary_table == iJoin + 1);

        OGRLayer *poJoinLayer = papoTableLayers[psJoinInfo->secondary_table];

        osFilter = GetFilterForJoin(psJoinInfo->poExpr, poSrcFeat, poJoinLayer,
                                    psJoinInfo->secondary_table);
        //CPLDebug("OGR", "Filter = %s\n", osFilter.c_str());

        // if source key is null, we can't do join.
        if( osFilter.size() == 0 )
        {
            apoFeatures.push_back( NULL );
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
    OGRFeature *poDstFeat = new OGRFeature( poDefn );

    poDstFeat->SetFID( poSrcFeat->GetFID() );

    poDstFeat->SetStyleString( poSrcFeat->GetStyleString() );
    poDstFeat->SetNativeData( poSrcFeat->GetNativeData() );
    poDstFeat->SetNativeMediaType( poSrcFeat->GetNativeMediaType() );

/* -------------------------------------------------------------------- */
/*      Evaluate fields that are complex expressions.                   */
/* -------------------------------------------------------------------- */
    int iRegularField = 0;
    int iGeomField = 0;
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        if( psColDef->field_index != -1 )
        {
            if( psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY )
                iGeomField++;
            else
                iRegularField++;
            continue;
        }

        swq_expr_node *poResult =
            psColDef->expr->Evaluate( OGRMultiFeatureFetcher,
                                      (void *) &apoFeatures );

        if( poResult == NULL )
        {
            delete poDstFeat;
            return NULL;
        }

        if( poResult->is_null )
        {
            if( poResult->field_type == SWQ_GEOMETRY )
                iGeomField++;
            else
                iRegularField++;
            delete poResult;
            continue;
        }

        switch( poResult->field_type )
        {
          case SWQ_BOOLEAN:
          case SWQ_INTEGER:
            poDstFeat->SetField( iRegularField++, (int)poResult->int_value );
            break;

          case SWQ_INTEGER64:
            poDstFeat->SetField( iRegularField++, poResult->int_value );
            break;

          case SWQ_FLOAT:
            poDstFeat->SetField( iRegularField++, poResult->float_value );
            break;

          case SWQ_GEOMETRY:
          {
            OGRGenSQLGeomFieldDefn* poGeomFieldDefn =
                (OGRGenSQLGeomFieldDefn*) poDstFeat->GetGeomFieldDefnRef(iGeomField);
            if( poGeomFieldDefn->bForceGeomType &&
                poResult->geometry_value != NULL )
            {
                OGRwkbGeometryType eCurType =
                    wkbFlatten(poResult->geometry_value->getGeometryType());
                OGRwkbGeometryType eReqType =
                    wkbFlatten(poGeomFieldDefn->GetType());
                if( eCurType == wkbPolygon && eReqType == wkbMultiPolygon )
                {
                    poResult->geometry_value = (OGRGeometry*)
                        OGR_G_ForceToMultiPolygon( (OGRGeometryH) poResult->geometry_value );
                }
                else if( (eCurType == wkbMultiPolygon || eCurType == wkbGeometryCollection) &&
                         eReqType == wkbPolygon )
                {
                    poResult->geometry_value = (OGRGeometry*)
                        OGR_G_ForceToPolygon( (OGRGeometryH) poResult->geometry_value );
                }
                else if( eCurType == wkbLineString && eReqType == wkbMultiLineString )
                {
                    poResult->geometry_value = (OGRGeometry*)
                        OGR_G_ForceToMultiLineString( (OGRGeometryH) poResult->geometry_value );
                }
                else if( (eCurType == wkbMultiLineString || eCurType == wkbGeometryCollection) &&
                         eReqType == wkbLineString )
                {
                    poResult->geometry_value = (OGRGeometry*)
                        OGR_G_ForceToLineString( (OGRGeometryH) poResult->geometry_value );
                }
            }
            poDstFeat->SetGeomField( iGeomField++, poResult->geometry_value );
            break;
          }

          default:
            poDstFeat->SetField( iRegularField++, poResult->string_value );
            break;
        }

        delete poResult;
    }

/* -------------------------------------------------------------------- */
/*      Copy fields from primary record to the destination feature.     */
/* -------------------------------------------------------------------- */
    iRegularField = 0;
    iGeomField = 0;
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;

        if( psColDef->table_index != 0 )
        {
            if( psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY )
                iGeomField++;
            else
                iRegularField++;
            continue;
        }

        if( IS_GEOM_FIELD_INDEX(poSrcFeat->GetDefnRef(), psColDef->field_index) )
        {
            int iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                poSrcFeat->GetDefnRef(), psColDef->field_index);
            poDstFeat->SetGeomField( iGeomField ++,
                                     poSrcFeat->GetGeomFieldRef(iSrcGeomField) );
        }
        else if( psColDef->field_index >= iFIDFieldIndex &&
                 psColDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
        {
            switch (SpecialFieldTypes[psColDef->field_index - iFIDFieldIndex])
            {
              case SWQ_INTEGER:
              case SWQ_INTEGER64:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsInteger64(psColDef->field_index) );
                break;
              case SWQ_FLOAT:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsDouble(psColDef->field_index) );
                break;
              default:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsString(psColDef->field_index) );
            }
            iRegularField ++;
        }
        else
        {
            switch (psColDef->target_type)
            {
              case SWQ_INTEGER:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsInteger(psColDef->field_index) );
                break;

              case SWQ_INTEGER64:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsInteger64(psColDef->field_index) );
                break;

              case SWQ_FLOAT:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsDouble(psColDef->field_index) );
                break;

              case SWQ_STRING:
              case SWQ_TIMESTAMP:
              case SWQ_DATE:
              case SWQ_TIME:
                poDstFeat->SetField( iRegularField, poSrcFeat->GetFieldAsString(psColDef->field_index) );
                break;

              case SWQ_GEOMETRY:
                  CPLAssert(false);
                  break;

              default:
                poDstFeat->SetField( iRegularField,
                         poSrcFeat->GetRawFieldRef( psColDef->field_index ) );
            }
            iRegularField ++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy values from any joined tables.                             */
/* -------------------------------------------------------------------- */
    for( int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        CPLString osFilter;

        swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
        OGRFeature *poJoinFeature = apoFeatures[iJoin+1];

        if( poJoinFeature == NULL )
            continue;

        // Copy over selected field values.
        iRegularField = 0;
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;

            if( psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY )
                continue;

            if( psColDef->table_index == psJoinInfo->secondary_table )
                poDstFeat->SetField( iRegularField,
                                     poJoinFeature->GetRawFieldRef(
                                         psColDef->field_index ) );

            iRegularField ++;
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

    int bEvaluateSpatialFilter = MustEvaluateSpatialFilterOnGenSQL();

/* -------------------------------------------------------------------- */
/*      Handle ordered sets.                                            */
/* -------------------------------------------------------------------- */
    while( true )
    {
        OGRFeature *poFeature = NULL;

        if( panFIDIndex != NULL )
            poFeature = GetFeature( nNextIndexFID++ );
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

        if( (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) &&
            (!bEvaluateSpatialFilter ||
             FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) )) )
            return poFeature;

        delete poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::GetFeature( GIntBig nFID )

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

    if( poSrcFeature == NULL )
        return NULL;

    OGRFeature *poResult = TranslateFeature( poSrcFeature );
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
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD &&
        poSummaryFeature == NULL )
    {
        // Run PrepareSummary() is we have a COUNT column so as to be
        // able to downcast it from OFTInteger64 to OFTInteger
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;
            if( psColDef->col_func == SWQCF_COUNT )
            {
                PrepareSummary();
                break;
            }
        }
    }

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
    int nOrderItems = psSelectInfo->order_specs;

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
    size_t nFeaturesAlloc = 100;

    panFIDIndex = NULL;
    OGRField *pasIndexFields = static_cast<OGRField *>(
        CPLCalloc(sizeof(OGRField), nOrderItems * nFeaturesAlloc));
    GIntBig *panFIDList = static_cast<GIntBig *>(
        CPLMalloc(sizeof(GIntBig) * nFeaturesAlloc));

/* -------------------------------------------------------------------- */
/*      Read in all the key values.                                     */
/* -------------------------------------------------------------------- */
    OGRFeature *poSrcFeat = NULL;
    nIndexSize = 0;

    while( (poSrcFeat = poSrcLayer->GetNextFeature()) != NULL )
    {
        if ((size_t)nIndexSize == nFeaturesAlloc)
        {
            GIntBig nNewFeaturesAlloc = (nFeaturesAlloc * 4) / 3;
            if( (GIntBig)(size_t)(sizeof(OGRField) * nOrderItems * nNewFeaturesAlloc) !=
                    (GIntBig)sizeof(OGRField) * nOrderItems * nNewFeaturesAlloc )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate pasIndexFields");
                VSIFree(pasIndexFields);
                VSIFree(panFIDList);
                nIndexSize = 0;
                return;
            }
            OGRField* pasNewIndexFields = (OGRField *)
                VSI_REALLOC_VERBOSE(pasIndexFields,
                           sizeof(OGRField) * nOrderItems * (size_t)nNewFeaturesAlloc);
            if (pasNewIndexFields == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate pasIndexFields");
                VSIFree(pasIndexFields);
                VSIFree(panFIDList);
                nIndexSize = 0;
                return;
            }
            pasIndexFields = pasNewIndexFields;

            GIntBig* panNewFIDList = (GIntBig *)
                VSI_REALLOC_VERBOSE(panFIDList, sizeof(GIntBig) *  (size_t)nNewFeaturesAlloc);
            if (panNewFIDList == NULL)
            {
                VSIFree(pasIndexFields);
                VSIFree(panFIDList);
                nIndexSize = 0;
                return;
            }
            panFIDList = panNewFIDList;

            memset(pasIndexFields + nFeaturesAlloc, 0,
                   sizeof(OGRField) * nOrderItems * (size_t)(nNewFeaturesAlloc - nFeaturesAlloc));

            nFeaturesAlloc = (size_t)nNewFeaturesAlloc;
        }

        for( int iKey = 0; iKey < nOrderItems; iKey++ )
        {
            swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
            OGRField *psDstField = pasIndexFields + nIndexSize * nOrderItems + iKey;

            if ( psKeyDef->field_index >= iFIDFieldIndex)
            {
                if ( psKeyDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
                {
                    switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
                    {
                      case SWQ_INTEGER:
                      case SWQ_INTEGER64:
                        psDstField->Integer64 = poSrcFeat->GetFieldAsInteger64(psKeyDef->field_index);
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

            OGRFieldDefn *poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn(
                psKeyDef->field_index );

            OGRField *psSrcField =
                poSrcFeat->GetRawFieldRef( psKeyDef->field_index );

            if( poFDefn->GetType() == OFTInteger
                || poFDefn->GetType() == OFTInteger64
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
    panFIDIndex = (GIntBig *) VSI_MALLOC_VERBOSE(sizeof(GIntBig) * (size_t)nIndexSize);
    if( panFIDIndex == NULL )
    {
        VSIFree(pasIndexFields);
        VSIFree(panFIDList);
        nIndexSize = 0;
        return;
    }
    for( int i = 0; i < nIndexSize; i++ )
        panFIDIndex[i] = i;

/* -------------------------------------------------------------------- */
/*      Quick sort the records.                                         */
/* -------------------------------------------------------------------- */
    if( !SortIndexSection( pasIndexFields, 0, nIndexSize ) )
    {
        VSIFree(pasIndexFields);
        VSIFree(panFIDList);
        nIndexSize = 0;
        VSIFree(panFIDIndex);
        panFIDIndex = NULL;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Rework the FID map to map to real FIDs.                         */
/* -------------------------------------------------------------------- */
    int bAlreadySorted = TRUE;
    for( int i = 0; i < nIndexSize; i++ )
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

        if ( psKeyDef->field_index >= iFIDFieldIndex &&
            psKeyDef->field_index < iFIDFieldIndex + SPECIAL_FIELD_COUNT )
        {
            /* warning: only special fields of type string should be deallocated */
            if (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex] == SWQ_STRING)
            {
                for( int i = 0; i < nIndexSize; i++ )
                {
                    OGRField *psField = pasIndexFields + iKey + i * nOrderItems;
                    CPLFree( psField->String );
                }
            }
            continue;
        }

        OGRFieldDefn *poFDefn =
            poSrcLayer->GetLayerDefn()->GetFieldDefn( psKeyDef->field_index );

        if( poFDefn->GetType() == OFTString )
        {
            for( int i = 0; i < nIndexSize; i++ )
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
    /* on the source array. Very useful for layers where random access */
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

int OGRGenSQLResultsLayer::SortIndexSection( OGRField *pasIndexFields,
                                              GIntBig nStart, GIntBig nEntries )

{
    if( nEntries < 2 )
        return TRUE;

    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    int      nOrderItems = psSelectInfo->order_specs;

    GIntBig nFirstGroup = nEntries / 2;
    GIntBig nFirstStart = nStart;
    GIntBig nSecondGroup = nEntries - nFirstGroup;
    GIntBig nSecondStart = nStart + nFirstGroup;
    GIntBig iMerge = 0;

    if( !SortIndexSection( pasIndexFields, nFirstStart, nFirstGroup ) ||
        !SortIndexSection( pasIndexFields, nSecondStart, nSecondGroup ) )
        return FALSE;

    GIntBig *panMerged = (GIntBig *)
        VSI_MALLOC_VERBOSE( sizeof(GIntBig) * (size_t)nEntries );
    if( panMerged == NULL )
    {
        return FALSE;
    }

    while( iMerge < nEntries )
    {
        int  nResult = 0;

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

    memcpy( panFIDIndex + nStart, panMerged, sizeof(GIntBig) * (size_t)nEntries );
    CPLFree( panMerged );

    return TRUE;
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
        OGRFieldDefn *poFDefn = NULL;

        if( psKeyDef->field_index >= iFIDFieldIndex + SPECIAL_FIELD_COUNT )
        {
            CPLAssert( false );
            return 0;
        }
        else if( psKeyDef->field_index >= iFIDFieldIndex )
            poFDefn = NULL;
        else
            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn(
                psKeyDef->field_index );

        if( pasFirstTuple[iKey].Set.nMarker1 == OGRUnsetMarker
             && pasFirstTuple[iKey].Set.nMarker2 == OGRUnsetMarker )
        {
            if( pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker
                && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker )
                nResult = 0;
            else
                nResult = -1;
        }
        else if ( pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker
                && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker )
        {
            nResult = 1;
        }
        else if ( poFDefn == NULL )
        {
            switch (SpecialFieldTypes[psKeyDef->field_index - iFIDFieldIndex])
            {
              case SWQ_INTEGER:
              case SWQ_INTEGER64:
                if( pasFirstTuple[iKey].Integer64 < pasSecondTuple[iKey].Integer64 )
                    nResult = -1;
                else if( pasFirstTuple[iKey].Integer64 > pasSecondTuple[iKey].Integer64 )
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
                CPLAssert( false );
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
        else if( poFDefn->GetType() == OFTInteger64 )
        {
            if( pasFirstTuple[iKey].Integer64 < pasSecondTuple[iKey].Integer64 )
                nResult = -1;
            else if( pasFirstTuple[iKey].Integer64
                     > pasSecondTuple[iKey].Integer64 )
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
        ExploreExprForIgnoredFields(psJoinDef->poExpr, hSet);
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
        char** papszIgnoredFields = NULL;
        for( int iSrcField = 0;
             iSrcField<poSrcFDefn->GetFieldCount();
             iSrcField++ )
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

void OGRGenSQLResultsLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeom )
{
    InvalidateOrderByIndex();
    if( iGeomField == 0 )
        OGRLayer::SetSpatialFilter(poGeom);
    else
        OGRLayer::SetSpatialFilter(iGeomField, poGeom);
}

//! @endcond
