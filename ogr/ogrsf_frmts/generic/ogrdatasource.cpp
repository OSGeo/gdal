/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_gensql.h"
#include "ogr_attrind.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::OGRDataSource()

{
    m_poStyleTable = NULL;
    m_nRefCount = 0;
    m_poDriver = NULL;
    m_hMutex = NULL;
}

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::~OGRDataSource()

{
    if ( m_poStyleTable )
    {
        delete m_poStyleTable;
        m_poStyleTable = NULL;
    }

    if( m_hMutex != NULL )
        CPLDestroyMutex( m_hMutex );
}

/************************************************************************/
/*                         DestroyDataSource()                          */
/************************************************************************/

void OGRDataSource::DestroyDataSource( OGRDataSource *poDS )

{
    delete poDS;
}

/************************************************************************/
/*                           OGR_DS_Destroy()                           */
/************************************************************************/

void OGR_DS_Destroy( OGRDataSourceH hDS )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_Destroy" );
    delete (OGRDataSource *) hDS;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

OGRErr OGRDataSource::Release()

{
    return OGRSFDriverRegistrar::GetRegistrar()->ReleaseDataSource( this );
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

int OGRDataSource::Reference()

{
    return ++m_nRefCount;
}

/************************************************************************/
/*                          OGR_DS_Reference()                          */
/************************************************************************/

int OGR_DS_Reference( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_Reference", 0 );

    return ((OGRDataSource *) hDataSource)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

int OGRDataSource::Dereference()

{
    return --m_nRefCount;
}

/************************************************************************/
/*                         OGR_DS_Dereference()                         */
/************************************************************************/

int OGR_DS_Dereference( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_Dereference", 0 );

    return ((OGRDataSource *) hDataSource)->Dereference();
}

/************************************************************************/
/*                            GetRefCount()                             */
/************************************************************************/

int OGRDataSource::GetRefCount() const

{
    return m_nRefCount;
}

/************************************************************************/
/*                         OGR_DS_GetRefCount()                         */
/************************************************************************/

int OGR_DS_GetRefCount( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_GetRefCount", 0 );

    return ((OGRDataSource *) hDataSource)->GetRefCount();
}

/************************************************************************/
/*                         GetSummaryRefCount()                         */
/************************************************************************/

int OGRDataSource::GetSummaryRefCount() const

{
    CPLMutexHolderD( (void **) &m_hMutex );
    int nSummaryCount = m_nRefCount;
    int iLayer;
    OGRDataSource *poUseThis = (OGRDataSource *) this;

    for( iLayer=0; iLayer < poUseThis->GetLayerCount(); iLayer++ )
        nSummaryCount += poUseThis->GetLayer( iLayer )->GetRefCount();

    return nSummaryCount;
}

/************************************************************************/
/*                     OGR_DS_GetSummaryRefCount()                      */
/************************************************************************/

int OGR_DS_GetSummaryRefCount( OGRDataSourceH hDataSource )

{
    VALIDATE_POINTER1( hDataSource, "OGR_DS_GetSummaryRefCount", 0 );

    return ((OGRDataSource *) hDataSource)->GetSummaryRefCount();
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDataSource::CreateLayer( const char * pszName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )

{
    (void) eGType;
    (void) poSpatialRef;
    (void) pszName;
    (void) papszOptions;

    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateLayer() not supported by this data source." );
              
    return NULL;
}

/************************************************************************/
/*                         OGR_DS_CreateLayer()                         */
/************************************************************************/

OGRLayerH OGR_DS_CreateLayer( OGRDataSourceH hDS, 
                              const char * pszName,
                              OGRSpatialReferenceH hSpatialRef,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_CreateLayer", NULL );

    if (pszName == NULL)
    {
        CPLError ( CE_Failure, CPLE_ObjectNull, "Name was NULL in OGR_DS_CreateLayer");
        return 0;
    }
    return (OGRLayerH) ((OGRDataSource *)hDS)->CreateLayer( 
        pszName, (OGRSpatialReference *) hSpatialRef, eType, papszOptions );
}

/************************************************************************/
/*                             CopyLayer()                              */
/************************************************************************/

OGRLayer *OGRDataSource::CopyLayer( OGRLayer *poSrcLayer, 
                                    const char *pszNewName, 
                                    char **papszOptions )

{
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();
    OGRLayer *poDstLayer = NULL;

/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    if( !TestCapability( ODsCCreateLayer ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "This datasource does not support creation of layers." );
        return NULL;
    }

    CPLErrorReset();
    poDstLayer = CreateLayer( pszNewName, poSrcLayer->GetSpatialRef(),
                              poSrcDefn->GetGeomType(), papszOptions );
    
    if( poDstLayer == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all field.                         */
/*      If only a subset of all fields requested, then output only      */
/*      the selected fields, and in the order that they were            */
/*      selected.                                                       */
/* -------------------------------------------------------------------- */
    int         iField;
    
    for( iField = 0; iField < poSrcDefn->GetFieldCount(); iField++ )
        poDstLayer->CreateField( poSrcDefn->GetFieldDefn(iField) );

/* -------------------------------------------------------------------- */
/*      Check if the destination layer supports transactions and set a  */
/*      default number of features in a single transaction.             */
/* -------------------------------------------------------------------- */
    int nGroupTransactions = 0;
    if( poDstLayer->TestCapability( OLCTransactions ) )
        nGroupTransactions = 128;

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;

    poSrcLayer->ResetReading();

    if( nGroupTransactions <= 0 )
    {
      while( TRUE )
      {
        OGRFeature      *poDstFeature = NULL;

        poFeature = poSrcLayer->GetNextFeature();
        
        if( poFeature == NULL )
            break;

        CPLErrorReset();
        poDstFeature = OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

        if( poDstFeature->SetFrom( poFeature, TRUE ) != OGRERR_NONE )
        {
            delete poFeature;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to translate feature %ld from layer %s.\n",
                      poFeature->GetFID(), poSrcDefn->GetName() );
            return poDstLayer;
        }

        poDstFeature->SetFID( poFeature->GetFID() );

        OGRFeature::DestroyFeature( poFeature );

        CPLErrorReset();
        if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE )
        {
            OGRFeature::DestroyFeature( poDstFeature );
            return poDstLayer;
        }

        OGRFeature::DestroyFeature( poDstFeature );
      }
    }
    else
    {
      int i, bStopTransfer = FALSE, bStopTransaction = FALSE;
      int nFeatCount = 0; // Number of features in the temporary array
      int nFeaturesToAdd = 0;
      OGRFeature **papoDstFeature =
          (OGRFeature **)CPLCalloc(sizeof(OGRFeature *), nGroupTransactions);
      while( !bStopTransfer )
      {
/* -------------------------------------------------------------------- */
/*      Fill the array with features                                    */
/* -------------------------------------------------------------------- */
        for( nFeatCount = 0; nFeatCount < nGroupTransactions; nFeatCount++ )
        {
            poFeature = poSrcLayer->GetNextFeature();

            if( poFeature == NULL )
            {
                bStopTransfer = 1;
                break;
            }

            CPLErrorReset();
            papoDstFeature[nFeatCount] =
                        OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

            if( papoDstFeature[nFeatCount]->SetFrom( poFeature, TRUE ) != OGRERR_NONE )
            {
                OGRFeature::DestroyFeature( poFeature );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to translate feature %ld from layer %s.\n",
                          poFeature->GetFID(), poSrcDefn->GetName() );
                bStopTransfer = TRUE;
                break;
            }

            papoDstFeature[nFeatCount]->SetFID( poFeature->GetFID() );

            OGRFeature::DestroyFeature( poFeature );
        }
        nFeaturesToAdd = nFeatCount;

        CPLErrorReset();
        bStopTransaction = FALSE;
        while( !bStopTransaction )
        {
            bStopTransaction = TRUE;
            poDstLayer->StartTransaction();
            for( i = 0; i < nFeaturesToAdd; i++ )
            {
                if( poDstLayer->CreateFeature( papoDstFeature[i] ) != OGRERR_NONE )
                {
                    nFeaturesToAdd = i;
                    bStopTransfer = TRUE;
                    bStopTransaction = FALSE;
                }
            }
            if( bStopTransaction )
                poDstLayer->CommitTransaction();
            else
                poDstLayer->RollbackTransaction();
        }

        for( i = 0; i < nFeatCount; i++ )
            OGRFeature::DestroyFeature( papoDstFeature[i] );
      }
      CPLFree(papoDstFeature);
    }
    return poDstLayer;
}

/************************************************************************/
/*                          OGR_DS_CopyLayer()                          */
/************************************************************************/

OGRLayerH OGR_DS_CopyLayer( OGRDataSourceH hDS, 
                            OGRLayerH hSrcLayer, const char *pszNewName,
                            char **papszOptions )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_CopyLayer", NULL );
    VALIDATE_POINTER1( hSrcLayer, "OGR_DS_CopyLayer", NULL );

    return (OGRLayerH) 
        ((OGRDataSource *) hDS)->CopyLayer( (OGRLayer *) hSrcLayer, 
                                            pszNewName, papszOptions );
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRDataSource::DeleteLayer( int iLayer )

{
    (void) iLayer;
    CPLError( CE_Failure, CPLE_NotSupported,
              "DeleteLayer() not supported by this data source." );
              
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                         OGR_DS_DeleteLayer()                         */
/************************************************************************/

OGRErr OGR_DS_DeleteLayer( OGRDataSourceH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_DeleteLayer", OGRERR_INVALID_HANDLE );

    return ((OGRDataSource *) hDS)->DeleteLayer( iLayer );
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRDataSource::GetLayerByName( const char *pszName )

{
    CPLMutexHolderD( &m_hMutex );

    if ( ! pszName )
        return NULL;

    int  i;

    /* first a case sensitive check */
    for( i = 0; i < GetLayerCount(); i++ )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( strcmp( pszName, poLayer->GetLayerDefn()->GetName() ) == 0 )
            return poLayer;
    }

    /* then case insensitive */
    for( i = 0; i < GetLayerCount(); i++ )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( EQUAL( pszName, poLayer->GetLayerDefn()->GetName() ) )
            return poLayer;
    }

    return NULL;
}

/************************************************************************/
/*                       OGR_DS_GetLayerByName()                        */
/************************************************************************/

OGRLayerH OGR_DS_GetLayerByName( OGRDataSourceH hDS, const char *pszName )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayerByName", NULL );

    return (OGRLayerH) ((OGRDataSource *) hDS)->GetLayerByName( pszName );
}

