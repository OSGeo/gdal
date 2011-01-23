/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMDBDataSource class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "ogr_mdb.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include <vector>
#include "ogrgeomediageometry.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRMDBDataSource()                          */
/************************************************************************/

OGRMDBDataSource::OGRMDBDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    papoLayersInvisible = NULL;
    nLayers = 0;
    nLayersWithInvisible = 0;
    poDB = NULL;
}

/************************************************************************/
/*                         ~OGRMDBDataSource()                         */
/************************************************************************/

OGRMDBDataSource::~OGRMDBDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    for( i = 0; i < nLayersWithInvisible; i++ )
        delete papoLayersInvisible[i];
    CPLFree( papoLayersInvisible );


    delete poDB;
}


/************************************************************************/
/*                              OpenGDB()                               */
/************************************************************************/

int OGRMDBDataSource::OpenGDB(OGRMDBTable* poGDB_GeomColumns)
{
    int iTableName = poGDB_GeomColumns->GetColumnIndex("TableName", TRUE);
    int iFieldName = poGDB_GeomColumns->GetColumnIndex("FieldName", TRUE);
    int iShapeType = poGDB_GeomColumns->GetColumnIndex("ShapeType", TRUE);
    int iExtentLeft = poGDB_GeomColumns->GetColumnIndex("ExtentLeft", TRUE);
    int iExtentRight = poGDB_GeomColumns->GetColumnIndex("ExtentRight", TRUE);
    int iExtentBottom = poGDB_GeomColumns->GetColumnIndex("ExtentBottom", TRUE);
    int iExtentTop = poGDB_GeomColumns->GetColumnIndex("ExtentTop", TRUE);
    int iSRID = poGDB_GeomColumns->GetColumnIndex("SRID", TRUE);
    int iHasZ = poGDB_GeomColumns->GetColumnIndex("HasZ", TRUE);

    if (iTableName < 0 || iFieldName < 0 || iShapeType < 0 ||
        iExtentLeft < 0 || iExtentRight < 0 || iExtentBottom < 0 ||
        iExtentTop < 0 || iSRID < 0 || iHasZ < 0)
        return FALSE;

    while(poGDB_GeomColumns->GetNextRow())
    {
        OGRMDBLayer  *poLayer;

        char* pszTableName = poGDB_GeomColumns->GetColumnAsString(iTableName);
        char* pszFieldName = poGDB_GeomColumns->GetColumnAsString(iFieldName);
        if (pszTableName == NULL || pszFieldName == NULL)
        {
            CPLFree(pszTableName);
            CPLFree(pszFieldName);
            continue;
        }

        OGRMDBTable* poTable = poDB->GetTable(pszTableName);
        if (poTable == NULL)
        {
            CPLFree(pszTableName);
            CPLFree(pszFieldName);
            continue;
        }

        poLayer = new OGRMDBLayer( this, poTable );

        if( poLayer->Initialize( pszTableName,
                                 pszFieldName,
                                 poGDB_GeomColumns->GetColumnAsInt(iShapeType),
                                 poGDB_GeomColumns->GetColumnAsDouble(iExtentLeft),
                                 poGDB_GeomColumns->GetColumnAsDouble(iExtentRight),
                                 poGDB_GeomColumns->GetColumnAsDouble(iExtentBottom),
                                 poGDB_GeomColumns->GetColumnAsDouble(iExtentTop),
                                 poGDB_GeomColumns->GetColumnAsInt(iSRID),
                                 poGDB_GeomColumns->GetColumnAsInt(iHasZ) )
            != CE_None )
        {
            delete poLayer;
        }
        else
        {
            papoLayers = (OGRMDBLayer**)CPLRealloc(papoLayers, (nLayers+1) * sizeof(OGRMDBLayer*));
            papoLayers[nLayers++] = poLayer;
        }

        CPLFree(pszTableName);
        CPLFree(pszFieldName);
    }

    return TRUE;
}

/************************************************************************/
/*                        OpenGeomediaWarehouse()                       */
/************************************************************************/

