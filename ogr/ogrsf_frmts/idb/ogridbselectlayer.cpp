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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_idb.h"

/************************************************************************/
/*                          OGRIDBSelectLayer()                         */
/************************************************************************/

OGRIDBSelectLayer::OGRIDBSelectLayer(OGRIDBDataSource *poDSIn,
                                     ITCursor *poCurrIn)

{
    poDS = poDSIn;

    iNextShapeId = 0;
    nSRSId = -1;
    poFeatureDefn = nullptr;

    m_poCurr = poCurrIn;
    pszBaseQuery = CPLStrdup(poCurrIn->Command());

    BuildFeatureDefn("SELECT", m_poCurr);
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
    if (m_poCurr != nullptr)
    {
        delete m_poCurr;
        m_poCurr = nullptr;
    }
}

/************************************************************************/
/*                            GetQuery()                            */
/************************************************************************/

ITCursor *OGRIDBSelectLayer::GetQuery()

{
    if (m_poCurr == nullptr)
        ResetQuery();

    return m_poCurr;
}

/************************************************************************/
/*                           ResetQuery()                           */
/************************************************************************/

OGRErr OGRIDBSelectLayer::ResetQuery()

{
    ClearQuery();

    iNextShapeId = 0;

    CPLDebug("OGR_IDB", "Recreating statement.");
    m_poCurr = new ITCursor(*poDS->GetConnection());

    if (m_poCurr->Prepare(pszBaseQuery) && m_poCurr->Open(ITCursor::ReadOnly))
        return OGRERR_NONE;
    else
    {
        delete m_poCurr;
        m_poCurr = nullptr;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIDBSelectLayer::ResetReading()

{
    if (iNextShapeId != 0)
        ClearQuery();

    OGRIDBLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRIDBSelectLayer::GetFeature(GIntBig nFeatureId)

{
    return OGRIDBLayer::GetFeature(nFeatureId);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBSelectLayer::TestCapability(const char *pszCap)

{
    return OGRIDBLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                            IGetExtent()                              */
/*                                                                      */
/*      Since SELECT layers currently cannot ever have geometry, we     */
/*      can optimize the IGetExtent() method!                           */
/************************************************************************/

OGRErr OGRIDBSelectLayer::IGetExtent(int, OGREnvelope *, bool)

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

GIntBig OGRIDBSelectLayer::GetFeatureCount(int bForce)

{
    return OGRIDBLayer::GetFeatureCount(bForce);
}