/************************************************************************/
/*                       ProcessSQLCreateIndex()                        */
/*                                                                      */
/*      The correct syntax for creating an index in our dialect of      */
/*      SQL is:                                                         */
/*                                                                      */
/*        CREATE INDEX ON <layername> USING <columnname>                */
/************************************************************************/

OGRErr OGRDataSource::ProcessSQLCreateIndex( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) != 6 
        || !EQUAL(papszTokens[0],"CREATE")
        || !EQUAL(papszTokens[1],"INDEX")
        || !EQUAL(papszTokens[2],"ON")
        || !EQUAL(papszTokens[4],"USING") )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in CREATE INDEX command.\n"
                  "Was '%s'\n"
                  "Should be of form 'CREATE INDEX ON <table> USING <field>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    int  i;
    OGRLayer *poLayer = NULL;

    {
        CPLMutexHolderD( &m_hMutex );

        for( i = 0; i < GetLayerCount(); i++ )
        {
            poLayer = GetLayer(i);
            
            if( EQUAL(poLayer->GetLayerDefn()->GetName(),papszTokens[3]) )
                break;
        }
        
        if( i >= GetLayerCount() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "CREATE INDEX ON failed, no such layer as `%s'.",
                      papszTokens[3] );
            CSLDestroy( papszTokens );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CREATE INDEX ON not supported by this driver." );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named field.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy( papszTokens );

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "`%s' failed, field not found.",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to create the index.                                    */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    eErr = poLayer->GetIndex()->CreateIndex( i );
    if( eErr == OGRERR_NONE )
        eErr = poLayer->GetIndex()->IndexAllFeatures( i );
    else
    {
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Cannot '%s'", pszSQLCommand);
    }

    return eErr;
}

