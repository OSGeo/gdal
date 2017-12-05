/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGResultLayer class, access the resultset from
 *           a particular select query done via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_pg.h"

CPL_CVSID("$Id$")

#define PQexec this_is_an_error

/************************************************************************/
/*                          OGRPGResultLayer()                          */
/************************************************************************/

OGRPGResultLayer::OGRPGResultLayer( OGRPGDataSource *poDSIn,
                                    const char * pszRawQueryIn,
                                    PGresult *hInitialResultIn ) :
    pszRawStatement(CPLStrdup(pszRawQueryIn)),
    pszGeomTableName(NULL),
    pszGeomTableSchemaName(NULL),
    osWHERE("")
{
    poDS = poDSIn;

    iNextShapeId = 0;

    BuildFullQueryStatement();

    ReadResultDefinition(hInitialResultIn);

    /* Find at which index the geometry column is */
    /* and prepare a request to identify not-nullable fields */
    int iGeomCol = -1;
    CPLString osRequest;
    std::map< std::pair<int,int>, int> oMapAttributeToFieldIndex;

    for( int iRawField = 0;
         iRawField < PQnfields(hInitialResultIn);
         iRawField++ )
    {
        if( poFeatureDefn->GetGeomFieldCount() == 1 &&
            strcmp(PQfname(hInitialResultIn,iRawField),
                poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) == 0 )
        {
            iGeomCol = iRawField;
        }

        Oid tableOID = PQftable(hInitialResultIn, iRawField);
        int tableCol = PQftablecol(hInitialResultIn, iRawField);
        if( tableOID != InvalidOid && tableCol > 0 )
        {
            if( !osRequest.empty() )
                osRequest += " OR ";
            osRequest += "(attrelid = ";
            osRequest += CPLSPrintf("%d", tableOID);
            osRequest += " AND attnum = ";
            osRequest += CPLSPrintf("%d)", tableCol);
            oMapAttributeToFieldIndex[std::pair<int,int>(tableOID,tableCol)] = iRawField;
        }
    }

    if( !osRequest.empty() )
    {
        osRequest = "SELECT attnum, attrelid FROM pg_attribute WHERE attnotnull = 't' AND (" + osRequest + ")";
        PGresult* hResult = OGRPG_PQexec(poDS->GetPGConn(), osRequest );
        if( hResult && PQresultStatus(hResult) == PGRES_TUPLES_OK)
        {
            int iCol;
            for( iCol = 0; iCol < PQntuples(hResult); iCol++ )
            {
                const char* pszAttNum = PQgetvalue(hResult,iCol,0);
                const char* pszAttRelid = PQgetvalue(hResult,iCol,1);
                int iRawField = oMapAttributeToFieldIndex[std::pair<int,int>(atoi(pszAttRelid),atoi(pszAttNum))];
                const char* pszFieldname = PQfname(hInitialResultIn,iRawField);
                int iFieldIdx = poFeatureDefn->GetFieldIndex(pszFieldname);
                if( iFieldIdx >= 0 )
                    poFeatureDefn->GetFieldDefn(iFieldIdx)->SetNullable(FALSE);
                else
                {
                    iFieldIdx = poFeatureDefn->GetGeomFieldIndex(pszFieldname);
                    if( iFieldIdx >= 0 )
                        poFeatureDefn->GetGeomFieldDefn(iFieldIdx)->SetNullable(FALSE);
                }
            }
        }
        OGRPGClearResult( hResult );
    }

    /* Determine the table from which the geometry column is extracted */
    if (iGeomCol != -1)
    {
        Oid tableOID = PQftable(hInitialResultIn, iGeomCol);
        if (tableOID != InvalidOid)
        {
            CPLString osGetTableName;
            osGetTableName.Printf("SELECT c.relname, n.nspname FROM pg_class c "
                                  "JOIN pg_namespace n ON c.relnamespace=n.oid WHERE c.oid = %d ", tableOID);
            PGresult* hTableNameResult = OGRPG_PQexec(poDS->GetPGConn(), osGetTableName );
            if( hTableNameResult && PQresultStatus(hTableNameResult) == PGRES_TUPLES_OK)
            {
                if ( PQntuples(hTableNameResult) > 0 )
                {
                    pszGeomTableName = CPLStrdup(PQgetvalue(hTableNameResult,0,0));
                    pszGeomTableSchemaName = CPLStrdup(PQgetvalue(hTableNameResult,0,1));
                }
            }
            OGRPGClearResult( hTableNameResult );
        }
    }
}

/************************************************************************/
/*                          ~OGRPGResultLayer()                          */
/************************************************************************/

OGRPGResultLayer::~OGRPGResultLayer()

