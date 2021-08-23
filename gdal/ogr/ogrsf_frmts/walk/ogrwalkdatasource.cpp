/******************************************************************************
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRWalkDataSource()                          */
/************************************************************************/

OGRWalkDataSource::OGRWalkDataSource() :
    pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0)
{}

/************************************************************************/
/*                        ~OGRWalkDataSource()                          */
/************************************************************************/

OGRWalkDataSource::~OGRWalkDataSource()

{
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
    {
        CPLAssert( nullptr != papoLayers[i] );
        delete papoLayers[i];
    }

    CPLFree( papoLayers );
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

int OGRWalkDataSource::Open( const char * pszNewName, int /* bUpdate */ )
{
/* -------------------------------------------------------------------- */
/*      If this is the name of an MDB file, then construct the          */
/*      appropriate connection string.  Otherwise clip of WALK: to      */
/*      get the DSN.                                                    */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszNewName, "WALK:") )
    {
        char *pszDSN = CPLStrdup( pszNewName + 5 );
        CPLDebug( "Walk", "EstablishSession(%s)", pszDSN );
        if( !oSession.EstablishSession( pszDSN, nullptr, nullptr ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to initialize ODBC connection to DSN for %s,\n"
                      "%s", pszDSN, oSession.GetLastError() );
            CPLFree( pszDSN );
            return FALSE;
        }
    }
    else
    {
        if ( !oSession.ConnectToMsAccess( pszNewName, nullptr ) )
        {
           return FALSE;
        }
    }

    // check for WalkLayers table
    {
        bool bFoundWalkLayersTable = false;
        CPLODBCStatement oTableList( &oSession );
        if( oTableList.GetTables() )
        {
            while( oTableList.Fetch() )
            {
                const CPLString osTableName = CPLString( oTableList.GetColData(2, "") );
                const CPLString osLCTableName(CPLString(osTableName).tolower());
                if( osLCTableName == "walklayers" )
                {
                    bFoundWalkLayersTable = true;
                    break;
                }
            }
        }
        if (!bFoundWalkLayersTable )
            return FALSE;
    }

    pszName = CPLStrdup( pszNewName );

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
        int i, iNew = static_cast<int>(apapszGeomColumns.size());
        char **papszRecord = nullptr;

        for( i = 1; i < 7; i++ )
            papszRecord = CSLAddString( papszRecord, oStmt.GetColData(i) ); //Add LayerName, Extent and Memo

        apapszGeomColumns.resize(iNew+1);
        apapszGeomColumns[iNew] = papszRecord;
    }

/* -------------------------------------------------------------------- */
/*      Create a layer for each spatial table.                          */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRWalkLayer **) CPLCalloc(apapszGeomColumns.size(),
                                             sizeof( void * ));

    for( unsigned int iTable = 0; iTable < apapszGeomColumns.size(); iTable++ )
    {
        char **papszRecord = apapszGeomColumns[iTable];

        OGRWalkTableLayer  *poLayer = new OGRWalkTableLayer( this );

        if( poLayer->Initialize( papszRecord[0],        // LayerName
                                 "Geometry",            // Geometry Column Name
                                 CPLAtof(papszRecord[1]),  // Extent MinE
                                 CPLAtof(papszRecord[2]),  // Extent MaxE
                                 CPLAtof(papszRecord[3]),  // Extent MinN
                                 CPLAtof(papszRecord[4]),  // Extent MaxN
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
        return nullptr;
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
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
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
        delete poStmt;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Are there result columns for this statement?                    */
/* -------------------------------------------------------------------- */
    if( poStmt->GetColCount() == 0 )
    {
        delete poStmt;
        CPLErrorReset();
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a results layer.  It will take ownership of the          */
/*      statement.                                                      */
/* -------------------------------------------------------------------- */
    OGRWalkSelectLayer *poLayer = new OGRWalkSelectLayer( this, poStmt );

    if( poSpatialFilter != nullptr )
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
