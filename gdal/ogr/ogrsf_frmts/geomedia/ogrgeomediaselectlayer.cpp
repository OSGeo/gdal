/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeomediaSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_geomedia.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRGeomediaSelectLayer()                      */
/************************************************************************/

OGRGeomediaSelectLayer::OGRGeomediaSelectLayer( OGRGeomediaDataSource *poDSIn,
                                                CPLODBCStatement * poStmtIn ) :
    pszBaseStatement(CPLStrdup( poStmtIn->GetCommand() ))
{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;

    poStmt = poStmtIn;

    BuildFeatureDefn( "SELECT", poStmt );
}

/************************************************************************/
/*                        ~OGRGeomediaSelectLayer()                     */
/************************************************************************/

OGRGeomediaSelectLayer::~OGRGeomediaSelectLayer()

{
    ClearStatement();
    CPLFree(pszBaseStatement);
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRGeomediaSelectLayer::ClearStatement()

{
    if( poStmt != nullptr )
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRGeomediaSelectLayer::GetStatement()

{
    if( poStmt == nullptr )
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeomediaSelectLayer::ResetStatement()

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
        poStmt = nullptr;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeomediaSelectLayer::ResetReading()

{
    if( iNextShapeId != 0 )
        ClearStatement();

    OGRGeomediaLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGeomediaSelectLayer::GetFeature( GIntBig nFeatureId )

{
    return OGRGeomediaLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeomediaSelectLayer::TestCapability( const char * pszCap )

{
    return OGRGeomediaLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRGeomediaSelectLayer::GetFeatureCount( int bForce )

{
    return OGRGeomediaLayer::GetFeatureCount( bForce );
}
