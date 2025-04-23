/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoSelectLayer class, layer access to the results
 *           of a SELECT statement executed via ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_pgeo.h"

/************************************************************************/
/*                          OGRPGeoSelectLayer()                        */
/************************************************************************/

OGRPGeoSelectLayer::OGRPGeoSelectLayer(OGRPGeoDataSource *poDSIn,
                                       CPLODBCStatement *poStmtIn)
    : pszBaseStatement(CPLStrdup(poStmtIn->GetCommand()))
{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;

    poStmt = poStmtIn;

    // Just to make test_ogrsf happy, but would/could need be extended to
    // other cases.
    if (STARTS_WITH_CI(pszBaseStatement, "SELECT * FROM "))
    {

        OGRLayer *poBaseLayer =
            poDSIn->GetLayerByName(pszBaseStatement + strlen("SELECT * FROM "));
        if (poBaseLayer != nullptr)
        {
            poSRS = poBaseLayer->GetSpatialRef();
            if (poSRS != nullptr)
                poSRS->Reference();
        }
    }

    BuildFeatureDefn("SELECT", poStmt);
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
    if (poStmt != nullptr)
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRPGeoSelectLayer::GetStatement()

{
    if (poStmt == nullptr)
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

    CPLDebug("ODBC", "Recreating statement.");
    poStmt = new CPLODBCStatement(poDS->GetSession(), m_nStatementFlags);
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

void OGRPGeoSelectLayer::ResetReading()

{
    if (iNextShapeId != 0)
        ClearStatement();

    OGRPGeoLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGeoSelectLayer::GetFeature(GIntBig nFeatureId)

{
    return OGRPGeoLayer::GetFeature(nFeatureId);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoSelectLayer::TestCapability(const char *pszCap)

{
    return OGRPGeoLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRPGeoSelectLayer::GetFeatureCount(int bForce)

{
    return OGRPGeoLayer::GetFeatureCount(bForce);
}