int OGRMDBDataSource::OpenGeomediaWarehouse(OGRMDBTable* poGAliasTable)
{
    int iTableName = poGAliasTable->GetColumnIndex("TableName", TRUE);
    int iTableType = poGAliasTable->GetColumnIndex("TableType", TRUE);

    if (iTableName < 0 || iTableType < 0)
        return FALSE;

    char* pszFeatureTableName = NULL;
    char* pszGeometryProperties = NULL;
    char* pszGCoordSystemTable = NULL;
    while(poGAliasTable->GetNextRow())
    {
        char* pszTableType = poGAliasTable->GetColumnAsString(iTableType);
        if (pszTableType == NULL)
            continue;

        if (strcmp(pszTableType, "INGRFeatures") == 0)
        {
            pszFeatureTableName = poGAliasTable->GetColumnAsString(iTableName);
        }
        else if (strcmp(pszTableType, "INGRGeometryProperties") == 0)
        {
            pszGeometryProperties = poGAliasTable->GetColumnAsString(iTableName);
        }
        else if (strcmp(pszTableType, "GCoordSystemTable") == 0)
        {
            pszGCoordSystemTable = poGAliasTable->GetColumnAsString(iTableName);
        }

        CPLFree(pszTableType);
    }

    if (pszFeatureTableName == NULL)
    {
        CPLFree(pszGeometryProperties);
        CPLFree(pszGCoordSystemTable);
        return FALSE;
    }

    OGRMDBTable* poGFeaturesTable = poDB->GetTable(pszFeatureTableName);
    CPLFree(pszFeatureTableName);
    pszFeatureTableName = NULL;

    OGRMDBTable* poGeometryPropertiesTable;
    if (pszGeometryProperties)
        poGeometryPropertiesTable = poDB->GetTable(pszGeometryProperties);
    else
        poGeometryPropertiesTable = NULL;
    CPLFree(pszGeometryProperties);
    pszGeometryProperties = NULL;

    if (poGFeaturesTable == NULL)
    {
        delete poGeometryPropertiesTable;
        CPLFree(pszGCoordSystemTable);
        return FALSE;
    }

    int iFeatureName = poGFeaturesTable->GetColumnIndex("FeatureName", TRUE);
    int iGeometryType = poGFeaturesTable->GetColumnIndex("GeometryType", TRUE);
    int iPrimaryGeometryFieldName = poGFeaturesTable->GetColumnIndex("PrimaryGeometryFieldName", TRUE);

    if (iFeatureName < 0 || iGeometryType < 0 || iPrimaryGeometryFieldName < 0)
    {
        delete poGeometryPropertiesTable;
        delete poGFeaturesTable;
        CPLFree(pszGCoordSystemTable);
        return FALSE;
    }

    if (poGeometryPropertiesTable != NULL && poGeometryPropertiesTable->GetRowCount() != poGFeaturesTable->GetRowCount())
    {
        delete poGeometryPropertiesTable;
        poGeometryPropertiesTable = NULL;
    }

    int iGCoordSystemGUID = -1;
    if (poGeometryPropertiesTable)
    {
        iGCoordSystemGUID = poGeometryPropertiesTable->GetColumnIndex("GCoordSystemGUID", TRUE);
        if (iGCoordSystemGUID < 0)
        {
            delete poGeometryPropertiesTable;
            delete poGFeaturesTable;
            CPLFree(pszGCoordSystemTable);
            return FALSE;
        }
    }

    while(poGFeaturesTable->GetNextRow() &&
          (poGeometryPropertiesTable == NULL || poGeometryPropertiesTable->GetNextRow()))
    {
        char* pszFeatureName = poGFeaturesTable->GetColumnAsString(iFeatureName);
        //int nGeometryType = poGFeaturesTable->GetColumnAsInt(iGeometryType);
        char* pszGeometryFieldName = poGFeaturesTable->GetColumnAsString(iPrimaryGeometryFieldName);
        char* pszGCoordSystemGUID;
        if (poGeometryPropertiesTable)
            pszGCoordSystemGUID = poGeometryPropertiesTable->GetColumnAsString(iGCoordSystemGUID);
        else
            pszGCoordSystemGUID = NULL;
        if (pszFeatureName && pszGeometryFieldName)
        {
            OGRMDBTable* poTable = poDB->GetTable(pszFeatureName);
            if (poTable)
            {
                OGRMDBLayer* poLayer = new OGRMDBLayer( this, poTable );

                if( poLayer->Initialize( pszFeatureName,
                                         pszGeometryFieldName,
                                         GetGeomediaSRS(pszGCoordSystemTable, pszGCoordSystemGUID) )
                    != CE_None )
                {
                    delete poLayer;
                }
                else
                {
                    papoLayers = (OGRMDBLayer**)CPLRealloc(papoLayers, (nLayers+1) * sizeof(OGRMDBLayer*));
                    papoLayers[nLayers++] = poLayer;
                }
            }
        }
        CPLFree(pszFeatureName);
        CPLFree(pszGeometryFieldName);
        CPLFree(pszGCoordSystemGUID);
    }

    delete poGeometryPropertiesTable;
    delete poGFeaturesTable;
    CPLFree(pszGCoordSystemTable);

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMDBDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen )

