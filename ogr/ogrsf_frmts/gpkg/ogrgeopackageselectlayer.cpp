/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageSelectLayer class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_geopackage.h"

/************************************************************************/
/*                        OGRGeoPackageSelectLayer()                    */
/************************************************************************/

OGRGeoPackageSelectLayer::OGRGeoPackageSelectLayer(
    GDALGeoPackageDataset *poDS, const CPLString &osSQLIn,
    sqlite3_stmt *hStmtIn, bool bUseStatementForGetNextFeature,
    bool bEmptyLayer)
    : OGRGeoPackageLayer(poDS)
{
    // Cannot be moved to initializer list because of use of this, which MSVC
    // 2008 doesn't like
    poBehavior = new OGRSQLiteSelectLayerCommonBehaviour(poDS, this, osSQLIn,
                                                         bEmptyLayer);

    BuildFeatureDefn("SELECT", hStmtIn);

    m_bEOF = bEmptyLayer;
    if (bUseStatementForGetNextFeature)
    {
        m_poQueryStatement = hStmtIn;
        m_bDoStep = false;
    }
    else
    {
        sqlite3_finalize(hStmtIn);
    }
}

/************************************************************************/
/*                       ~OGRGeoPackageSelectLayer()                    */
/************************************************************************/

OGRGeoPackageSelectLayer::~OGRGeoPackageSelectLayer()
{
    delete poBehavior;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoPackageSelectLayer::ResetReading()
{
    poBehavior->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageSelectLayer::GetNextFeature()
{
    return poBehavior->GetNextFeature();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

GIntBig OGRGeoPackageSelectLayer::GetFeatureCount(int bForce)
{
    return poBehavior->GetFeatureCount(bForce);
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::ResetStatement()

{
    ClearStatement();

    m_iNextShapeId = 0;
    m_bDoStep = true;

#ifdef DEBUG
    CPLDebug("OGR_GPKG", "prepare_v2(%s)", poBehavior->m_osSQLCurrent.c_str());
#endif

    const int rc =
        sqlite3_prepare_v2(m_poDS->GetDB(), poBehavior->m_osSQLCurrent,
                           static_cast<int>(poBehavior->m_osSQLCurrent.size()),
                           &m_poQueryStatement, nullptr);

    if (rc == SQLITE_OK)
    {
        return OGRERR_NONE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ResetStatement(): sqlite3_prepare_v2(%s):\n  %s",
                 poBehavior->m_osSQLCurrent.c_str(),
                 sqlite3_errmsg(m_poDS->GetDB()));
        m_poQueryStatement = nullptr;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::SetAttributeFilter(const char *pszQuery)
{
    return poBehavior->SetAttributeFilter(pszQuery);
}

/************************************************************************/
/*                         ISetSpatialFilter()                          */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::ISetSpatialFilter(int iGeomField,
                                                   const OGRGeometry *poGeomIn)

{
    return poBehavior->SetSpatialFilter(iGeomField, poGeomIn);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoPackageSelectLayer::TestCapability(const char *pszCap)
{
    return poBehavior->TestCapability(pszCap);
}

/************************************************************************/
/*                             IGetExtent()                             */
/************************************************************************/

OGRErr OGRGeoPackageSelectLayer::IGetExtent(int iGeomField,
                                            OGREnvelope *psExtent, bool bForce)
{
    return poBehavior->GetExtent(iGeomField, psExtent, bForce);
}
