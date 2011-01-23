/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeomediaDataSource class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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
 ****************************************************************************/

#include "ogr_geomedia.h"
#include "ogrgeomediageometry.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRGeomediaDataSource()                        */
/************************************************************************/

OGRGeomediaDataSource::OGRGeomediaDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    papoLayersInvisible = NULL;
    nLayers = 0;
    nLayersWithInvisible = 0;
}

/************************************************************************/
/*                       ~OGRGeomediaDataSource()                       */
/************************************************************************/

OGRGeomediaDataSource::~OGRGeomediaDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    for( i = 0; i < nLayersWithInvisible; i++ )
        delete papoLayersInvisible[i];
    CPLFree( papoLayersInvisible );
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

int OGRGeomediaDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      If this is the name of an MDB file, then construct the          */
/*      appropriate connection string.  Otherwise clip of GEOMEDIA: to  */
/*      get the DSN.                                                    */
/*                                                                      */
/* -------------------------------------------------------------------- */
    char *pszDSN;
    if( EQUALN(pszNewName,"GEOMEDIA:",9) )
        pszDSN = CPLStrdup( pszNewName + 9 );
    else
    {
        const char *pszDSNStringTemplate = NULL;
        pszDSNStringTemplate = CPLGetConfigOption( "GEOMEDIA_DRIVER_TEMPLATE", "DRIVER=Microsoft Access Driver (*.mdb);DBQ=%s");
        if (!CheckDSNStringTemplate(pszDSNStringTemplate))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal value for GEOMEDIA_DRIVER_TEMPLATE option");
            return FALSE;
        }
        pszDSN = (char *) CPLMalloc(strlen(pszNewName)+strlen(pszDSNStringTemplate)+100);
        sprintf( pszDSN, pszDSNStringTemplate,  pszNewName );
    }

