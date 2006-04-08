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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.17  2006/04/08 06:14:31  fwarmerdam
 * added proper (I believe) support for spatial and attribute filter on gensql layers
 *
 * Revision 1.16  2006/03/21 18:46:28  fwarmerdam
 * Don't report debug message if setting NULL spatial filter.
 *
 * Revision 1.15  2005/09/21 01:01:28  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.14  2005/02/02 20:00:29  fwarmerdam
 * added SetNextByIndex support
 *
 * Revision 1.13  2003/03/20 19:13:21  warmerda
 * Added ClearFilters() method to cleanup spatial or attribute filters on the
 * target layer, and any joined layers.  Used in destructor and after all
 * features have been read from source layer.
 *
 * Revision 1.12  2003/03/20 17:43:43  warmerda
 * fixed bug when secondary schema larger than primary schema
 *
 * Revision 1.11  2003/03/19 20:31:18  warmerda
 * add support for tables from external datasources
 *
 * Revision 1.10  2003/03/05 05:10:17  warmerda
 * implement join support
 *
 * Revision 1.9  2002/10/25 15:00:41  warmerda
 * Fixed int/long type mismatch.
 *
 * Revision 1.8  2002/08/12 16:40:56  dron
 * stricmp() function replaced by EQUALN() macro
 *
 * Revision 1.7  2002/08/12 15:06:01  warmerda
 * fix DISTINCT not working with FID (from Bruce)
 *
 * 
 * Revision 1.6  2002/04/29 19:35:50  warmerda
 * fixes for selecting FID
 *
 * Revision 1.5  2002/04/29 18:13:36  warmerda
 * Fix serious bug with datasets with sparse FID sets.
 *
 * Revision 1.4  2002/04/25 21:03:07  warmerda
 * avoid strcasecmp
 *
 * Revision 1.3  2002/04/25 16:07:55  warmerda
 * fleshed out DISTINCT support
 *
 * Revision 1.2  2002/04/25 03:42:04  warmerda
 * fixed spatial filter support on SQL results
 *
 * Revision 1.1  2002/04/25 02:24:37  warmerda
 * New
 *
 */

#include "ogr_p.h"
#include "ogr_gensql.h"
#include "cpl_string.h"

CPL_C_START
#include "swq.h"
CPL_C_END

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRGenSQLResultsLayer()                        */
/************************************************************************/

OGRGenSQLResultsLayer::OGRGenSQLResultsLayer( OGRDataSource *poSrcDS,
                                              void *pSelectInfo, 
                                              OGRGeometry *poSpatFilter )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    this->poSrcDS = poSrcDS;
    this->pSelectInfo = pSelectInfo;
    poDefn = NULL;
    poSummaryFeature = NULL;
    panFIDIndex = NULL;
    nIndexSize = 0;
    nNextIndexFID = 0;
    nExtraDSCount = 0;
    papoExtraDS = NULL;

    if( poSpatFilter != NULL )
        SetSpatialFilter( poSpatFilter );

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
/*      Prepare a feature definition based on the query.                */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();

    poDefn = new OGRFeatureDefn( psSelectInfo->table_defs[0].table_alias );
    poDefn->Reference();

    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        OGRFieldDefn oFDefn( psColDef->field_name, OFTInteger );
        OGRFieldDefn *poSrcFDefn = NULL;
        OGRFeatureDefn *poLayerDefn = 
            papoTableLayers[psColDef->table_index]->GetLayerDefn();

        if( psColDef->field_index > -1 
            && psColDef->field_index < poLayerDefn->GetFieldCount() )
            poSrcFDefn = poLayerDefn->GetFieldDefn(psColDef->field_index);

        if( psColDef->col_func_name != NULL )
        {
            oFDefn.SetName( CPLSPrintf( "%s_%s",psColDef->col_func_name,
                                        psColDef->field_name) );
        }

        if( psColDef->col_func == SWQCF_COUNT )
            oFDefn.SetType( OFTInteger );
        else if( poSrcFDefn != NULL )
        {
            oFDefn.SetType( poSrcFDefn->GetType() );
            oFDefn.SetWidth( poSrcFDefn->GetWidth() );
            oFDefn.SetPrecision( poSrcFDefn->GetPrecision() );
        }

        poDefn->AddFieldDefn( &oFDefn );
    }

    poDefn->SetGeomType( poSrcLayer->GetLayerDefn()->GetGeomType() );

    iFIDFieldIndex = poSrcDefn->GetFieldCount();

