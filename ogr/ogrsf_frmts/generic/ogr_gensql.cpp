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

    if( poSpatFilter != NULL )
        this->poSpatialFilter = poSpatFilter->clone();
    else
        this->poSpatialFilter = NULL;

/* -------------------------------------------------------------------- */
/*      Find the source layer on the source dataset.  Eventually        */
/*      selects might be able to operate on more than one table at a    */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    poSrcLayer = NULL;
    for( int iLayer = 0; iLayer < poSrcDS->GetLayerCount(); iLayer++ )
    {
        if( EQUAL(poSrcDS->GetLayer(iLayer)->GetLayerDefn()->GetName(),
                  psSelectInfo->from_table) )
        {
            poSrcLayer = poSrcDS->GetLayer(iLayer);
            break;
        }
    }

    if( poSrcLayer == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Prepare a feature definition based on the query.                */
/* -------------------------------------------------------------------- */
    poDefn = new OGRFeatureDefn( psSelectInfo->from_table );

    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;
        OGRFieldDefn oFDefn( psColDef->field_name, OFTInteger );
        OGRFieldDefn *poSrcFDefn;

        if( psColDef->field_index > -1 )
            poSrcFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn(psColDef->field_index);

        if( psColDef->col_func_name != NULL )
        {
            oFDefn.SetName( CPLSPrintf( "%s_%s",psColDef->col_func_name,
                                        psColDef->field_name) );
        }

        if( psColDef->col_func == SWQCF_COUNT )
            oFDefn.SetType( OFTInteger );
        else
        {
            oFDefn.SetType( poSrcFDefn->GetType() );
            oFDefn.SetWidth( poSrcFDefn->GetWidth() );
            oFDefn.SetPrecision( poSrcFDefn->GetPrecision() );
        }

        poDefn->AddFieldDefn( &oFDefn );
    }

    poDefn->SetGeomType( poSrcLayer->GetLayerDefn()->GetGeomType() );

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
    if( panFIDIndex != NULL )
        CPLFree( panFIDIndex );

    if( poSummaryFeature )
        delete poSummaryFeature;

    if( pSelectInfo != NULL )
        swq_select_free( (swq_select *) pSelectInfo );

    if( poDefn != NULL )
        delete poDefn;
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
        
        poSrcLayer->SetSpatialFilter( poSpatialFilter );
        
        poSrcLayer->ResetReading();
    }

    nNextIndexFID = 0;
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
    else
        return poSrcLayer->GetFeatureCount( bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGenSQLResultsLayer::TestCapability( const char *pszCap )

{
    swq_select *psSelectInfo = (swq_select *) pSelectInfo;

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
    
    poSrcLayer->SetSpatialFilter( poSpatialFilter );
        
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

            pszError = 
                swq_select_summarize( psSelectInfo, iField, 
                                      poSrcFeature->GetFieldAsString( 
                                          psColDef->field_index ) );
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

    poDstFeat = new OGRFeature( poDefn );

    poDstFeat->SetFID( poSrcFeat->GetFID() );

    poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );
    
    for( int iField = 0; iField < psSelectInfo->result_columns; iField++ )
    {
        swq_col_def *psColDef = psSelectInfo->column_defs + iField;

        poDstFeat->SetField( iField,
                    poSrcFeat->GetRawFieldRef( psColDef->field_index ) );
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
    if( panFIDIndex != NULL )
        return GetFeature( nNextIndexFID++ );

/* -------------------------------------------------------------------- */
/*      Regular result sets.  Extract the desired fields from the       */
/*      source feature into the destination feature.                    */
/* -------------------------------------------------------------------- */
    OGRFeature *poSrcFeat = poSrcLayer->GetNextFeature();

    if( poSrcFeat == NULL )
        return NULL;

    OGRFeature *poDstFeat = TranslateFeature( poSrcFeat );

    delete poSrcFeat;

    return poDstFeat;
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
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGenSQLResultsLayer::SetSpatialFilter( OGRGeometry * )

{
    CPLDebug( "OGRGenSQLResultsLayer", "SetSpatialFilter called and ignored.");
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

            poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
                psKeyDef->field_index );

            psDstField = pasIndexFields + iFeature * nOrderItems + iKey;
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

        iFeature++;
    }

/* -------------------------------------------------------------------- */
/*      Quick sort the records.                                         */
/* -------------------------------------------------------------------- */
    SortIndexSection( pasIndexFields, 0, nIndexSize );

/* -------------------------------------------------------------------- */
/*      Free the key field values.                                      */
/* -------------------------------------------------------------------- */
    for( int iKey = 0; iKey < nOrderItems; iKey++ )
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRFieldDefn *poFDefn;

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
        
        poFDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( 
            psKeyDef->field_index );
        
        if( (pasFirstTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
             && pasFirstTuple[iKey].Set.nMarker2 == OGRUnsetMarker)
            || (pasSecondTuple[iKey].Set.nMarker1 == OGRUnsetMarker 
                && pasSecondTuple[iKey].Set.nMarker2 == OGRUnsetMarker) )
            nResult = 0;
        else if( poFDefn->GetType() == OFTString )
            nResult = strcasecmp(pasFirstTuple[iKey].String,
                                 pasSecondTuple[iKey].String);
        else if( poFDefn->GetType() == OFTInteger )
        {
            if( pasFirstTuple[iKey].Integer < pasSecondTuple[iKey].Integer )
                nResult = -1;
            else if( pasFirstTuple[iKey].Integer 
                     > pasSecondTuple[iKey].Integer )
                nResult = 1;
        }
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
