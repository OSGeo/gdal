/******************************************************************************
 * $Id: ogrwalkdatasource.cpp
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWalkDatasource class.
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
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

#include "ogrwalk.h"
#include <vector>

/************************************************************************/
/*                         OGRWalkDataSource()                          */
/************************************************************************/

OGRWalkDataSource::OGRWalkDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
}

/************************************************************************/
/*                        ~OGRWalkDataSource()                          */
/************************************************************************/

OGRWalkDataSource::~OGRWalkDataSource()

{
    int i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
    {
        CPLAssert( NULL != papoLayers[i] );

        delete papoLayers[i];
    }

    CPLFree( papoLayers );
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

int OGRWalkDataSource::Open( const char * pszNewName, int bUpdate )
{
/* -------------------------------------------------------------------- */
/*      If this is the name of an MDB file, then construct the          */
/*      appropriate connection string.  Otherwise clip of WALK: to      */
/*      get the DSN.                                                    */
/* -------------------------------------------------------------------- */
    char *pszDSN;

    if( EQUALN(pszNewName,"WALK:",5) )
        pszDSN = CPLStrdup( pszNewName + 5 );
    else
    {
        const char *pszDSNStringTemplate = "DRIVER=Microsoft Access Driver (*.mdb);DBQ=%s";
        pszDSN = (char *) CPLMalloc(strlen(pszNewName)+strlen(pszDSNStringTemplate)+100);

        sprintf( pszDSN, pszDSNStringTemplate,  pszNewName );
    }

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "Walk", "EstablishSession(%s)", pszDSN );

    if( !oSession.EstablishSession( pszDSN, NULL, NULL ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to initialize ODBC connection to DSN for %s,\n"
                  "%s", pszDSN, oSession.GetLastError() );
        CPLFree( pszDSN );
        return FALSE;
    }

    CPLFree( pszDSN );

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Collect list of layers and their attributes.                    */
/* -------------------------------------------------------------------- */
    std::vector<char **> apapszGeomColumns;
    CPLODBCStatement oStmt( &oSession );

    oStmt.Append( "SELECT LayerID, LayerName, minE, maxE, minN, maxN, Memo  FROM WalkLayers" );

    if( !oStmt.ExecuteSQL() )
    {
        CPLDebug( "Walk", 
                  "SELECT on WalkLayers fails, perhaps not a walk database?\n%s", 
                  oSession.GetLastError() );
        return FALSE;
    }

    while( oStmt.Fetch() )
    {
        int i, iNew = apapszGeomColumns.size();
        char **papszRecord = NULL;
        
        for( i = 1; i < 7; i++ )
            papszRecord = CSLAddString( papszRecord, oStmt.GetColData(i) ); //Add LayerName, Extent and Memo

        apapszGeomColumns.resize(iNew+1);
        apapszGeomColumns[iNew] = papszRecord;
    }

/* -------------------------------------------------------------------- */
/*      Create a layer for each spatial table.                          */
/* -------------------------------------------------------------------- */
    unsigned int iTable;

    papoLayers = (OGRWalkLayer **) CPLCalloc(apapszGeomColumns.size(),
                                             sizeof( void * ));

    for( iTable = 0; iTable < apapszGeomColumns.size(); iTable++ )
    {
        char **papszRecord = apapszGeomColumns[iTable];

        OGRWalkTableLayer  *poLayer = new OGRWalkTableLayer( this );

        if( poLayer->Initialize( papszRecord[0],        // LayerName
                                 "Geometry",            // Geometry Column Name
                                 atof(papszRecord[1]),  // Extent MinE
                                 atof(papszRecord[2]),  // Extent MaxE
                                 atof(papszRecord[3]),  // Extent MinN
                                 atof(papszRecord[4]),  // Extent MaxN
                                 papszRecord[5])        // Memo for SpatialRef
            != CE_None )
        {
            delete poLayer;
        }
        else
            papoLayers[nLayers++] = poLayer;

        CSLDestroy( papszRecord );
    }

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRWalkDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRWalkDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic imlplementation for OGRSQL dialect.                 */
/* -------------------------------------------------------------------- */
    if( pszDialect == NULL || EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Execute normal SQL statement in Walk.                           */
/*      Table_name = Layer_name + Postfix                               */
/*      Postfix: "Features", "Annotations" or "Styles"                  */
/* -------------------------------------------------------------------- */
    CPLODBCStatement *poStmt = new CPLODBCStatement( &oSession );

    CPLDebug( "Walk", "ExecuteSQL(%s) called.", pszSQLCommand );
    poStmt->Append( pszSQLCommand );
    if( !poStmt->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", oSession.GetLastError() );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poStmt->GetColCount() == 0 )
    {
        delete poStmt;
        CPLErrorReset();
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */
    OGRWalkSelectLayer *poLayer = NULL;

    poLayer = new OGRWalkSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRWalkDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
