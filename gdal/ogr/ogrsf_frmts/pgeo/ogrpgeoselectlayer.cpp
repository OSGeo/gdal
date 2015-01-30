/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_pgeo.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRPGeoSelectLayer()                        */
/************************************************************************/

OGRPGeoSelectLayer::OGRPGeoSelectLayer( OGRPGeoDataSource *poDSIn,
                                        CPLODBCStatement * poStmtIn )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;

    poStmt = poStmtIn;
    pszBaseStatement = CPLStrdup( poStmtIn->GetCommand() );
    
    /* Just to make test_ogrsf happy, but would/could need be extended to */
    /* other cases */
    if( EQUALN(pszBaseStatement, "SELECT * FROM ", strlen("SELECT * FROM ")) )
    {
        
        OGRLayer* poBaseLayer =
            poDSIn->GetLayerByName(pszBaseStatement + strlen("SELECT * FROM "));
        if( poBaseLayer != NULL )
        {
            poSRS = poBaseLayer->GetSpatialRef();
            if( poSRS != NULL )
                poSRS->Reference();
        }
    }

    BuildFeatureDefn( "SELECT", poStmt );
}

/************************************************************************/
/*                          ~OGRPGeoSelectLayer()                       */
/************************************************************************/

OGRPGeoSelectLayer::~OGRPGeoSelectLayer()

{
    ClearStatement();
    CPLFree(pszBaseStatement);
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRPGeoSelectLayer::ClearStatement()

{
    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRPGeoSelectLayer::GetStatement()

{
    if( poStmt == NULL )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRPGeoSelectLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    CPLDebug( "ODBC", "Recreating statement." );
    poStmt = new CPLODBCStatement( poDS->GetSession() );
    poStmt->Append( pszBaseStatement );

    if( poStmt->ExecuteSQL() )
        return OGRERR_NONE;
    else
    {
        delete poStmt;
        poStmt = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGeoSelectLayer::ResetReading()

{
    if( iNextShapeId != 0 )
        ClearStatement();

    OGRPGeoLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGeoSelectLayer::GetFeature( GIntBig nFeatureId )

{
    return OGRPGeoLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoSelectLayer::TestCapability( const char * pszCap )

{
    return OGRPGeoLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRPGeoSelectLayer::GetFeatureCount( int bForce )

{
    return OGRPGeoLayer::GetFeatureCount( bForce );
}