{
    CPLFree( pszRawStatement );
    CPLFree( pszGeomTableName );
    CPLFree( pszGeomTableSchemaName );
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRPGResultLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    const size_t nLen = strlen(pszRawStatement) + osWHERE.size() + 40;
    pszQueryStatement = (char*) CPLMalloc(nLen);

    if (osWHERE.empty())
        strcpy(pszQueryStatement, pszRawStatement);
    else
        snprintf(pszQueryStatement, nLen, "SELECT * FROM (%s) AS ogrpgsubquery %s",
                pszRawStatement, osWHERE.c_str());
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGResultLayer::ResetReading()

{
    OGRPGLayer::ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPGResultLayer::GetFeatureCount( int bForce )

{
    if( TestCapability(OLCFastFeatureCount) == FALSE )
        return OGRPGLayer::GetFeatureCount( bForce );

    PGconn              *hPGConn = poDS->GetPGConn();
    PGresult            *hResult = NULL;
    CPLString           osCommand;
    int                 nCount = 0;

    osCommand.Printf(
        "SELECT count(*) FROM (%s) AS ogrpgcount",
        pszQueryStatement );

    hResult = OGRPG_PQexec(hPGConn, osCommand);
    if( hResult != NULL && PQresultStatus(hResult) == PGRES_TUPLES_OK )
        nCount = atoi(PQgetvalue(hResult,0,0));
    else
        CPLDebug( "PG", "%s; failed.", osCommand.c_str() );
    OGRPGClearResult( hResult );

    return nCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGResultLayer::TestCapability( const char * pszCap )

{
    GetLayerDefn();

    if( EQUAL(pszCap,OLCFastFeatureCount) ||
        EQUAL(pszCap,OLCFastSetNextByIndex) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
        return (m_poFilterGeom == NULL ||
                poGeomFieldDefn == NULL ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
               && m_poAttrQuery == NULL;
    }
    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
        return (poGeomFieldDefn == NULL ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
               && m_poAttrQuery == NULL;
    }

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
        if( poFeatureDefn->GetGeomFieldCount() > 0 )
            poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(0);
        return (poGeomFieldDefn == NULL ||
                poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
               && m_poAttrQuery == NULL;
    }
    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGResultLayer::GetNextFeature()

{
    OGRPGGeomFieldDefn* poGeomFieldDefn = NULL;
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poGeomFieldDefn = poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);

    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || poGeomFieldDefn == NULL
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY
            || poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGResultLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    OGRPGGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->myGetGeomFieldDefn(m_iGeomFieldFilter);
    if( InstallFilter( poGeomIn ) )
    {
        if ( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOMETRY ||
             poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
        {
            if( m_poFilterGeom != NULL)
            {
                char szBox3D_1[128];
                char szBox3D_2[128];
                OGREnvelope  sEnvelope;

                m_poFilterGeom->getEnvelope( &sEnvelope );
                if( poGeomFieldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
                {
                    if( sEnvelope.MinX < -180.0 )
                        sEnvelope.MinX = -180.0;
                    if( sEnvelope.MinY < -90.0 )
                        sEnvelope.MinY = -90.0;
                    if( sEnvelope.MaxX > 180.0 )
                        sEnvelope.MaxX = 180.0;
                    if( sEnvelope.MaxY > 90.0 )
                        sEnvelope.MaxY = 90.0;
                }
                CPLsnprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
                CPLsnprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
                osWHERE.Printf("WHERE %s && %s('BOX3D(%s, %s)'::box3d,%d) ",
                               OGRPGEscapeColumnName(poGeomFieldDefn->GetNameRef()).c_str(),
                               (poDS->sPostGISVersion.nMajor >= 2) ? "ST_SetSRID" : "SetSRID",
                               szBox3D_1, szBox3D_2, poGeomFieldDefn->nSRSId );
            }
            else
            {
                osWHERE = "";
            }

            BuildFullQueryStatement();
        }

        ResetReading();
    }
}

/************************************************************************/
/*                            ResolveSRID()                             */
/************************************************************************/

void OGRPGResultLayer::ResolveSRID(OGRPGGeomFieldDefn* poGFldDefn)

{
    /* We have to get the SRID of the geometry column, so to be able */
    /* to do spatial filtering */
    int nSRSId = UNDETERMINED_SRID;
    if( poGFldDefn->ePostgisType == GEOM_TYPE_GEOMETRY )
    {
        if (pszGeomTableName != NULL)
        {
            CPLString osName(pszGeomTableSchemaName);
            osName += ".";
            osName += pszGeomTableName;
            OGRPGLayer* poBaseLayer = (OGRPGLayer*) poDS->GetLayerByName(osName);
            if (poBaseLayer)
            {
                int iBaseIdx = poBaseLayer->GetLayerDefn()->
                    GetGeomFieldIndex( poGFldDefn->GetNameRef() );
                if( iBaseIdx >= 0 )
                {
                    OGRPGGeomFieldDefn* poBaseGFldDefn =
                        poBaseLayer->myGetLayerDefn()->myGetGeomFieldDefn(iBaseIdx);
                    poBaseGFldDefn->GetSpatialRef(); /* To make sure nSRSId is resolved */
                    nSRSId = poBaseGFldDefn->nSRSId;
                }
            }
        }

        if( nSRSId == UNDETERMINED_SRID )
        {
            CPLString osGetSRID;

            const char* psGetSRIDFct =
                poDS->sPostGISVersion.nMajor >= 2 ? "ST_SRID" : "getsrid";

            osGetSRID += "SELECT ";
            osGetSRID += psGetSRIDFct;
            osGetSRID += "(";
            osGetSRID += OGRPGEscapeColumnName(poGFldDefn->GetNameRef());
            if (poDS->sPostGISVersion.nMajor > 2 || (poDS->sPostGISVersion.nMajor == 2 && poDS->sPostGISVersion.nMinor >= 2))
                osGetSRID += "::geometry";
            osGetSRID += ") FROM(";
            osGetSRID += pszRawStatement;
            osGetSRID += ") AS ogrpggetsrid LIMIT 1";

            PGresult* hSRSIdResult = OGRPG_PQexec(poDS->GetPGConn(), osGetSRID );

            nSRSId = poDS->GetUndefinedSRID();

            if( hSRSIdResult && PQresultStatus(hSRSIdResult) == PGRES_TUPLES_OK)
            {
                if ( PQntuples(hSRSIdResult) > 0 )
                    nSRSId = atoi(PQgetvalue(hSRSIdResult, 0, 0));
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                            "%s", PQerrorMessage(poDS->GetPGConn()) );
            }

            OGRPGClearResult(hSRSIdResult);
        }
    }
    else if( poGFldDefn->ePostgisType == GEOM_TYPE_GEOGRAPHY )
    {
        nSRSId = 4326;
    }
    poGFldDefn->nSRSId = nSRSId;
}