{
    CPLAssert( nLayers == 0 );

    pszName = CPLStrdup( pszNewName );

    if (!env.Init())
        return FALSE;

    poDB = OGRMDBDatabase::Open(&env, pszNewName);
    if (!poDB)
        return FALSE;

    poDB->FetchTableNames();

    /* Is it a ESRI Personal Geodatabase ? */
    OGRMDBTable* poGDB_GeomColumns = poDB->GetTable("GDB_GeomColumns");
    if (poGDB_GeomColumns && !CSLTestBoolean(CPLGetConfigOption("MDB_RAW", "OFF")))
    {
        int nRet = OpenGDB(poGDB_GeomColumns);
        delete poGDB_GeomColumns;
        return nRet;
    }
    delete poGDB_GeomColumns;

    /* Is it a Geomedia warehouse ? */
    OGRMDBTable* poGAliasTable = poDB->GetTable("GAliasTable");
    if (poGAliasTable && !CSLTestBoolean(CPLGetConfigOption("MDB_RAW", "OFF")))
    {
        int nRet = OpenGeomediaWarehouse(poGAliasTable);
        delete poGAliasTable;
        return nRet;
    }
    delete poGAliasTable;

    /* Well, no, just a regular MDB */
    int nTables = (int) poDB->apoTableNames.size();
    for(int i=0;i<nTables;i++)
    {
        OGRMDBTable* poTable = poDB->GetTable(poDB->apoTableNames[i]);
        if (poTable == NULL)
            continue;

        OGRMDBLayer* poLayer = new OGRMDBLayer( this, poTable );
        if( poLayer->BuildFeatureDefn() != CE_None )
        {
            delete poLayer;
            continue;
        }

        papoLayers = (OGRMDBLayer**)CPLRealloc(papoLayers, (nLayers+1) * sizeof(OGRMDBLayer*));
        papoLayers[nLayers++] = poLayer;
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMDBDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMDBDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMDBDataSource::GetLayerByName( const char* pszName )

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

    OGRMDBTable* poTable = poDB->GetTable(pszName);
    if (poTable == NULL)
        return NULL;

    OGRMDBLayer* poMDBLayer = new OGRMDBLayer( this, poTable );
    if( poMDBLayer->BuildFeatureDefn() != CE_None )
    {
        delete poMDBLayer;
        return NULL;
    }

    papoLayersInvisible = (OGRMDBLayer**)CPLRealloc(papoLayersInvisible,
                            (nLayersWithInvisible+1) * sizeof(OGRMDBLayer*));
    papoLayersInvisible[nLayersWithInvisible++] = poMDBLayer;

    return poMDBLayer;
}

/************************************************************************/
/*                          GetGeomediaSRS()                            */
/************************************************************************/

OGRSpatialReference* OGRMDBDataSource::GetGeomediaSRS(const char* pszGCoordSystemTable,
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
