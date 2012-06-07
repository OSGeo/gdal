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

    poFeatureDefn = ReadResultDefinition(hInitialResultIn);

    pszGeomTableName = NULL;
    pszGeomTableSchemaName = NULL;

    /* Find at which index the geometry column is */
    int iGeomCol = -1;
    if (pszGeomColumn != NULL)
    {
        int iRawField;
        for( iRawField = 0; iRawField < PQnfields(hInitialResultIn); iRawField++ )
        {
            if( strcmp(PQfname(hInitialResultIn,iRawField), pszGeomColumn) == 0 )
            {
                iGeomCol = iRawField;
                break;
            }
        }
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

    if (bHasPostGISGeography)
    {
        // FIXME? But for the moment, PostGIS 1.5 only handles SRID:4326.
        nSRSId = 4326;
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
    if( EQUAL(pszCap,OLCFastFeatureCount) ||
        EQUAL(pszCap,OLCFastSetNextByIndex) )
        return (m_poFilterGeom == NULL || 
                ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != UNDETERMINED_SRID)) && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != UNDETERMINED_SRID) && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return (bHasPostGISGeometry && nSRSId != UNDETERMINED_SRID) && m_poAttrQuery == NULL;
        
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
    if( m_poFilterGeom != NULL &&
        (bHasPostGISGeometry || bHasPostGISGeography) && nSRSId == UNDETERMINED_SRID )
    {
        GetSpatialRef(); /* make sure that we fetch the SRID if not already done */
    }

    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != UNDETERMINED_SRID)
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPGResultLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        if( (bHasPostGISGeometry || bHasPostGISGeography) && nSRSId == UNDETERMINED_SRID )
        {
            GetSpatialRef(); /* make sure that we fetch the SRID if not already done */
        }

        if ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != UNDETERMINED_SRID)
        {
            if( m_poFilterGeom != NULL)
            {
                char szBox3D_1[128];
                char szBox3D_2[128];
                char* pszComma;
                OGREnvelope  sEnvelope;

                m_poFilterGeom->getEnvelope( &sEnvelope );
                snprintf(szBox3D_1, sizeof(szBox3D_1), "%.12f %.12f", sEnvelope.MinX, sEnvelope.MinY);
                while((pszComma = strchr(szBox3D_1, ',')) != NULL)
                    *pszComma = '.';
                snprintf(szBox3D_2, sizeof(szBox3D_2), "%.12f %.12f", sEnvelope.MaxX, sEnvelope.MaxY);
                while((pszComma = strchr(szBox3D_2, ',')) != NULL)
                    *pszComma = '.';
                osWHERE.Printf("WHERE %s && %s('BOX3D(%s, %s)'::box3d,%d) ",
                               OGRPGEscapeColumnName(pszGeomColumn).c_str(),
                               (poDS->sPostGISVersion.nMajor >= 2) ? "ST_SetSRID" : "SetSRID",
                               szBox3D_1, szBox3D_2, nSRSId );
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
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRPGResultLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    CPLString   osCommand;

    const char* pszExtentFct;
    if (poDS->sPostGISVersion.nMajor >= 2)
        pszExtentFct = "ST_Extent";
    else
        pszExtentFct = "Extent";

    if( (bHasPostGISGeometry || bHasPostGISGeography) && nSRSId == UNDETERMINED_SRID )
    {
        GetSpatialRef(); /* make sure that we fetch the SRID if not already done */
    }

    if ( TestCapability(OLCFastGetExtent) )
    {
        /* Do not take the spatial filter into account */
        osCommand.Printf( "SELECT %s(%s) FROM (%s) AS ogrpgextent",
                          pszExtentFct, OGRPGEscapeColumnName(pszGeomColumn).c_str(),
                          pszRawStatement );
    }
    else if ( bHasPostGISGeography )
    {
        /* Probably not very efficient, but more efficient than client-side implementation */
        osCommand.Printf( "SELECT %s(ST_GeomFromWKB(ST_AsBinary(%s))) FROM (%s) AS ogrpgextent",
                          pszExtentFct, OGRPGEscapeColumnName(pszGeomColumn).c_str(),
                          pszRawStatement );
    }
    
    return RunGetExtentRequest(psExtent, bForce, osCommand);
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is UNDETERMINED_SRID        */
/*      (meaning we haven't yet even looked for it).                    */
/************************************************************************/

OGRSpatialReference *OGRPGResultLayer::GetSpatialRef()

{
    if( nSRSId == UNDETERMINED_SRID )
    {
        /* We have to get the SRID of the geometry column, so to be able */
        /* to do spatial filtering */
        if (bHasPostGISGeometry)
        {
            if (pszGeomTableName != NULL)
            {
                CPLString osName(pszGeomTableSchemaName);
                osName += ".";
                osName += pszGeomTableName;
                OGRPGLayer* poBaseLayer = (OGRPGLayer*) poDS->GetLayerByName(osName);
                if (poBaseLayer)
                {
                    nSRSId = poBaseLayer->GetSRID();
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
                osGetSRID += OGRPGEscapeColumnName(pszGeomColumn);
                osGetSRID += ") FROM(";
                osGetSRID += pszRawStatement;
                osGetSRID += ") AS ogrpggetsrid LIMIT 1";

                PGresult* hSRSIdResult = OGRPG_PQexec(poDS->GetPGConn(), osGetSRID );

                nSRSId = -1;

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
    }

    return OGRPGLayer::GetSpatialRef();
}
