/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

#define PQexec this_is_an_error

/************************************************************************/
/*                          OGRPGResultLayer()                          */
/************************************************************************/

OGRPGResultLayer::OGRPGResultLayer( OGRPGDataSource *poDSIn, 
                                    const char * pszRawQueryIn,
                                    PGresult *hInitialResultIn )
{
    poDS = poDSIn;

    iNextShapeId = 0;

    pszRawStatement = CPLStrdup(pszRawQueryIn);

    osWHERE = "";

    BuildFullQueryStatement();

    ReadResultDefinition(hInitialResultIn);

    pszGeomTableName = NULL;
    pszGeomTableSchemaName = NULL;

    /* Find at which index the geometry column is */
    int iGeomCol = -1;
    if (poFeatureDefn->GetGeomFieldCount() == 1)
    {
        int iRawField;
        for( iRawField = 0; iRawField < PQnfields(hInitialResultIn); iRawField++ )
        {
            if( strcmp(PQfname(hInitialResultIn,iRawField),
                    poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) == 0 )
            {
                iGeomCol = iRawField;
                break;
            }
        }
    }

#ifndef PG_PRE74
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
#endif
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

    pszQueryStatement = (char*) CPLMalloc(strlen(pszRawStatement) + strlen(osWHERE) + 40);

    if (strlen(osWHERE) == 0)
        strcpy(pszQueryStatement, pszRawStatement);
    else
        sprintf(pszQueryStatement, "SELECT * FROM (%s) AS ogrpgsubquery %s",
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

int OGRPGResultLayer::GetFeatureCount( int bForce )

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

    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
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
                char* pszComma;
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
                snprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
                while((pszComma = strchr(szBox3D_1, ',')) != NULL)
                    *pszComma = '.';
                snprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
                while((pszComma = strchr(szBox3D_2, ',')) != NULL)
                    *pszComma = '.';
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

            const char* psGetSRIDFct;
            if (poDS->sPostGISVersion.nMajor >= 2)
                psGetSRIDFct = "ST_SRID";
            else
                psGetSRIDFct = "getsrid";

            osGetSRID += "SELECT ";
            osGetSRID += psGetSRIDFct;
            osGetSRID += "(";
            osGetSRID += OGRPGEscapeColumnName(poGFldDefn->GetNameRef());
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
