/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_pgeo.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRPGeoDataSource()                          */
/************************************************************************/

OGRPGeoDataSource::OGRPGeoDataSource() :
    papoLayers(NULL),
    nLayers(0),
    pszName(NULL),
    bDSUpdate(FALSE)
{ }

/************************************************************************/
/*                         ~OGRPGeoDataSource()                         */
/************************************************************************/

OGRPGeoDataSource::~OGRPGeoDataSource()

{
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );
}

/************************************************************************/
/*                  CheckDSNStringTemplate()                            */
/* The string will be used as the formatting argument of sprintf with   */
/* a string in vararg. So let's check there's only one '%s', and nothing*/
/* else                                                                 */
/************************************************************************/

static int CheckDSNStringTemplate(const char* pszStr)
{
    int nPercentSFound = FALSE;
    while(*pszStr)
    {
        if (*pszStr == '%')
        {
            if (pszStr[1] != 's')
            {
                return FALSE;
            }
            else
            {
                if (nPercentSFound)
                    return FALSE;
                nPercentSFound = TRUE;
            }
        }
        pszStr ++;
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPGeoDataSource::Open( const char * pszNewName, int bUpdate,
                             CPL_UNUSED int bTestOpen )
{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      If this is the name of an MDB file, then construct the          */
/*      appropriate connection string.  Otherwise clip of PGEO: to      */
/*      get the DSN.                                                    */
/*                                                                      */
/* -------------------------------------------------------------------- */
    char *pszDSN;
    const char* pszOptionName = "";
    const char* pszDSNStringTemplate = NULL;
    if( STARTS_WITH_CI(pszNewName, "PGEO:") )
        pszDSN = CPLStrdup( pszNewName + 5 );
    else
    {
        pszOptionName = "PGEO_DRIVER_TEMPLATE";
        pszDSNStringTemplate = CPLGetConfigOption( pszOptionName, NULL );
        if( pszDSNStringTemplate == NULL )
        {
            pszOptionName = "";
            pszDSNStringTemplate = "DRIVER=Microsoft Access Driver (*.mdb);DBQ=%s";
        }
        if (!CheckDSNStringTemplate(pszDSNStringTemplate))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal value for PGEO_DRIVER_TEMPLATE option");
            return FALSE;
        }
        pszDSN = (char *) CPLMalloc(strlen(pszNewName)+strlen(pszDSNStringTemplate)+100);
        snprintf( pszDSN,
                  strlen(pszNewName)+strlen(pszDSNStringTemplate)+100,
                  pszDSNStringTemplate,  pszNewName );
    }

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "PGeo", "EstablishSession(%s)", pszDSN );

    if( !oSession.EstablishSession( pszDSN, NULL, NULL ) )
    {
        int bError = TRUE;
        if( !STARTS_WITH_CI(pszNewName, "PGEO:") )
        {
            // Trying with another template (#5594)
            pszDSNStringTemplate = "DRIVER=Microsoft Access Driver (*.mdb, *.accdb);DBQ=%s";
            CPLFree( pszDSN );
            pszDSN = (char *) CPLMalloc(strlen(pszNewName)+strlen(pszDSNStringTemplate)+100);
            snprintf( pszDSN,
                     strlen(pszNewName)+strlen(pszDSNStringTemplate)+100,
                     pszDSNStringTemplate,  pszNewName );
            CPLDebug( "PGeo", "EstablishSession(%s)", pszDSN );
            if( oSession.EstablishSession( pszDSN, NULL, NULL ) )
            {
                bError = FALSE;
            }
        }
        if( bError )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to initialize ODBC connection to DSN for %s,\n"
                    "%s", pszDSN, oSession.GetLastError() );
            CPLFree( pszDSN );
            return FALSE;
        }
    }

    CPLFree( pszDSN );

    pszName = CPLStrdup( pszNewName );
    
    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Collect list of tables and their supporting info from           */
/*      GDB_GeomColumns.                                                */
/* -------------------------------------------------------------------- */
    std::vector<char **> apapszGeomColumns;
    CPLODBCStatement oStmt( &oSession );
        
    oStmt.Append( "SELECT TableName, FieldName, ShapeType, ExtentLeft, ExtentRight, ExtentBottom, ExtentTop, SRID, HasZ FROM GDB_GeomColumns" );

    if( !oStmt.ExecuteSQL() )
    {
        CPLDebug( "PGEO", 
                  "SELECT on GDB_GeomColumns fails, perhaps not a personal geodatabase?\n%s", 
                  oSession.GetLastError() );
        return FALSE;
    }

    while( oStmt.Fetch() )
    {
        int i, iNew = static_cast<int>(apapszGeomColumns.size());
        char **papszRecord = NULL;
        for( i = 0; i < 9; i++ )
            papszRecord = CSLAddString( papszRecord, 
                                        oStmt.GetColData(i) );
        apapszGeomColumns.resize(iNew+1);
        apapszGeomColumns[iNew] = papszRecord;
    }
            
/* -------------------------------------------------------------------- */
/*      Create a layer for each spatial table.                          */
/* -------------------------------------------------------------------- */
    unsigned int iTable;

    papoLayers = (OGRPGeoLayer **) CPLCalloc(apapszGeomColumns.size(),
                                             sizeof(void*));

    for( iTable = 0; iTable < apapszGeomColumns.size(); iTable++ )
    {
        char **papszRecord = apapszGeomColumns[iTable];
        OGRPGeoTableLayer  *poLayer;

        poLayer = new OGRPGeoTableLayer( this );

        if( poLayer->Initialize( papszRecord[0],         // TableName
                                 papszRecord[1],         // FieldName
                                 atoi(papszRecord[2]),   // ShapeType
                                 CPLAtof(papszRecord[3]),   // ExtentLeft
                                 CPLAtof(papszRecord[4]),   // ExtentRight
                                 CPLAtof(papszRecord[5]),   // ExtentBottom
                                 CPLAtof(papszRecord[6]),   // ExtentTop
                                 atoi(papszRecord[7]),   // SRID
                                 atoi(papszRecord[8]))  // HasZ
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
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoDataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGeoDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}


/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRPGeoDataSource::ExecuteSQL( const char *pszSQLCommand,
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
/*      Execute statement.                                              */
/* -------------------------------------------------------------------- */
    CPLODBCStatement *poStmt = new CPLODBCStatement( &oSession );

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
    OGRPGeoSelectLayer *poLayer = NULL;
        
    poLayer = new OGRPGeoSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRPGeoDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
