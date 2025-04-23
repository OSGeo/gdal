/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_odbc.h"

/************************************************************************/
/*                          OGRODBCSelectLayer()                         */
/************************************************************************/

OGRODBCSelectLayer::OGRODBCSelectLayer(OGRODBCDataSource *poDSIn,
                                       CPLODBCStatement *poStmtIn)
    : pszBaseStatement(CPLStrdup(poStmtIn->GetCommand()))
{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;

    poStmt = poStmtIn;

    BuildFeatureDefn("SELECT", poStmt);
}

/************************************************************************/
/*                          ~OGRODBCSelectLayer()                          */
/************************************************************************/

OGRODBCSelectLayer::~OGRODBCSelectLayer()

{
    ClearStatement();
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRODBCSelectLayer::ClearStatement()

{
    if (poStmt != nullptr)
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRODBCSelectLayer::GetStatement()

{
    if (poStmt == nullptr)
        ResetStatement();

    return poStmt;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRODBCSelectLayer::ResetStatement()

{
    ClearStatement();

    iNextShapeId = 0;

    CPLDebug("OGR_ODBC", "Recreating statement.");
    poStmt = new CPLODBCStatement(poDS->GetSession());
    poStmt->Append(pszBaseStatement);

    if (poStmt->ExecuteSQL())
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

void OGRODBCSelectLayer::ResetReading()

{
    if (iNextShapeId != 0)
        ClearStatement();

    OGRODBCLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRODBCSelectLayer::GetFeature(GIntBig nFeatureId)

{
    return OGRODBCLayer::GetFeature(nFeatureId);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCSelectLayer::TestCapability(const char *pszCap)

{
    return OGRODBCLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                            IGetExtent()                              */
/*                                                                      */
/*      Since SELECT layers currently cannot ever have geometry, we     */
/*      can optimize the IGetExtent() method!                           */
/************************************************************************/

OGRErr OGRODBCSelectLayer::IGetExtent(int, OGREnvelope *, bool)

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

GIntBig OGRODBCSelectLayer::GetFeatureCount(int bForce)

{
    return OGRODBCLayer::GetFeatureCount(bForce);
}
