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
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_pg.h"

CPL_CVSID("$Id$");

/* These are the OIDs for some builtin types, as returned by PQftype(). */
/* They were copied from pg_type.h in src/include/catalog/pg_type.h */

#define BOOLOID                 16
#define BYTEAOID                17
#define CHAROID                 18
#define NAMEOID                 19
#define INT8OID                 20
#define INT2OID                 21
#define INT2VECTOROID   22
#define INT4OID                 23
#define REGPROCOID              24
#define TEXTOID                 25
#define OIDOID                  26
#define TIDOID          27
#define XIDOID 28
#define CIDOID 29
#define OIDVECTOROID    30
#define FLOAT4OID 700
#define FLOAT8OID 701


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

        if( nTypeOID == CHAROID || nTypeOID == TEXTOID )
        {
            oField.SetType( OFTString );
            oField.SetWidth( PQfsize(hResult, iRawField) );
        }
        else if( nTypeOID == INT8OID 
                 || nTypeOID == INT2OID
                 || nTypeOID == INT4OID )
        {
            oField.SetType( OFTInteger );
        }
        else if( nTypeOID == FLOAT4OID || nTypeOID == FLOAT8OID )
        {
            oField.SetType( OFTReal );
        }
        else /* unknown type */
        {
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
    return nFeatureCount;
}