/************************************************************************/
/*                        ProcessSQLDropIndex()                         */
/*                                                                      */
/*      The correct syntax for droping one or more indexes in           */
/*      the OGR SQL dialect is:                                         */
/*                                                                      */
/*          DROP INDEX ON <layername> [USING <columnname>]              */
/************************************************************************/

OGRErr OGRDataSource::ProcessSQLDropIndex( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( (CSLCount(papszTokens) != 4 && CSLCount(papszTokens) != 6)
        || !EQUAL(papszTokens[0],"DROP")
        || !EQUAL(papszTokens[1],"INDEX")
        || !EQUAL(papszTokens[2],"ON") 
        || (CSLCount(papszTokens) == 6 && !EQUAL(papszTokens[4],"USING")) )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in DROP INDEX command.\n"
                  "Was '%s'\n"
                  "Should be of form 'DROP INDEX ON <table> [USING <field>]'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    int  i;
    OGRLayer *poLayer=NULL;

    {
        CPLMutexHolderD( &m_hMutex );

        for( i = 0; i < GetLayerCount(); i++ )
        {
            poLayer = GetLayer(i);
        
            if( EQUAL(poLayer->GetLayerDefn()->GetName(),papszTokens[3]) )
                break;
        }

        if( i >= GetLayerCount() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "CREATE INDEX ON failed, no such layer as `%s'.",
                      papszTokens[3] );
            CSLDestroy( papszTokens );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Indexes not supported by this driver." );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      If we weren't given a field name, drop all indexes.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    if( CSLCount(papszTokens) == 4 )
    {
        for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
        {
            OGRAttrIndex *poAttrIndex;

            poAttrIndex = poLayer->GetIndex()->GetFieldIndex(i);
            if( poAttrIndex != NULL )
            {
                eErr = poLayer->GetIndex()->DropIndex( i );
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
        }

        CSLDestroy(papszTokens);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named field.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy( papszTokens );

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "`%s' failed, field not found.",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to drop the index.                                      */
/* -------------------------------------------------------------------- */
    eErr = poLayer->GetIndex()->DropIndex( i );

    return eErr;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRDataSource::ExecuteSQL( const char *pszStatement,
                                      OGRGeometry *poSpatialFilter,
                                      const char *pszDialect )

{
    const char *pszError;
    swq_select *psSelectInfo = NULL;

    (void) pszDialect;

    swq_field_list sFieldList;
    int            nFIDIndex = 0;
    OGRGenSQLResultsLayer *poResults = NULL;

    memset( &sFieldList, 0, sizeof(sFieldList) );

/* -------------------------------------------------------------------- */
/*      Handle CREATE INDEX statements specially.                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"CREATE INDEX",12) )
    {
        ProcessSQLCreateIndex( pszStatement );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Handle DROP INDEX statements specially.                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"DROP INDEX",10) )
    {
        ProcessSQLDropIndex( pszStatement );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Preparse the SQL statement.                                     */
/* -------------------------------------------------------------------- */
    pszError = swq_select_preparse( pszStatement, &psSelectInfo );
    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Validate that all the source tables are recognised, count       */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    int  nFieldCount = 0, iTable, iField;
    int  iEDS;
    int  nExtraDSCount = 0;
    OGRDataSource** papoExtraDS = NULL;
    OGRSFDriverRegistrar *poReg=OGRSFDriverRegistrar::GetRegistrar();

    for( iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        OGRLayer *poSrcLayer;
        OGRDataSource *poTableDS = this;

        if( psTableDef->data_source != NULL )
        {
            poTableDS = (OGRDataSource *) 
                OGROpenShared( psTableDef->data_source, FALSE, NULL );
            if( poTableDS == NULL )
            {
                if( strlen(CPLGetLastErrorMsg()) == 0 )
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Unable to open secondary datasource\n"
                              "`%s' required by JOIN.",
                              psTableDef->data_source );

                swq_select_free( psSelectInfo );
                goto end;
            }

            /* Keep in an array to release at the end of this function */
            papoExtraDS = (OGRDataSource** )CPLRealloc(papoExtraDS,
                               sizeof(OGRDataSource*) * (nExtraDSCount + 1));
            papoExtraDS[nExtraDSCount++] = poTableDS;
        }

        poSrcLayer = poTableDS->GetLayerByName( psTableDef->table_name );

        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "SELECT from table %s failed, no such table/featureclass.",
                      psTableDef->table_name );
            swq_select_free( psSelectInfo );
            goto end;
        }

        nFieldCount += poSrcLayer->GetLayerDefn()->GetFieldCount();
    }
    
/* -------------------------------------------------------------------- */
/*      Build the field list for all indicated tables.                  */
/* -------------------------------------------------------------------- */

    sFieldList.table_count = psSelectInfo->table_count;
    sFieldList.table_defs = psSelectInfo->table_defs;

    sFieldList.count = 0;
    sFieldList.names = (char **) CPLMalloc( sizeof(char *) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    sFieldList.types = (swq_field_type *)  
        CPLMalloc( sizeof(swq_field_type) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    sFieldList.table_ids = (int *) 
        CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    sFieldList.ids = (int *) 
        CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    
    for( iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        OGRDataSource *poTableDS = this;
        OGRLayer *poSrcLayer;
        
        if( psTableDef->data_source != NULL )
        {
            poTableDS = (OGRDataSource *) 
                OGROpenShared( psTableDef->data_source, FALSE, NULL );
            CPLAssert( poTableDS != NULL );
            poTableDS->Dereference();
        }

        poSrcLayer = poTableDS->GetLayerByName( psTableDef->table_name );

        for( iField = 0; 
             iField < poSrcLayer->GetLayerDefn()->GetFieldCount();
             iField++ )
        {
            OGRFieldDefn *poFDefn=poSrcLayer->GetLayerDefn()->GetFieldDefn(iField);
            int iOutField = sFieldList.count++;
            sFieldList.names[iOutField] = (char *) poFDefn->GetNameRef();
            if( poFDefn->GetType() == OFTInteger )
                sFieldList.types[iOutField] = SWQ_INTEGER;
            else if( poFDefn->GetType() == OFTReal )
                sFieldList.types[iOutField] = SWQ_FLOAT;
            else if( poFDefn->GetType() == OFTString )
                sFieldList.types[iOutField] = SWQ_STRING;
            else
                sFieldList.types[iOutField] = SWQ_OTHER;

            sFieldList.table_ids[iOutField] = iTable;
            sFieldList.ids[iOutField] = iField;
        }

        if( iTable == 0 )
            nFIDIndex = poSrcLayer->GetLayerDefn()->GetFieldCount();
    }

/* -------------------------------------------------------------------- */
/*      Expand '*' in 'SELECT *' now before we add the pseudo fields    */
/* -------------------------------------------------------------------- */
    pszError = 
        swq_select_expand_wildcard( psSelectInfo, &sFieldList );

    if( pszError != NULL )
    {
        swq_select_free( psSelectInfo );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        goto end;
    }

    for (iField = 0; iField < SPECIAL_FIELD_COUNT; iField++)
    {
        sFieldList.names[sFieldList.count] = (char*) SpecialFieldNames[iField];
        sFieldList.types[sFieldList.count] = SpecialFieldTypes[iField];
        sFieldList.table_ids[sFieldList.count] = 0;
        sFieldList.ids[sFieldList.count] = nFIDIndex + iField;
        sFieldList.count++;
    }
    
/* -------------------------------------------------------------------- */
/*      Finish the parse operation.                                     */
/* -------------------------------------------------------------------- */
    
    pszError = swq_select_parse( psSelectInfo, &sFieldList, 0 );

    if( pszError != NULL )
    {
        swq_select_free( psSelectInfo );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Everything seems OK, try to instantiate a results layer.        */
/* -------------------------------------------------------------------- */

    poResults = new OGRGenSQLResultsLayer( this, psSelectInfo, 
                                           poSpatialFilter );

    // Eventually, we should keep track of layers to cleanup.

end:
    CPLFree( sFieldList.names );
    CPLFree( sFieldList.types );
    CPLFree( sFieldList.table_ids );
    CPLFree( sFieldList.ids );

    /* Release the datasets we have opened with OGROpenShared() */
    /* It is safe to do that as the 'new OGRGenSQLResultsLayer' itself */
    /* has taken a reference on them, which it will release in its */
    /* destructor */
    for(iEDS = 0; iEDS < nExtraDSCount; iEDS++)
        poReg->ReleaseDataSource( papoExtraDS[iEDS] );
    CPLFree(papoExtraDS);

    return poResults;
}

/************************************************************************/
/*                         OGR_DS_ExecuteSQL()                          */
/************************************************************************/

OGRLayerH OGR_DS_ExecuteSQL( OGRDataSourceH hDS, 
                             const char *pszStatement,
                             OGRGeometryH hSpatialFilter,
                             const char *pszDialect )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_ExecuteSQL", NULL );

    return (OGRLayerH) 
        ((OGRDataSource *)hDS)->ExecuteSQL( pszStatement,
                                            (OGRGeometry *) hSpatialFilter,
                                            pszDialect );
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRDataSource::ReleaseResultSet( OGRLayer * poResultsSet )

{
    delete poResultsSet;
}

/************************************************************************/
/*                      OGR_DS_ReleaseResultSet()                       */
/************************************************************************/

void OGR_DS_ReleaseResultSet( OGRDataSourceH hDS, OGRLayerH hLayer )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_ReleaseResultSet" );

    ((OGRDataSource *) hDS)->ReleaseResultSet( (OGRLayer *) hLayer );
}

/************************************************************************/
/*                       OGR_DS_TestCapability()                        */
/************************************************************************/

int OGR_DS_TestCapability( OGRDataSourceH hDS, const char *pszCap )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_TestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "OGR_DS_TestCapability", 0 );

    return ((OGRDataSource *) hDS)->TestCapability( pszCap );
}

/************************************************************************/
/*                        OGR_DS_GetLayerCount()                        */
/************************************************************************/

int OGR_DS_GetLayerCount( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayerCount", 0 );

    return ((OGRDataSource *)hDS)->GetLayerCount();
}

/************************************************************************/
/*                          OGR_DS_GetLayer()                           */
/************************************************************************/

OGRLayerH OGR_DS_GetLayer( OGRDataSourceH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetLayer", NULL );

    return (OGRLayerH) ((OGRDataSource*)hDS)->GetLayer( iLayer );
}

/************************************************************************/
/*                           OGR_DS_GetName()                           */
/************************************************************************/

const char *OGR_DS_GetName( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetName", NULL );

    return ((OGRDataSource*)hDS)->GetName();
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRDataSource::SyncToDisk()

{
    CPLMutexHolderD( &m_hMutex );
    int i;
    OGRErr eErr;

    for( i = 0; i < GetLayerCount(); i++ )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( poLayer )
        {
            eErr = poLayer->SyncToDisk();
            if( eErr != OGRERR_NONE )
                return eErr;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OGR_DS_SyncToDisk()                          */
/************************************************************************/

OGRErr OGR_DS_SyncToDisk( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_SyncToDisk", OGRERR_INVALID_HANDLE );

    return ((OGRDataSource *) hDS)->SyncToDisk();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

OGRSFDriver *OGRDataSource::GetDriver() const

{
    return m_poDriver;
}

/************************************************************************/
/*                          OGR_DS_GetDriver()                          */
/************************************************************************/

OGRSFDriverH OGR_DS_GetDriver( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetDriver", NULL );

    return (OGRSFDriverH) ((OGRDataSource *) hDS)->GetDriver();
}

/************************************************************************/
/*                             SetDriver()                              */
/************************************************************************/

void OGRDataSource::SetDriver( OGRSFDriver *poDriver ) 

{
    m_poDriver = poDriver;
}

/************************************************************************/
/*                            GetStyleTable()                           */
/************************************************************************/

OGRStyleTable *OGRDataSource::GetStyleTable()
{
    return m_poStyleTable;
}

/************************************************************************/
/*                         SetStyleTableDirectly()                      */
/************************************************************************/

void OGRDataSource::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable;
}

/************************************************************************/
/*                            SetStyleTable()                           */
/************************************************************************/

void OGRDataSource::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    if ( poStyleTable )
        m_poStyleTable = poStyleTable->Clone();
}

/************************************************************************/
/*                         OGR_DS_GetStyleTable()                       */
/************************************************************************/

OGRStyleTableH OGR_DS_GetStyleTable( OGRDataSourceH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetStyleTable", NULL );
    
    return (OGRStyleTableH) ((OGRDataSource *) hDS)->GetStyleTable( );
}

/************************************************************************/
/*                         OGR_DS_SetStyleTableDirectly()               */
/************************************************************************/

void OGR_DS_SetStyleTableDirectly( OGRDataSourceH hDS,
                                   OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTableDirectly" );
    
    ((OGRDataSource *) hDS)->SetStyleTableDirectly( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                         OGR_DS_SetStyleTable()                       */
/************************************************************************/

void OGR_DS_SetStyleTable( OGRDataSourceH hDS, OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_DS_SetStyleTable" );
    
    ((OGRDataSource *) hDS)->SetStyleTable( (OGRStyleTable *) hStyleTable);
}
