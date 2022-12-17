/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

OGRLayer *OGRParquetWriterDataset::ICreateLayer(
    const char *pszName, OGRSpatialReference *poSpatialRef,
    OGRwkbGeometryType eGType, char **papszOptions)
{
    if (m_poLayer)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Can write only one layer in a Parquet file");
        return nullptr;
    }
    m_poLayer = cpl::make_unique<OGRParquetWriterLayer>(
        m_poMemoryPool.get(), m_poOutputStream, pszName);
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
