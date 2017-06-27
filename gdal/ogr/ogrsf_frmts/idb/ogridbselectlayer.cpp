/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBSelectLayer class, layer access to the results
 *           of a SELECT statement executed via Open()
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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
#include "ogr_idb.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRIDBSelectLayer()                         */
/************************************************************************/

OGRIDBSelectLayer::OGRIDBSelectLayer( OGRIDBDataSource *poDSIn,
                                        ITCursor * poCurrIn )

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = NULL;

    poCurr = poCurrIn;
    pszBaseQuery = CPLStrdup( poCurrIn->Command() );

    BuildFeatureDefn( "SELECT", poCurr );
}

/************************************************************************/
/*                          ~OGRIDBSelectLayer()                          */
/************************************************************************/

OGRIDBSelectLayer::~OGRIDBSelectLayer()

{
    ClearQuery();
}

/************************************************************************/
/*                           ClearQuery()                           */
/************************************************************************/

void OGRIDBSelectLayer::ClearQuery()

{
    if( poCurr != NULL )
    {
        delete poCurr;
        poCurr = NULL;
    }
}

/************************************************************************/
/*                            GetQuery()                            */
/************************************************************************/

ITCursor *OGRIDBSelectLayer::GetQuery()

{
    if( poCurr == NULL )
        ResetQuery();

    return poCurr;
}

/************************************************************************/
/*                           ResetQuery()                           */
/************************************************************************/

OGRErr OGRIDBSelectLayer::ResetQuery()

{
    ClearQuery();

    iNextShapeId = 0;

    CPLDebug( "OGR_IDB", "Recreating statement." );
    poCurr = new ITCursor( *poDS->GetConnection() );

    if( poCurr->Prepare( pszBaseQuery ) &&
        poCurr->Open( ITCursor::ReadOnly ) )
        return OGRERR_NONE;
    else
    {
        delete poCurr;
        poCurr = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIDBSelectLayer::ResetReading()

{
    if( iNextShapeId != 0 )
        ClearQuery();

    OGRIDBLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRIDBSelectLayer::GetFeature( GIntBig nFeatureId )

{
    return OGRIDBLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBSelectLayer::TestCapability( const char * pszCap )

{
    return OGRIDBLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Since SELECT layers currently cannot ever have geometry, we     */
/*      can optimize the GetExtent() method!                            */
/************************************************************************/

OGRErr OGRIDBSelectLayer::GetExtent(OGREnvelope *, int )

{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRIDBSelectLayer::GetFeatureCount( int bForce )

{
    return OGRIDBLayer::GetFeatureCount( bForce );
}
