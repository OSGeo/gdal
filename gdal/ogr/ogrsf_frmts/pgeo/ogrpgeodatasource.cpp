/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include <unordered_set>
#include "filegdb_fielddomain.h"
#include "ogr_openfilegdb.h"

#ifdef __linux
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRPGeoDataSource()                          */
/************************************************************************/

OGRPGeoDataSource::OGRPGeoDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    pszName(nullptr)
{}

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

int OGRPGeoDataSource::Open( GDALOpenInfo *poOpenInfo )
{
    CPLAssert( nLayers == 0 );

     const char * pszNewName = poOpenInfo->pszFilename;
/* -------------------------------------------------------------------- */
/*      If this is the name of an MDB file, then construct the          */
/*      appropriate connection string.  Otherwise clip of PGEO: to      */
/*      get the DSN.                                                    */
/*                                                                      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszNewName, "PGEO:") )
    {
        char *pszDSN = CPLStrdup( pszNewName + 5 );
        CPLDebug( "PGeo", "EstablishSession(%s)", pszDSN );
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
        const char* pszDSNStringTemplate = CPLGetConfigOption( "PGEO_DRIVER_TEMPLATE", nullptr );
        if( pszDSNStringTemplate && !CheckDSNStringTemplate(pszDSNStringTemplate))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal value for PGEO_DRIVER_TEMPLATE option");
            return FALSE;
        }
        if ( !oSession.ConnectToMsAccess( pszNewName, pszDSNStringTemplate ) )
        {
            return FALSE;
        }
    }

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Collect list of tables and their supporting info from           */
/*      GDB_GeomColumns.                                                */
/* -------------------------------------------------------------------- */
    std::vector<char **> apapszGeomColumns;
    CPLODBCStatement oStmt( &oSession );

    oStmt.Append( "SELECT TableName, FieldName, ShapeType, ExtentLeft, ExtentRight, ExtentBottom, ExtentTop, SRID, HasZ, HasM FROM GDB_GeomColumns" );

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
        char **papszRecord = nullptr;
        for( i = 0; i < 10; i++ )
            papszRecord = CSLAddString( papszRecord,
                                        oStmt.GetColData(i) );
        apapszGeomColumns.resize(iNew+1);
        apapszGeomColumns[iNew] = papszRecord;
    }

    const bool bListAllTables = CPLTestBool(CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "NO"));

    // Collate a list of all tables in the data source, skipping over internal and system tables
    CPLODBCStatement oTableList( &oSession );
    std::vector< CPLString > aosTableNames;
    if( oTableList.GetTables() )
    {
        while( oTableList.Fetch() )
        {
            const CPLString osTableName = CPLString( oTableList.GetColData(2) );
            const CPLString osLCTableName(CPLString(osTableName).tolower());

            if( osLCTableName == "gdb_items" )
            {
                m_bHasGdbItemsTable = true;
            }

            if( !osTableName.empty() && ( bListAllTables || !IsPrivateLayerName( osTableName ) ) )
            {
                aosTableNames.emplace_back( osTableName );
            }
        }
    }

    // Create a layer for each spatial table.
    papoLayers = (OGRPGeoLayer **) CPLCalloc(apapszGeomColumns.size(),
                                             sizeof(void*));

    std::unordered_set<std::string> oSetSpatialTableNames;
    for( unsigned int iTable = 0; iTable < apapszGeomColumns.size(); iTable++ )
    {
        char **papszRecord = apapszGeomColumns[iTable];
        if ( EQUAL(papszRecord[0], "GDB_Items"))
        {
            // don't expose this internal layer
            CSLDestroy( papszRecord );
            continue;
        }

        OGRPGeoTableLayer  *poLayer = new OGRPGeoTableLayer( this );

        if( poLayer->Initialize( papszRecord[0],         // TableName
                                 papszRecord[1],         // FieldName
                                 atoi(papszRecord[2]),   // ShapeType
                                 CPLAtof(papszRecord[3]),   // ExtentLeft
                                 CPLAtof(papszRecord[4]),   // ExtentRight
                                 CPLAtof(papszRecord[5]),   // ExtentBottom
                                 CPLAtof(papszRecord[6]),   // ExtentTop
                                 atoi(papszRecord[7]),   // SRID
                                 atoi(papszRecord[8]),  // HasZ
                                 atoi(papszRecord[9]))  // HasM
            != CE_None )
        {
            delete poLayer;
        }
        else
        {
            papoLayers[nLayers++] = poLayer;
            oSetSpatialTableNames.insert( CPLString( papszRecord[ 0 ] ) );
        }

        CSLDestroy( papszRecord );
    }


    // Add non-spatial tables.
    for ( const CPLString &osTableName : aosTableNames )
    {
        if( oSetSpatialTableNames.find( osTableName ) != oSetSpatialTableNames.end() )
        {
            // a spatial table, already handled above
            continue;
        }

        OGRPGeoTableLayer  *poLayer = new OGRPGeoTableLayer( this );

        if( poLayer->Initialize( osTableName.c_str(),         // TableName
                                 nullptr,         // FieldName
                                 0,   // ShapeType (ESRI_LAYERGEOMTYPE_NULL)
                                 0,   // ExtentLeft
                                 0,   // ExtentRight
                                 0,   // ExtentBottom
                                 0,   // ExtentTop
                                 0,   // SRID
                                 0,   // HasZ
                                 0)   // HasM
            != CE_None )
        {
            delete poLayer;
        }
        else
        {
            papoLayers = static_cast< OGRPGeoLayer **>( CPLRealloc(papoLayers, sizeof(void*) * ( nLayers+1 ) ) );
            papoLayers[nLayers++] = poLayer;
        }
    }

    // collect domains
    if ( m_bHasGdbItemsTable )
    {
        CPLODBCStatement oItemsStmt( &oSession );
        oItemsStmt.Append( "SELECT Definition FROM GDB_Items" );
        if( oItemsStmt.ExecuteSQL() )
        {
            while( oItemsStmt.Fetch() )
            {
                const CPLString osDefinition = CPLString( oItemsStmt.GetColData(0, "") );
                if( strstr(osDefinition, "GPCodedValueDomain2") != nullptr ||
                    strstr(osDefinition, "GPRangeDomain2") != nullptr )
                {
                    if( auto poDomain = ParseXMLFieldDomainDef(osDefinition) )
                    {
                        const auto domainName = poDomain->GetName();
                        m_oMapFieldDomains[domainName] = std::move(poDomain);
                    }
                }
            }
        }
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
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                    IsPrivateLayerName()                              */
/************************************************************************/

bool OGRPGeoDataSource::IsPrivateLayerName(const CPLString &osName)
{
    const CPLString osLCTableName(CPLString(osName).tolower());

    return ( (osLCTableName.size() >= 4 && osLCTableName.substr(0, 4) == "msys") // MS Access internal tables
         || osLCTableName.endsWith( "_shape_index") // gdb spatial index tables, internal details only
         || (osLCTableName.size() >= 4 && osLCTableName.substr(0, 4) == "gdb_") // gdb private tables
        );
}

/************************************************************************/
/*                    IsLayerPrivate()                                  */
/************************************************************************/

bool OGRPGeoDataSource::IsLayerPrivate(int iLayer) const
{
    if( iLayer < 0 || iLayer >= nLayers )
        return false;

    const std::string osName( papoLayers[iLayer]->GetName() );
    return IsPrivateLayerName( osName );
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRPGeoDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{

/* -------------------------------------------------------------------- */
/*      Special case GetLayerDefinition                                 */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerDefinition "))
    {
        OGRPGeoTableLayer* poLayer = cpl::down_cast<OGRPGeoTableLayer *>(
            GetLayerByName(pszSQLCommand + strlen("GetLayerDefinition ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerDefinition", poLayer->GetXMLDefinition().c_str() );
            return poRet;
        }

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerMetadata                                   */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerMetadata "))
    {
        OGRPGeoTableLayer* poLayer = cpl::down_cast<OGRPGeoTableLayer *>(
            GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerMetadata", poLayer->GetXMLDocumentation().c_str() );
            return poRet;
        }

        return nullptr;
    }

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
    OGRPGeoSelectLayer* poLayer = new OGRPGeoSelectLayer( this, poStmt );

    if( poSpatialFilter != nullptr )
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

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

bool OGRPGeoDataSource::CountStarWorking() const
{
#ifdef _WIN32
    return true;
#else
    // SELECT COUNT(*) worked in mdbtools 0.9.0 to 0.9.2, but got broken in
    // 0.9.3. So test if it is working
    // See https://github.com/OSGeo/gdal/issues/4103
    if( !m_COUNT_STAR_state_known )
    {
        m_COUNT_STAR_state_known = true;

#ifdef __linux
        // Temporarily redirect stderr to /dev/null
        int new_fd = open("/dev/null", O_WRONLY|O_CREAT|O_TRUNC, 0666);
        int old_stderr = -1;
        if( new_fd != -1 )
        {
            old_stderr = dup(fileno(stderr));
            if( old_stderr != -1 )
            {
                dup2(new_fd, fileno(stderr));
            }
            close(new_fd);
        }
#endif

        CPLErrorHandlerPusher oErrorHandler(CPLErrorHandlerPusher);
        CPLErrorStateBackuper oStateBackuper;

        CPLODBCStatement oStmt( &oSession );
        oStmt.Append( "SELECT COUNT(*) FROM GDB_GeomColumns" );
        if( oStmt.ExecuteSQL() && oStmt.Fetch() )
        {
            m_COUNT_STAR_working = true;
        }

#ifdef __linux
        if( old_stderr != -1 )
        {
            dup2(old_stderr, fileno(stderr));
            close(old_stderr);
        }
#endif
    }
    return m_COUNT_STAR_working;
#endif
}