/* -------------------------------------------------------------------- */
/*      Initialize based on the DSN.                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "Geomedia", "EstablishSession(%s)", pszDSN );

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
/*      Collect list of tables and their supporting info from           */
/*      GAliasTable.                                                    */
/* -------------------------------------------------------------------- */
    CPLString osGFeaturesTable = GetTableNameFromType("INGRFeatures");
    if (osGFeaturesTable.size() == 0)
        return FALSE;

    CPLString osGeometryProperties = GetTableNameFromType("INGRGeometryProperties");
    CPLString osGCoordSystemTable = GetTableNameFromType("GCoordSystemTable");

    std::vector<char **> apapszGeomColumns;
    {
        CPLODBCStatement oStmt( &oSession );
        oStmt.Appendf( "SELECT FeatureName, PrimaryGeometryFieldName FROM %s", osGFeaturesTable.c_str() );

        if( !oStmt.ExecuteSQL() )
        {
            CPLDebug( "GEOMEDIA",
                    "SELECT on %s fails, perhaps not a geomedia geodatabase?\n%s",
                    osGFeaturesTable.c_str(),
                    oSession.GetLastError() );
            return FALSE;
        }

        while( oStmt.Fetch() )
        {
            int i, iNew = apapszGeomColumns.size();
            char **papszRecord = NULL;
            for( i = 0; i < 2; i++ )
                papszRecord = CSLAddString( papszRecord,
                                            oStmt.GetColData(i) );
            apapszGeomColumns.resize(iNew+1);
            apapszGeomColumns[iNew] = papszRecord;
        }
    }

    std::vector<OGRSpatialReference*> apoSRS;
    if (osGeometryProperties.size() != 0 && osGCoordSystemTable.size() != 0)
    {
        std::vector<CPLString> aosGUID;
        {
            CPLODBCStatement oStmt( &oSession );
            oStmt.Appendf( "SELECT GCoordSystemGUID FROM %s", osGeometryProperties.c_str() );

            if( !oStmt.ExecuteSQL() )
            {
                CPLDebug( "GEOMEDIA",
                        "SELECT on %s fails, perhaps not a geomedia geodatabase?\n%s",
                        osGeometryProperties.c_str(),
                        oSession.GetLastError() );
                return FALSE;
            }

            while( oStmt.Fetch() )
            {
                aosGUID.push_back(oStmt.GetColData(0));
            }

            if (apapszGeomColumns.size() != aosGUID.size())
            {
                CPLDebug( "GEOMEDIA", "%s and %s don't have the same size",
                        osGFeaturesTable.c_str(), osGeometryProperties.c_str() );
                return FALSE;
            }
        }

        int i;
        for(i=0; i<(int)aosGUID.size();i++)
        {
            apoSRS.push_back(GetGeomediaSRS(osGCoordSystemTable, aosGUID[i]));
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a layer for each spatial table.                          */
/* -------------------------------------------------------------------- */
    unsigned int iTable;

    papoLayers = (OGRGeomediaLayer **) CPLCalloc(apapszGeomColumns.size(),
                                             sizeof(void*));

    for( iTable = 0; iTable < apapszGeomColumns.size(); iTable++ )
    {
        char **papszRecord = apapszGeomColumns[iTable];
        OGRGeomediaTableLayer  *poLayer;

        poLayer = new OGRGeomediaTableLayer( this );

        if( poLayer->Initialize( papszRecord[0], papszRecord[1], (apoSRS.size()) ? apoSRS[iTable] : NULL )
            != CE_None )
        {
            delete poLayer;
        }
        else
        {
            papoLayers[nLayers++] = poLayer;
        }
        CSLDestroy(papszRecord);
    }

    return TRUE;
}


/************************************************************************/
/*                     GetTableNameFromType()                           */
/************************************************************************/

CPLString OGRGeomediaDataSource::GetTableNameFromType(const char* pszTableType)
{
    CPLODBCStatement oStmt( &oSession );

    oStmt.Appendf( "SELECT TableName FROM GAliasTable WHERE TableType = '%s'", pszTableType );

    if( !oStmt.ExecuteSQL() )
    {
        CPLDebug( "GEOMEDIA",
                  "SELECT for %s on GAliasTable fails, perhaps not a geomedia geodatabase?\n%s",
                  pszTableType,
                  oSession.GetLastError() );
        return "";
    }

    while( oStmt.Fetch() )
    {
        return oStmt.GetColData(0);
    }

    return "";
}


/************************************************************************/
/*                          GetGeomediaSRS()                            */
/************************************************************************/

OGRSpatialReference* OGRGeomediaDataSource::GetGeomediaSRS(const char* pszGCoordSystemTable,
                                                      const char* pszGCoordSystemGUID)
{
    if (pszGCoordSystemTable == NULL || pszGCoordSystemGUID == NULL)
        return NULL;

    OGRLayer* poGCoordSystemTable = GetLayerByName(pszGCoordSystemTable);
    if (poGCoordSystemTable == NULL)
        return NULL;

    poGCoordSystemTable->ResetReading();

    OGRFeature* poFeature;
    while((poFeature = poGCoordSystemTable->GetNextFeature()) != NULL)
    {
        const char* pszCSGUID = poFeature->GetFieldAsString("CSGUID");
        if (pszCSGUID && strcmp(pszCSGUID, pszGCoordSystemGUID) == 0)
        {
            OGRSpatialReference* poSRS = OGRGetGeomediaSRS(poFeature);
            delete poFeature;
            return poSRS;
        }

        delete poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeomediaDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGeomediaDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}


/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer *OGRGeomediaDataSource::GetLayerByName( const char* pszName )

{
    if (pszName == NULL)
        return NULL;
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszName);
    if (poLayer)
        return poLayer;

    for( int i = 0; i < nLayersWithInvisible; i++ )
    {
        poLayer = papoLayersInvisible[i];

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
            return poLayer;
    }

    OGRGeomediaTableLayer  *poGeomediaLayer;

    poGeomediaLayer = new OGRGeomediaTableLayer( this );

    if( poGeomediaLayer->Initialize(pszName, NULL, NULL) != CE_None )
    {
        delete poGeomediaLayer;
        return NULL;
    }

    papoLayersInvisible = (OGRGeomediaLayer**)CPLRealloc(papoLayersInvisible,
                            (nLayersWithInvisible+1) * sizeof(OGRGeomediaLayer*));
    papoLayersInvisible[nLayersWithInvisible++] = poGeomediaLayer;

    return poGeomediaLayer;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRGeomediaDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic imlplementation for OGRSQL dialect.                 */
/* -------------------------------------------------------------------- */
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
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
    OGRGeomediaSelectLayer *poLayer = NULL;
        
    poLayer = new OGRGeomediaSelectLayer( this, poStmt );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );
    
    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRGeomediaDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
