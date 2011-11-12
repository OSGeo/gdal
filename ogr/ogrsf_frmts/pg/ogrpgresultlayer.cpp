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

    /* We have to get the SRID of the geometry column, so to be able */
    /* to do spatial filtering */
    if (bHasPostGISGeometry)
    {
        CPLString osGetSRID;
        osGetSRID += "SELECT getsrid(";
        osGetSRID += OGRPGEscapeColumnName(pszGeomColumn);
        osGetSRID += ") FROM (";
        osGetSRID += pszRawStatement;
        osGetSRID += ") AS ogrpggetsrid LIMIT 1";

        PGresult* hSRSIdResult = OGRPG_PQexec(poDS->GetPGConn(), osGetSRID );

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
    else if (bHasPostGISGeography)
    {
        // FIXME? But for the moment, PostGIS 1.5 only handles SRID:4326.
        nSRSId = 4326;
    }

    /* Now set the cursor that will fetch the first rows */
    /* This is usefull when used in situations like */
    /* ds->ReleaseResultSet(ds->ExecuteSQL("SELECT AddGeometryColumn(....)")) */
    /* when people don't actually try to get elements */
    SetInitialQueryCursor();
}

/************************************************************************/
/*                          ~OGRPGResultLayer()                          */
/************************************************************************/

OGRPGResultLayer::~OGRPGResultLayer()

{
    CPLFree( pszRawStatement );
}

/************************************************************************/
/*                        ReadResultDefinition()                        */
/*                                                                      */
/*      Build a schema from the current resultset.                      */
/************************************************************************/

OGRFeatureDefn *OGRPGResultLayer::ReadResultDefinition(PGresult *hInitialResultIn)