/* -------------------------------------------------------------------- */
/*      If an ORDER BY is in effect, apply it now.                      */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->order_specs > 0 
        && psSelectInfo->query_mode == SWQM_RECORDSET )
        CreateOrderByIndex();

    ResetReading();
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
             
    if( panFIDIndex != NULL )
        CPLFree( panFIDIndex );

    if( poSummaryFeature )
        delete poSummaryFeature;

    if( pSelectInfo != NULL )
        swq_select_free( (swq_select *) pSelectInfo );

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
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::ResetReading() 

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

    if( psSelectInfo->query_mode == SWQM_RECORDSET )
    {
        poSrcLayer->SetAttributeFilter( psSelectInfo->whole_where_clause );
        
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

    if( psSelectInfo->query_mode != SWQM_RECORDSET )
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
    poSrcLayer->SetAttributeFilter( psSelectInfo->whole_where_clause );
    
    poSrcLayer->SetSpatialFilter( m_poFilterGeom );
        
    poSrcLayer->ResetReading();

/* -------------------------------------------------------------------- */
/*      We treat COUNT(*) (or COUNT of anything without distinct) as    */
/*      a special case, and fill with GetFeatureCount().                */
/* -------------------------------------------------------------------- */

    if( psSelectInfo->result_columns == 1 
        && psSelectInfo->column_defs[0].col_func == SWQCF_COUNT
        && !psSelectInfo->column_defs[0].distinct_flag )
    {
        poSummaryFeature->SetField( 0, poSrcLayer->GetFeatureCount( TRUE ) );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, process all source feature through the summary       */
/*      building facilities of SWQ.                                     */
/* -------------------------------------------------------------------- */
    const char *pszError;
    OGRFeature *poSrcFeature;

    while( (poSrcFeature = poSrcLayer->GetNextFeature()) != NULL )
    {
        for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
        {
            swq_col_def *psColDef = psSelectInfo->column_defs + iField;

            if( EQUALN(psColDef->field_name, "FID", 3) )
            {
                // Special case where the column is "FID"
                char szBuffer[255];
                sprintf( szBuffer, "%ld", poSrcFeature->GetFID() );
                pszError = 
                    swq_select_summarize( psSelectInfo, iField, szBuffer);
            }
            else
            {
                pszError = 
                    swq_select_summarize( psSelectInfo, iField, 
                                          poSrcFeature->GetFieldAsString( 
                                              psColDef->field_index ) );
            }
            
            if( pszError != NULL )
            {
                delete poSummaryFeature;
                poSummaryFeature = NULL;

                CPLError( CE_Failure, CPLE_AppDefined, "%s", pszError );
                return FALSE;
            }
        }

        delete poSrcFeature;
    }

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
    }

    return TRUE;
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::TranslateFeature( OGRFeature *poSrcFeat )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;
    OGRFeature *poDstFeat;

    if( poSrcFeat == NULL )
        return NULL;

    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Create destination feature.                                     */
/* -------------------------------------------------------------------- */
    poDstFeat = new OGRFeature( poDefn );

    poDstFeat->SetFID( poSrcFeat->GetFID() );

    poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );
    
/* -------------------------------------------------------------------- */
/*      Copy fields from primary record to the destination feature.     */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;

        if( psColDef->field_index == iFIDFieldIndex )
            poDstFeat->SetField( iField, (int) poSrcFeat->GetFID() );
        else if( psColDef->table_index == 0 )
            poDstFeat->SetField( iField,
                         poSrcFeat->GetRawFieldRef( psColDef->field_index ) );
    }

/* -------------------------------------------------------------------- */
/*      Copy values from any joined tables.                             */
/* -------------------------------------------------------------------- */
    int iJoin;

    for( iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++ )
    {
        char szFilter[512];

        swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
        OGRLayer *poJoinLayer = papoTableLayers[psJoinInfo->secondary_table];

        // Prepare attribute query to express fetching on the joined variable
        sprintf( szFilter, "%s = ", 
                 poJoinLayer->GetLayerDefn()->GetFieldDefn( 
                     psJoinInfo->secondary_field )->GetNameRef() );

        OGRField *psSrcField = 
            poSrcFeat->GetRawFieldRef(psJoinInfo->primary_field);

        switch( poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                    psJoinInfo->primary_field )->GetType() )
        {
          case OFTInteger:
            sprintf( szFilter+strlen(szFilter), "%d", psSrcField->Integer );
            break;

          case OFTReal:
            sprintf( szFilter+strlen(szFilter), "%.16g", psSrcField->Real );
            break;

          case OFTString:
            // the string really ought to be escaped. 
            sprintf( szFilter+strlen(szFilter), "\"%s\"", 
                     psSrcField->String );
            break;

          default:
            CPLAssert( FALSE );
            continue;
        }

        poJoinLayer->ResetReading();
        if( poJoinLayer->SetAttributeFilter( szFilter ) != OGRERR_NONE )
            continue;

        // Fetch first joined feature.
        OGRFeature *poJoinFeature;

        poJoinFeature = poJoinLayer->GetNextFeature();

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

