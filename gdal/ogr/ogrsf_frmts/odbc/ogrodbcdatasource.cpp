/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_odbc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                         OGRODBCDataSource()                          */
/************************************************************************/

OGRODBCDataSource::OGRODBCDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    pszName(nullptr),
    bDSUpdate(FALSE),
    nKnownSRID(0),
    panSRID(nullptr),
    papoSRS(nullptr)
{}

/************************************************************************/
/*                         ~OGRODBCDataSource()                         */
/************************************************************************/

OGRODBCDataSource::~OGRODBCDataSource()

{
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != nullptr )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
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
/*                              OpenMDB()                               */
/************************************************************************/

int OGRODBCDataSource::OpenMDB( const char * pszNewName, int bUpdate )
{
    const char* pszOptionName = "PGEO_DRIVER_TEMPLATE";
    const char* pszDSNStringTemplate = CPLGetConfigOption( pszOptionName, nullptr );
    if( pszDSNStringTemplate == nullptr )
    {
        pszOptionName = "MDB_DRIVER_TEMPLATE";
        pszDSNStringTemplate = CPLGetConfigOption( pszOptionName, nullptr );
        if( pszDSNStringTemplate == nullptr )
        {
            pszOptionName = "";
        }
    }
    if (pszDSNStringTemplate && !CheckDSNStringTemplate(pszDSNStringTemplate))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Illegal value for %s option", pszOptionName );
        return FALSE;
    }

    if ( !oSession.ConnectToMsAccess( pszNewName, pszDSNStringTemplate ) )
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Check if it is a PGeo MDB.                                      */
/* -------------------------------------------------------------------- */
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT TableName, FieldName, ShapeType, ExtentLeft, ExtentRight, ExtentBottom, ExtentTop, SRID, HasZ FROM GDB_GeomColumns" );

        if( oStmt.ExecuteSQL() )
        {
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if it is a Geomedia MDB.                                  */
/* -------------------------------------------------------------------- */
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT TableName FROM GAliasTable WHERE TableType = 'INGRFeatures'" );

        if( oStmt.ExecuteSQL() )
        {
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if it is a Walk MDB.                                      */
/* -------------------------------------------------------------------- */
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT LayerID, LayerName, minE, maxE, minN, maxN, Memo FROM WalkLayers" );

        if( oStmt.ExecuteSQL() )
        {
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return all tables as  non-spatial tables.                       */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oTableList( &oSession );

    if( oTableList.GetTables() )
    {
        while( oTableList.Fetch() )
        {
            const char *pszSchema = oTableList.GetColData(1);
            const char* pszTableName = oTableList.GetColData(2);
            if( pszTableName != nullptr )
            {
                CPLString osLayerName;

                if( pszSchema != nullptr && strlen(pszSchema) > 0 )
                {
                    osLayerName = pszSchema;
                    osLayerName += ".";
                }

                osLayerName += pszTableName;

                OpenTable( osLayerName, nullptr, bUpdate );
            }
        }

        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRODBCDataSource::Open( const char * pszNewName, int bUpdate,
                             CPL_UNUSED int bTestOpen )
{
    CPLAssert( nLayers == 0 );

    if( !STARTS_WITH_CI(pszNewName, "ODBC:") && IsSupportedMsAccessFileExtension(CPLGetExtension(pszNewName)))
        return OpenMDB(pszNewName, bUpdate);

/* -------------------------------------------------------------------- */
/*      Start parsing dataset name from the end of string, fetching     */
/*      the name of spatial reference table and names for SRID and      */
/*      SRTEXT columns first.                                           */
/* -------------------------------------------------------------------- */
    char *pszWrkName = CPLStrdup( pszNewName + 5 ); // Skip the 'ODBC:' part
    char **papszTables = nullptr;
    char **papszGeomCol = nullptr;
    char *pszSRSTableName = nullptr;
    char *pszSRIDCol = nullptr;
    char *pszSRTextCol = nullptr;
    char *pszDelimiter = nullptr;

    if ( (pszDelimiter = strrchr( pszWrkName, ':' )) != nullptr )
    {
        char *pszOBracket = strchr( pszDelimiter + 1, '(' );

        if( strchr(pszDelimiter,'\\') != nullptr
            || strchr(pszDelimiter,'/') != nullptr )
        {
            /*
            ** if there are special tokens then this isn't really
            ** the srs table name, so avoid further processing.
            */
        }
        else if( pszOBracket == nullptr )
        {
            pszSRSTableName = CPLStrdup( pszDelimiter + 1 );
            *pszDelimiter = '\0';
        }
        else
        {
            char *pszCBracket = strchr( pszOBracket, ')' );
            if( pszCBracket != nullptr )
                *pszCBracket = '\0';

            char *pszComma = strchr( pszOBracket, ',' );
            if( pszComma != nullptr )
            {
                *pszComma = '\0';
                pszSRIDCol = CPLStrdup( pszComma + 1 );
            }

            *pszOBracket = '\0';
            pszSRSTableName = CPLStrdup( pszDelimiter + 1 );
            pszSRTextCol = CPLStrdup( pszOBracket + 1 );

            *pszDelimiter = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      Strip off any comma delimited set of tables names to access     */
/*      from the end of the string first.  Also allow an optional       */
/*      bracketed geometry column name after the table name.            */
/* -------------------------------------------------------------------- */
    while( (pszDelimiter = strrchr( pszWrkName, ',' )) != nullptr )
    {
        char *pszOBracket = strstr( pszDelimiter + 1, "(" );
        if( pszOBracket == nullptr )
        {
            papszTables = CSLAddString( papszTables, pszDelimiter + 1 );
            papszGeomCol = CSLAddString( papszGeomCol, "" );
        }
        else
        {
            char *pszCBracket = strstr(pszOBracket,")");

            if( pszCBracket != nullptr )
                *pszCBracket = '\0';

            *pszOBracket = '\0';
            papszTables = CSLAddString( papszTables, pszDelimiter + 1 );
            papszGeomCol = CSLAddString( papszGeomCol, pszOBracket+1 );
        }
        *pszDelimiter = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Split out userid, password and DSN.  The general form is        */
/*      user/password@dsn.  But if there are no @ characters the        */
/*      whole thing is assumed to be a DSN.                             */
/* -------------------------------------------------------------------- */
    char *pszUserid = nullptr;
    char *pszPassword = nullptr;
    char *pszDSN = nullptr;

    if( strstr(pszWrkName,"@") == nullptr )
    {
        pszDSN = CPLStrdup( pszWrkName );
    }
    else
    {

        pszDSN = CPLStrdup(strstr(pszWrkName, "@") + 1);
        if( *pszWrkName == '/' )
        {
            pszPassword = CPLStrdup(pszWrkName + 1);
            char *pszTarget = strstr(pszPassword,"@");
            *pszTarget = '\0';
        }
        else
        {
            pszUserid = CPLStrdup(pszWrkName);
            char *pszTarget = strstr(pszUserid,"@");
            *pszTarget = '\0';

            pszTarget = strstr(pszUserid,"/");
            if( pszTarget != nullptr )
            {
                *pszTarget = '\0';
                pszPassword = CPLStrdup(pszTarget+1);
            }
        }
    }

    CPLFree( pszWrkName );

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_ODBC",
              "EstablishSession(DSN:\"%s\", userid:\"%s\", password:\"%s\")",
              pszDSN, pszUserid ? pszUserid : "",
              pszPassword ? pszPassword : "" );

    if( !oSession.EstablishSession( pszDSN, pszUserid, pszPassword ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to initialize ODBC connection to DSN for %s,\n"
                  "%s",
                  pszNewName+5, oSession.GetLastError() );
        CSLDestroy( papszTables );
        CSLDestroy( papszGeomCol );
        CPLFree( pszDSN );
        CPLFree( pszUserid );
        CPLFree( pszPassword );
        CPLFree( pszSRIDCol );
        CPLFree( pszSRTextCol );
        CPLFree( pszSRSTableName );
        return FALSE;
    }

    CPLFree( pszDSN );
    CPLFree( pszUserid );
    CPLFree( pszPassword );

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      If no explicit list of tables was given, check for a list in    */
/*      a geometry_columns table.                                       */
/* -------------------------------------------------------------------- */
    if( papszTables == nullptr )
    {
        CPLODBCStatement oStmt( &oSession );

        oStmt.Append( "SELECT f_table_name, f_geometry_column, geometry_type"
                      " FROM geometry_columns" );
        if( oStmt.ExecuteSQL() )
        {
            while( oStmt.Fetch() )
            {
                papszTables =
                    CSLAddString( papszTables, oStmt.GetColData(0) );
                papszGeomCol =
                    CSLAddString( papszGeomCol, oStmt.GetColData(1) );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables as           */
/*      non-spatial tables.                                             */
/* -------------------------------------------------------------------- */
    if( papszTables == nullptr )
    {
        CPLODBCStatement oTableList( &oSession );

        if( oTableList.GetTables() )
        {
            while( oTableList.Fetch() )
            {
                const char *pszSchema = oTableList.GetColData(1);
                CPLString osLayerName;

                if( pszSchema != nullptr && strlen(pszSchema) > 0 )
                {
                    osLayerName = pszSchema;
                    osLayerName += ".";
                }

                osLayerName += oTableList.GetColData(2);

                papszTables = CSLAddString( papszTables, osLayerName );

                papszGeomCol = CSLAddString(papszGeomCol,"");
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have an explicit list of requested tables, use them       */
/*      (non-spatial).                                                  */
/* -------------------------------------------------------------------- */
    for( int iTable = 0;
         papszTables != nullptr && papszTables[iTable] != nullptr;
         iTable++ )
    {
        if( strlen(papszGeomCol[iTable]) > 0 )
            OpenTable( papszTables[iTable], papszGeomCol[iTable], bUpdate );
        else
            OpenTable( papszTables[iTable], nullptr, bUpdate );
    }

    CSLDestroy( papszTables );
    CSLDestroy( papszGeomCol );

/* -------------------------------------------------------------------- */
/*      If no explicit list of tables was given, check for a list in    */
/*      a geometry_columns table.                                       */
/* -------------------------------------------------------------------- */
    if ( pszSRSTableName )
    {
        CPLODBCStatement oSRSList( &oSession );

        if ( !pszSRTextCol )
            pszSRTextCol = CPLStrdup( "srtext" );
        if ( !pszSRIDCol )
            pszSRIDCol = CPLStrdup( "srid" );

        oSRSList.Append( "SELECT " );
        oSRSList.Append( pszSRIDCol );
        oSRSList.Append( "," );
        oSRSList.Append( pszSRTextCol );
        oSRSList.Append( " FROM " );
        oSRSList.Append( pszSRSTableName );

        CPLDebug( "OGR_ODBC", "ExecuteSQL(%s) to read SRS table",
                  oSRSList.GetCommand() );
        if ( oSRSList.ExecuteSQL() )
        {
            int nRows = 256;     // A reasonable number of SRIDs to start from
            panSRID = (int *)CPLMalloc( nRows * sizeof(int) );
            papoSRS = (OGRSpatialReference **)
                CPLMalloc( nRows * sizeof(OGRSpatialReference*) );

            while ( oSRSList.Fetch() )
            {
                const char *pszSRID = oSRSList.GetColData( pszSRIDCol );
                if ( !pszSRID )
                    continue;

                const char *pszSRText = oSRSList.GetColData( pszSRTextCol );

                if ( pszSRText )
                {
                    if ( nKnownSRID > nRows )
                    {
                        nRows *= 2;
                        panSRID = (int *)CPLRealloc( panSRID,
                                                     nRows * sizeof(int) );
                        papoSRS = (OGRSpatialReference **)
                            CPLRealloc( papoSRS,
                            nRows * sizeof(OGRSpatialReference*) );
                    }
                    panSRID[nKnownSRID] = atoi( pszSRID );
                    papoSRS[nKnownSRID] = new OGRSpatialReference();
                    papoSRS[nKnownSRID]->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    if ( papoSRS[nKnownSRID]->importFromWkt( pszSRText )
                         != OGRERR_NONE )
                    {
                        delete papoSRS[nKnownSRID];
                        continue;
                    }
                    nKnownSRID++;
                }
            }
        }
    }

    if ( pszSRIDCol )
        CPLFree( pszSRIDCol );
    if ( pszSRTextCol )
        CPLFree( pszSRTextCol );
    if ( pszSRSTableName )
        CPLFree( pszSRSTableName );

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRODBCDataSource::OpenTable( const char *pszNewName,
                                  const char *pszGeomCol,
                                  CPL_UNUSED int bUpdate )
{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRODBCTableLayer *poLayer = new OGRODBCTableLayer( this );

    if( poLayer->Initialize( pszNewName, pszGeomCol ) )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRODBCLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRODBCLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCDataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRODBCDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;
    else
        return papoLayers[iLayer];
}
/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRODBCDataSource::ExecuteSQL( const char *pszSQLCommand,
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

    CPLDebug( "ODBC", "ExecuteSQL(%s) called.", pszSQLCommand );
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

    OGRODBCSelectLayer* poLayer = new OGRODBCSelectLayer( this, poStmt );

    if( poSpatialFilter != nullptr )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRODBCDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                  IsSupportedMsAccessFileExtension()                  */
/************************************************************************/

bool OGRODBCDataSource::IsSupportedMsAccessFileExtension(const char *pszExtension)
{
    // these are all possible extensions for MS Access databases
    return EQUAL(pszExtension, "MDB") ||
        EQUAL(pszExtension, "ACCDB") ||
        EQUAL(pszExtension, "STYLE");
}