{
    PGresult            *hResult = hInitialResultIn;

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( "sql_statement" );
    int            iRawField;

    poDefn->Reference();

    for( iRawField = 0; iRawField < PQnfields(hResult); iRawField++ )
    {
        OGRFieldDefn    oField( PQfname(hResult,iRawField), OFTString);
        Oid             nTypeOID;

        nTypeOID = PQftype(hResult,iRawField);
        
        if( EQUAL(oField.GetNameRef(),"ogc_fid") )
        {
            if (bHasFid)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "More than one ogc_fid column was found in the result of the SQL request. Only last one will be used");
            }
            bHasFid = TRUE;
            CPLFree(pszFIDColumn);
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( nTypeOID == poDS->GetGeometryOID()  ||
                 nTypeOID == poDS->GetGeographyOID()  ||
                 EQUAL(oField.GetNameRef(),"ST_AsBinary") ||
                 EQUAL(oField.GetNameRef(),"BinaryBase64") ||
                 EQUAL(oField.GetNameRef(),"ST_AsEWKT") ||
                 EQUAL(oField.GetNameRef(),"ST_AsEWKB") ||
                 EQUAL(oField.GetNameRef(),"EWKBBase64") ||
                 EQUAL(oField.GetNameRef(),"ST_AsText") ||
                 EQUAL(oField.GetNameRef(),"AsBinary") ||
                 EQUAL(oField.GetNameRef(),"asEWKT") ||
                 EQUAL(oField.GetNameRef(),"asEWKB") ||
                 EQUAL(oField.GetNameRef(),"asText") )
        {
            if (bHasPostGISGeometry || bHasPostGISGeography )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "More than one geometry column was found in the result of the SQL request. Only last one will be used");
            }
            if (nTypeOID == poDS->GetGeographyOID())
                bHasPostGISGeography = TRUE;
            else
                bHasPostGISGeometry = TRUE;
            CPLFree(pszGeomColumn);
            pszGeomColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( EQUAL(oField.GetNameRef(),"WKB_GEOMETRY") )
        {
            bHasWkb = TRUE;
            if( nTypeOID == OIDOID )
                bWkbAsOid = TRUE;
            continue;
        }

        if( nTypeOID == BYTEAOID )
        {
            oField.SetType( OFTBinary );
        }
        else if( nTypeOID == CHAROID ||
                 nTypeOID == TEXTOID ||
                 nTypeOID == BPCHAROID ||
                 nTypeOID == VARCHAROID )
        {
            oField.SetType( OFTString );

            /* See http://www.mail-archive.com/pgsql-hackers@postgresql.org/msg57726.html */
            /* nTypmod = width + 4 */
            int nTypmod = PQfmod(hResult, iRawField);
            if (nTypmod >= 4 && (nTypeOID == BPCHAROID ||
                               nTypeOID == VARCHAROID ) )
            {
                oField.SetWidth( nTypmod - 4);
            }
        }
        else if( nTypeOID == BOOLOID )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 1 );
        }
        else if (nTypeOID == INT2OID )
        {
            oField.SetType( OFTInteger );
            oField.SetWidth( 5 );
        }
        else if (nTypeOID == INT4OID )
        {
            oField.SetType( OFTInteger );
        }
        else if ( nTypeOID == INT8OID )
        {
            /* FIXME: OFTInteger can not handle 64bit integers */
            oField.SetType( OFTInteger );
        }
        else if( nTypeOID == FLOAT4OID ||
                 nTypeOID == FLOAT8OID )
        {
            oField.SetType( OFTReal );
        }
        else if( nTypeOID == NUMERICOID )
        {
            /* See http://www.mail-archive.com/pgsql-hackers@postgresql.org/msg57726.html */
            /* typmod = (width << 16) + precision + 4 */
            int nTypmod = PQfmod(hResult, iRawField);
            if (nTypmod >= 4)
            {
                int nWidth = (nTypmod - 4) >> 16;
                int nPrecision = (nTypmod - 4) & 0xFFFF;
                if (nWidth <= 10 && nPrecision == 0)
                {
                    oField.SetType( OFTInteger );
                    oField.SetWidth( nWidth );
                }
                else
                {
                    oField.SetType( OFTReal );
                    oField.SetWidth( nWidth );
                    oField.SetPrecision( nPrecision );
                }
            }
            else
                oField.SetType( OFTReal );
        }
        else if ( nTypeOID == INT4ARRAYOID )
        {
            oField.SetType ( OFTIntegerList );
        }
        else if ( nTypeOID == FLOAT4ARRAYOID ||
                  nTypeOID == FLOAT8ARRAYOID )
        {
            oField.SetType ( OFTRealList );
        }
        else if ( nTypeOID == TEXTARRAYOID ||
                  nTypeOID == BPCHARARRAYOID ||
                  nTypeOID == VARCHARARRAYOID )
        {
            oField.SetType ( OFTStringList );
        }
        else if ( nTypeOID == DATEOID )
        {
            oField.SetType( OFTDate );
        }
        else if ( nTypeOID == TIMEOID )
        {
            oField.SetType( OFTTime );
        }
        else if ( nTypeOID == TIMESTAMPOID ||
                  nTypeOID == TIMESTAMPTZOID )
        {
            /* We can't deserialize properly timestamp with time zone */
            /* with binary cursors */
            if (nTypeOID == TIMESTAMPTZOID)
                bCanUseBinaryCursor = FALSE;

            oField.SetType( OFTDateTime );
        }
        else /* unknown type */
        {
            CPLDebug("PG", "Unhandled OID (%d) for column %d. Defaulting to String.", nTypeOID, iRawField);
            oField.SetType( OFTString );
        }
        
        poDefn->AddFieldDefn( &oField );
    }

    return poDefn;
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
                ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != -2)) && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != -2) && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return (bHasPostGISGeometry && nSRSId != -2) && m_poAttrQuery == NULL;
        
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

    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != -2)
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
        if ((bHasPostGISGeometry || bHasPostGISGeography) && nSRSId != -2)
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
                osWHERE.Printf("WHERE %s && SetSRID('BOX3D(%s, %s)'::box3d,%d) ",
                            OGRPGEscapeColumnName(pszGeomColumn).c_str(), szBox3D_1, szBox3D_2, nSRSId );
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
