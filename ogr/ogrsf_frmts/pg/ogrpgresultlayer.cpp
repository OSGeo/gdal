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

    /* Eventually we may need to make these a unique name */
    pszCursorName = "OGRPGResultLayerReader";
    hCursorResult = hInitialResultIn;
    hInitialResult = hInitialResultIn;

    BuildFullQueryStatement();

    nFeatureCount = PQntuples(hInitialResultIn);

    poFeatureDefn = ReadResultDefinition();
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

OGRFeatureDefn *OGRPGResultLayer::ReadResultDefinition()

{
    PGresult            *hResult = hInitialResult;

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( "sql_statement" );
    int            iRawField;

    poDefn->Reference();

    for( iRawField = 0; iRawField < PQnfields(hInitialResult); iRawField++ )
    {
        OGRFieldDefn    oField( PQfname(hResult,iRawField), OFTString);
        Oid             nTypeOID;

        nTypeOID = PQftype(hResult,iRawField);
        
        if( EQUAL(oField.GetNameRef(),"ogc_fid") )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
        else if( nTypeOID == poDS->GetGeometryOID()  ||
                 EQUAL(oField.GetNameRef(),"asEWKT") ||
                 EQUAL(oField.GetNameRef(),"asText") )
        {
            bHasPostGISGeometry = TRUE;
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
            oField.SetWidth( PQfsize(hResult, iRawField) );
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
                 nTypeOID == FLOAT8OID ||
                 nTypeOID == NUMERICOID )
        {
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

    /* Eventually we should consider trying to "insert" the spatial component
       of the query if possible within a SELECT, but for now we just use
       the raw query directly. */

    pszQueryStatement = CPLStrdup(pszRawStatement);
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
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return nFeatureCount;
    else
        return OGRLayer::GetFeatureCount(bForce);
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGResultLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;
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
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}
