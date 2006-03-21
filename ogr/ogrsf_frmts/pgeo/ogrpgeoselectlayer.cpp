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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2006/03/21 18:50:56  fwarmerdam
 * dont report SetAttributeFilter error if clearing
 *
 * Revision 1.2  2005/12/16 01:32:26  fwarmerdam
 * removed custom GetExtent on select layer
 *
 * Revision 1.1  2005/09/05 19:34:17  fwarmerdam
 * New
 *
 */

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

    BuildFeatureDefn( "SELECT", poStmt );
}

/************************************************************************/
/*                          ~OGRPGeoSelectLayer()                       */
/************************************************************************/

OGRPGeoSelectLayer::~OGRPGeoSelectLayer()

{
    ClearStatement();
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

OGRFeature *OGRPGeoSelectLayer::GetFeature( long nFeatureId )

{
    return OGRPGeoLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPGeoSelectLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery != NULL && strlen(pszQuery) > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SetAttributeFilter() not supported on ExecuteSQL() results." );
        
        return OGRERR_UNSUPPORTED_OPERATION;
    }
    else
    {
        return OGRERR_NONE;
    }
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

int OGRPGeoSelectLayer::GetFeatureCount( int bForce )

{
    return OGRPGeoLayer::GetFeatureCount( bForce );
}
