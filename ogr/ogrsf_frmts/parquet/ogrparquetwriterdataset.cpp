/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowwriterlayer.hpp"

/************************************************************************/
/*                       OGRParquetWriterDataset()                      */
/************************************************************************/

OGRParquetWriterDataset::OGRParquetWriterDataset(
    const std::shared_ptr<arrow::io::OutputStream> &poOutputStream)
    : m_poMemoryPool(arrow::MemoryPool::CreateDefault()),
      m_poOutputStream(poOutputStream)
{
}

/************************************************************************/
/*                                Close()                               */
/************************************************************************/

CPLErr OGRParquetWriterDataset::Close()
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

int OGRParquetWriterDataset::GetLayerCount()
{
    return m_poLayer ? 1 : 0;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

OGRLayer *OGRParquetWriterDataset::GetLayer(int idx)
{
    return idx == 0 ? m_poLayer.get() : nullptr;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetWriterDataset::TestCapability(const char *pszCap)
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
OGRParquetWriterDataset::ICreateLayer(const char *pszName,
                                      const OGRGeomFieldDefn *poGeomFieldDefn,
                                      CSLConstList papszOptions)
{
    if (m_poLayer)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Can write only one layer in a Parquet file");
        return nullptr;
    }

    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    m_poLayer = std::make_unique<OGRParquetWriterLayer>(
        this, m_poMemoryPool.get(), m_poOutputStream, pszName);
    if (!m_poLayer->SetOptions(papszOptions, poSpatialRef, eGType))
    {
        m_poLayer.reset();
        return nullptr;
    }
    return m_poLayer.get();
}

/************************************************************************/
/*                          AddFieldDomain()                            */
/************************************************************************/

bool OGRParquetWriterDataset::AddFieldDomain(
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
OGRParquetWriterDataset::GetFieldDomainNames(CSLConstList) const
{
    return m_poLayer ? m_poLayer->GetFieldDomainNames()
                     : std::vector<std::string>();
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

const OGRFieldDomain *
OGRParquetWriterDataset::GetFieldDomain(const std::string &name) const
{
    return m_poLayer ? m_poLayer->GetFieldDomain(name) : nullptr;
}
