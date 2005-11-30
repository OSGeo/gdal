/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSDEDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.3  2005/11/30 01:25:58  fwarmerdam
 * removed FetchSRSId stuff
 *
 * Revision 1.2  2005/11/25 05:58:27  fwarmerdam
 * preliminary operation of feature reading
 *
 * Revision 1.1  2005/11/22 17:01:48  fwarmerdam
 * New
 *
 */

#include "ogr_sde.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRSDEDataSource()                           */
/************************************************************************/

OGRSDEDataSource::OGRSDEDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    hConnection = NULL;
}

/************************************************************************/
/*                          ~OGRSDEDataSource()                          */
/************************************************************************/

OGRSDEDataSource::~OGRSDEDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    if( hConnection != NULL )
    {
        SE_connection_free( hConnection );
    }
}

/************************************************************************/
/*                           IssueSDEError()                            */
/************************************************************************/

void OGRSDEDataSource::IssueSDEError( int nErrorCode, 
                                      const char *pszFunction )

{
    char szErrorMsg[SE_MAX_MESSAGE_LENGTH+1];

    if( pszFunction == NULL )
        pszFunction = "SDE";

    SE_error_get_string( nErrorCode, szErrorMsg );

    CPLError( CE_Failure, CPLE_AppDefined, 
              "%s: %d/%s", 
              pszFunction, nErrorCode, szErrorMsg );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSDEDataSource::Open( const char * pszNewName )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      If we aren't prefixed with SDE: then ignore this datasource.    */
/* -------------------------------------------------------------------- */
    if( !EQUALN(pszNewName,"SDE:",4) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Parse arguments on comma.  We expect:                           */
/*        SDE:server,instance,database,username,password                */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeStringComplex( pszNewName+4, ",",
                                                   TRUE, TRUE );

    if( CSLCount( papszTokens ) != 5 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "SDE connect string had wrong number of arguments.\n"
                  "Expected 'SDE:server,instance,database,username,password'\n"
                  "Got '%s'", 
                  pszNewName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    int 	nSDEErr;
    SE_ERROR    sSDEErrorInfo;

    nSDEErr = SE_connection_create( papszTokens[0], 
                                    papszTokens[1], 
                                    papszTokens[2], 
                                    papszTokens[3],
                                    papszTokens[4],
                                    &sSDEErrorInfo, &hConnection );

    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_connection_create" );
        return FALSE;
    }

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Set unprotected concurrency policy, suitable for single         */
/*      threaded access.                                                */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_connection_set_concurrency( hConnection,
                                             SE_UNPROTECTED_POLICY);

    if( nSDEErr != SE_SUCCESS) {
        IssueSDEError( nSDEErr, NULL );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch list of spatial tables from SDE.                          */
/* -------------------------------------------------------------------- */
    SE_REGINFO *ahTableList;
    LONG nTableListCount;

    nSDEErr = SE_registration_get_info_list( hConnection, &ahTableList,
                                             &nTableListCount );
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_registration_get_info_list" );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Process the tables, turning any appropriate ones into layers.   */
/* -------------------------------------------------------------------- */
    int iTable;
    LONG nFIDColType;

    for( iTable = 0; iTable < nTableListCount; iTable++ )
    {
        char szTableName[SE_QUALIFIED_TABLE_NAME+1];
        char szIDColName[SE_MAX_COLUMN_LEN+1];

        // Ignore non-spatial, or hidden tables. 
        if( !SE_reginfo_has_layer( ahTableList[iTable] ) 
            || SE_reginfo_is_hidden( ahTableList[iTable] ) )
            continue;

        nSDEErr = SE_reginfo_get_table_name( ahTableList[iTable], 
                                            szTableName );
        if( nSDEErr != SE_SUCCESS )
            continue;

        nSDEErr = SE_reginfo_get_rowid_column( ahTableList[iTable],
                                               szIDColName, 
                                               &nFIDColType );
        
        if( nFIDColType == SE_REGISTRATION_ROW_ID_COLUMN_TYPE_NONE
            ||strlen(szIDColName) == 0 )
        {
            CPLDebug( "OGR_SDE", "Unable to determine FID column for %s.", 
                      szTableName );
            continue;
        }
        
        OpenTable( szTableName, szIDColName, NULL );
    }
 
    return nLayers > 0;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRSDEDataSource::OpenTable( const char *pszTableName, 
                                 const char *pszFIDColumn,
                                 const char *pszShapeColumn )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSDELayer  *poLayer;

    poLayer = new OGRSDELayer( this );

    if( !poLayer->Initialize( pszTableName, pszFIDColumn, pszShapeColumn ) )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSDELayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSDELayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDEDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSDEDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

