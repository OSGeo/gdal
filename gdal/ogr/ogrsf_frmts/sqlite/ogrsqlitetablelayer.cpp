/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_sqlite.h"
#include "ogr_p.h"
#include "cpl_time.h"
#include <string>

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSQLiteTableLayer()                         */
/************************************************************************/

OGRSQLiteTableLayer::OGRSQLiteTableLayer( OGRSQLiteDataSource *poDSIn )
{ // start tasks (non-db) are done in OGRSQLiteEditableLayer(), which runs first
 poDS = poDSIn;
     /* SpatiaLite v.2.4.0 (or any subsequent) is required
       to support 2.5D: if an obsolete version of the library
       is found we'll unconditionally activate 2D casting mode.
    */
    if ( poDS->IsSpatialiteDB() )
     bSpatialite2D = poDS->GetSpatialiteVersionNumber() < 24;
}

/************************************************************************/
/*                        ~OGRSQLiteTableLayer()                        */
/************************************************************************/

OGRSQLiteTableLayer::~OGRSQLiteTableLayer()
{ // cleanup done in ~OGRSQLiteEditableLayer()
}

/************************************************************************/
/*                             Initialize()                             */
/* Note:                                                                */
/* - when compleated the LayerType is known                             */
/* -- and the validity checked and CE_Failure returned if NOT true      */
/************************************************************************/
CPLErr OGRSQLiteTableLayer::Initialize( const char *pszTableName, 
                                        OGRSQLiteLayerType eSQLiteLayerType,
                                        int bDeferredCreation )
{
 // CPLDebug( "OGR", "-I-> OGRSQLiteTableLayer::Initialize(%s): layer_type=[%d] database_type=[%d]", pszTableName, eSQLiteLayerType,poDS->GetDatabaseType());
 return OGRSQLiteEditableLayer::Initialize(pszTableName,eSQLiteLayerType,bDeferredCreation);
}


/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRSQLiteTableLayer::CreateSpatialIndexIfNecessary()
{ // Table specific function
    if( bDeferredSpatialIndexCreation )
    {
        for(int iGeomCol = 0; iGeomCol < poFeatureDefn->GetGeomFieldCount(); iGeomCol ++)
            CreateSpatialIndex(iGeomCol);
    }
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

int OGRSQLiteTableLayer::CreateSpatialIndex(int iGeomCol)
{ // Table specific function
    CPLString osCommand;

    if( bDeferredCreation ) RunDeferredCreationIfNecessary();

    if( iGeomCol < 0 || iGeomCol >= poFeatureDefn->GetGeomFieldCount() )
        return FALSE;

    osCommand.Printf("SELECT CreateSpatialIndex('%s', '%s')",
                     pszEscapedTableName,
                     OGRSQLiteEscape(poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef()).c_str());

    char* pszErrMsg = NULL;
    sqlite3 *hDB = poDS->GetDB();
#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif
    int rc = sqlite3_exec( hDB, osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Unable to create spatial index:\n%s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    poFeatureDefn->myGetGeomFieldDefn(iGeomCol)->bHasSpatialIndex = TRUE;
    return TRUE;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RunDeferredCreationIfNecessary()
{ // Table specific function
    if( !bDeferredCreation )
        return OGRERR_NONE;
    bDeferredCreation = FALSE;

    const char* pszLayerName = poFeatureDefn->GetName();

    int rc;
    char *pszErrMsg;
    CPLString osCommand;
    
    osCommand.Printf( "CREATE TABLE '%s' ( %s INTEGER PRIMARY KEY", 
                      pszEscapedTableName,
                      pszFIDColumn );

    int i;
    if ( !poDS->IsSpatialiteDB() )
    {
        for(i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                poFeatureDefn->myGetGeomFieldDefn(i);

            if( poGeomFieldDefn->eGeomFormat == OSGF_WKT )
            {
                osCommand += CPLSPrintf(", '%s' VARCHAR", 
                    OGRSQLiteEscape(poGeomFieldDefn->GetNameRef()).c_str() );
            }
            else
            {
                osCommand += CPLSPrintf(", '%s' BLOB", 
                    OGRSQLiteEscape(poGeomFieldDefn->GetNameRef()).c_str() );
            }
            if( !poGeomFieldDefn->IsNullable() )
            {
                osCommand += " NOT NULL";
            }
        }
    }

    for(i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        if( i == iFIDAsRegularColumnIndex )
            continue;
        CPLString osFieldType(FieldDefnToSQliteFieldDefn(poFieldDefn));
        osCommand += CPLSPrintf(", '%s' %s",
                        OGRSQLiteEscape(poFieldDefn->GetNameRef()).c_str(),
                        osFieldType.c_str());
        if( !poFieldDefn->IsNullable() )
        {
            osCommand += " NOT NULL";
        }
        const char* pszDefault = poFieldDefn->GetDefault();
        if( pszDefault != NULL &&
            (!poFieldDefn->IsDefaultDriverSpecific() ||
             (pszDefault[0] == '(' && pszDefault[strlen(pszDefault)-1] == ')' &&
             (STARTS_WITH_CI(pszDefault+1, "strftime") ||
              STARTS_WITH_CI(pszDefault+1, " strftime")))) )
        {
            osCommand += " DEFAULT ";
            osCommand += poFieldDefn->GetDefault();
        }
    }
    osCommand += ")";

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

    rc = sqlite3_exec( poDS->GetDB(), osCommand, NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create table %s: %s",
                  pszLayerName, pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( poDS->HasGeometryColumns() )
    {
        /* Sometimes there is an old cruft entry in the geometry_columns
        * table if things were not properly cleaned up before.  We make
        * an effort to clean out such cruft.
        */
        osCommand.Printf(
            "DELETE FROM geometry_columns WHERE f_table_name = '%s'", 
            pszEscapedTableName );

#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

        rc = sqlite3_exec( poDS->GetDB(), osCommand, NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to run %s: %s",
                  osCommand.c_str(), pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }

        for(i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                poFeatureDefn->myGetGeomFieldDefn(i);
            RunAddGeometryColumn(poGeomFieldDefn, FALSE);
        }
    }

    if (RecomputeOrdinals() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( poDS->IsSpatialiteDB() && poDS->GetLayerCount() == 1)
    {
        /* To create the layer_statistics and spatialite_history tables */
        rc = sqlite3_exec( poDS->GetDB(), "SELECT UpdateLayerStatistics()", NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to run %s: %s",
                  osCommand.c_str(), pszErrMsg );
            sqlite3_free( pszErrMsg );
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}