/* -------------------------------------------------------------------- */
/*      Handle request for summary record.                              */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->query_mode == SWQM_SUMMARY_RECORD )
    {
        if( !PrepareSummary() || nFID != 0 )
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

        if( nFID < 0 || nFID >= psSummary->count )
            return NULL;

        poSummaryFeature->SetField( 0, psSummary->distinct_list[nFID] );
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

    if( nOrderItems == 0 )
        return;

    ResetReading();

/* -------------------------------------------------------------------- */
/*      Allocate set of key values, and the output index.               */
/* -------------------------------------------------------------------- */
    nIndexSize = poSrcLayer->GetFeatureCount();

    pasIndexFields = (OGRField *) 
        CPLCalloc(sizeof(OGRField), nOrderItems * nIndexSize);
    panFIDIndex = (long *) CPLCalloc(sizeof(long),nIndexSize);
    panFIDList = (long *) CPLCalloc(sizeof(long),nIndexSize);

    for( i = 0; i < nIndexSize; i++ )
        panFIDIndex[i] = i;

/* -------------------------------------------------------------------- */
/*      Read in all the key values.                                     */
/* -------------------------------------------------------------------- */
    OGRFeature *poSrcFeat;
    int         iFeature = 0;

    while( (poSrcFeat = poSrcLayer->GetNextFeature()) != NULL )
    {
        int iKey;

        for( iKey = 0; iKey < nOrderItems; iKey++ )
        {
            swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
            OGRFieldDefn *poFDefn;
            OGRField *psSrcField, *psDstField;

            psDstField = pasIndexFields + iFeature * nOrderItems + iKey;

            if( psKeyDef->field_index == iFIDFieldIndex )
            {
                psDstField->Integer = poSrcFeat->GetFID();
                continue;
            }

            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                psKeyDef->field_index );

            psSrcField = poSrcFeat->GetRawFieldRef( psKeyDef->field_index );

            if( poFDefn->GetType() == OFTInteger 
                || poFDefn->GetType() == OFTReal )
                memcpy( psDstField, psSrcField, sizeof(OGRField) );
            else if( poFDefn->GetType() == OFTString )
            {
                if( poSrcFeat->IsFieldSet( psKeyDef->field_index ) )
                    psDstField->String = CPLStrdup( psSrcField->String );
                else
                    memcpy( psDstField, psSrcField, sizeof(OGRField) );
            }
        }

        panFIDList[iFeature] = poSrcFeat->GetFID();
        delete poSrcFeat;

        iFeature++;
    }

    CPLAssert( nIndexSize == iFeature );

/* -------------------------------------------------------------------- */
/*      Quick sort the records.                                         */
/* -------------------------------------------------------------------- */
    SortIndexSection( pasIndexFields, 0, nIndexSize );

/* -------------------------------------------------------------------- */
/*      Rework the FID map to map to real FIDs.                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nIndexSize; i++ )
        panFIDIndex[i] = panFIDList[panFIDIndex[i]];

    CPLFree( panFIDList );

/* -------------------------------------------------------------------- */
/*      Free the key field values.                                      */
/* -------------------------------------------------------------------- */
    for( int iKey = 0; iKey < nOrderItems; iKey++ )
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRFieldDefn *poFDefn;

        if( psKeyDef->field_index == iFIDFieldIndex )
            continue;

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

    memcpy( panFIDIndex + nStart, panMerged, sizeof(int) * nEntries );
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

        if( psKeyDef->field_index == iFIDFieldIndex )
            poFDefn = NULL;
        else
            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                psKeyDef->field_index );
        
        if( (pasFirstTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
             && pasFirstTuple[iKey].Set.nMarker2 == OGRUnsetMarker)
            || (pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
                && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker) )
            nResult = 0;
        else if( poFDefn == NULL || poFDefn->GetType() == OFTInteger )
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

        if( psKeyDef->ascending_flag )
            nResult *= -1;
    }

    return nResult;
}
