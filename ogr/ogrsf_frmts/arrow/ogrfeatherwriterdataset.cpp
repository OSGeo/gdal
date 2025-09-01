/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_feather.h"

#include "../arrow_common/ograrrowwriterlayer.hpp"

/************************************************************************/
/*                       OGRFeatherWriterDataset()                      */
/************************************************************************/

OGRFeatherWriterDataset::OGRFeatherWriterDataset(
    const char *pszFilename,
    const std::shared_ptr<arrow::io::OutputStream> &poOutputStream)
    : m_osFilename(pszFilename),
      m_poMemoryPool(arrow::MemoryPool::CreateDefault()),
      m_poOutputStream(poOutputStream)
{
}

/************************************************************************/
/*                     ~OGRFeatherWriterDataset()                       */
/************************************************************************/

OGRFeatherWriterDataset::~OGRFeatherWriterDataset()
{
    OGRFeatherWriterDataset::Close();
}

/************************************************************************/
/*                                Close()                               */
/************************************************************************/

CPLErr OGRFeatherWriterDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (m_poLayer && !m_poLayer->Close())
        {
            eErr = CE_Failure;
        }

        if (GDALPamDataset::Close() != CE_None)
        {
            eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRFeatherWriterDataset::GetLayerCount()
{
    return m_poLayer ? 1 : 0;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

OGRLayer *OGRFeatherWriterDataset::GetLayer(int idx)
{
    return idx == 0 ? m_poLayer.get() : nullptr;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRFeatherWriterDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return m_poLayer == nullptr;
    if (EQUAL(pszCap, ODsCAddFieldDomain))
        return m_poLayer != nullptr;
    return false;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer *
OGRFeatherWriterDataset::ICreateLayer(const char *pszName,
                                      const OGRGeomFieldDefn *poGeomFieldDefn,
                                      CSLConstList papszOptions)
{
    if (m_poLayer)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Can write only one layer in a Feather file");
        return nullptr;
    }

    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    m_poLayer = std::make_unique<OGRFeatherWriterLayer>(
        this, m_poMemoryPool.get(), m_poOutputStream, pszName);
    if (!m_poLayer->SetOptions(m_osFilename, papszOptions, poSpatialRef,
                               eGType))
    {
        m_poLayer.reset();
        return nullptr;
    }
    return m_poLayer.get();
}

/************************************************************************/
/*                          AddFieldDomain()                            */
/************************************************************************/

bool OGRFeatherWriterDataset::AddFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    if (m_poLayer == nullptr)
    {
        failureReason = "Layer must be created";
        return false;
    }
    return m_poLayer->AddFieldDomain(std::move(domain), failureReason);
}

/************************************************************************/
/*                          GetFieldDomainNames()                       */
/************************************************************************/

std::vector<std::string>
OGRFeatherWriterDataset::GetFieldDomainNames(CSLConstList) const
{
    return m_poLayer ? m_poLayer->GetFieldDomainNames()
                     : std::vector<std::string>();
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

const OGRFieldDomain *
OGRFeatherWriterDataset::GetFieldDomain(const std::string &name) const
{
    return m_poLayer ? m_poLayer->GetFieldDomain(name) : nullptr;
}
