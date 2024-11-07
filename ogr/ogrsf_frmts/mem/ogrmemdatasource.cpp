/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_mem.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                          OGRMemDataSource()                          */
/************************************************************************/

OGRMemDataSource::OGRMemDataSource(const char *pszFilename,
                                   char ** /* papszOptions */)
    : papoLayers(nullptr), nLayers(0)
{
    SetDescription(pszFilename);
}

/************************************************************************/
/*                         ~OGRMemDataSource()                          */
/************************************************************************/

OGRMemDataSource::~OGRMemDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRMemDataSource::ICreateLayer(const char *pszLayerName,
                               const OGRGeomFieldDefn *poGeomFieldDefn,
                               CSLConstList papszOptions)
{
    // Create the layer object.

    const auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRSIn =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    OGRSpatialReference *poSRS = nullptr;
    if (poSRSIn)
    {
        poSRS = poSRSIn->Clone();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    OGRMemLayer *poLayer = new OGRMemLayer(pszLayerName, poSRS, eType);
    if (poSRS)
    {
        poSRS->Release();
    }

    if (CPLFetchBool(papszOptions, "ADVERTIZE_UTF8", false))
        poLayer->SetAdvertizeUTF8(true);

    poLayer->SetDataset(this);
    poLayer->SetFIDColumn(CSLFetchNameValueDef(papszOptions, "FID", ""));

    // Add layer to data source layer list.
    papoLayers = static_cast<OGRMemLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRMemLayer *) * (nLayers + 1)));

    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRMemDataSource::DeleteLayer(int iLayer)

{
    if (iLayer >= 0 && iLayer < nLayers)
    {
        delete papoLayers[iLayer];

        for (int i = iLayer + 1; i < nLayers; ++i)
            papoLayers[i - 1] = papoLayers[i];

        --nLayers;

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMemDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCDeleteLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return TRUE;
    else if (EQUAL(pszCap, ODsCAddFieldDomain))
        return TRUE;
    else if (EQUAL(pszCap, ODsCDeleteFieldDomain))
        return TRUE;
    else if (EQUAL(pszCap, ODsCUpdateFieldDomain))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMemDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                           AddFieldDomain()                           */
/************************************************************************/

bool OGRMemDataSource::AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                      std::string &failureReason)
{
    if (GetFieldDomain(domain->GetName()) != nullptr)
    {
        failureReason = "A domain of identical name already exists";
        return false;
    }
    const std::string domainName(domain->GetName());
    m_oMapFieldDomains[domainName] = std::move(domain);
    return true;
}

/************************************************************************/
/*                           DeleteFieldDomain()                        */
/************************************************************************/

bool OGRMemDataSource::DeleteFieldDomain(const std::string &name,
                                         std::string &failureReason)
{
    const auto iter = m_oMapFieldDomains.find(name);
    if (iter == m_oMapFieldDomains.end())
    {
        failureReason = "Domain does not exist";
        return false;
    }

    m_oMapFieldDomains.erase(iter);

    for (int i = 0; i < nLayers; i++)
    {
        OGRMemLayer *poLayer = papoLayers[i];
        for (int j = 0; j < poLayer->GetLayerDefn()->GetFieldCount(); ++j)
        {
            OGRFieldDefn *poFieldDefn =
                poLayer->GetLayerDefn()->GetFieldDefn(j);
            if (poFieldDefn->GetDomainName() == name)
            {
                auto oTemporaryUnsealer(poFieldDefn->GetTemporaryUnsealer());
                poFieldDefn->SetDomainName(std::string());
            }
        }
    }

    return true;
}

/************************************************************************/
/*                           UpdateFieldDomain()                        */
/************************************************************************/

bool OGRMemDataSource::UpdateFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    const auto iter = m_oMapFieldDomains.find(domainName);
    if (iter == m_oMapFieldDomains.end())
    {
        failureReason = "No matching domain found";
        return false;
    }
    m_oMapFieldDomains[domainName] = std::move(domain);
    return true;
}
