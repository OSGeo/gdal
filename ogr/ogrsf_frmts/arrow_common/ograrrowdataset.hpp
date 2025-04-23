/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGARROWDATASET_HPP_INCLUDED
#define OGARROWDATASET_HPP_INCLUDED

#include "ogr_arrow.h"

/************************************************************************/
/*                         OGRArrowDataset()                            */
/************************************************************************/

inline OGRArrowDataset::OGRArrowDataset(
    const std::shared_ptr<arrow::MemoryPool> &poMemoryPool)
    : m_poMemoryPool(poMemoryPool)
{
}

/************************************************************************/
/*                            SetLayer()                                */
/************************************************************************/

inline void OGRArrowDataset::SetLayer(std::unique_ptr<OGRArrowLayer> &&poLayer)
{
    m_poLayer = std::move(poLayer);
}

/************************************************************************/
/*                          RegisterDomainName()                        */
/************************************************************************/

inline void OGRArrowDataset::RegisterDomainName(const std::string &osDomainName,
                                                int iFieldIndex)
{
    m_aosDomainNames.push_back(osDomainName);
    m_oMapDomainNameToCol[osDomainName] = iFieldIndex;
}

/************************************************************************/
/*                          GetFieldDomainNames()                       */
/************************************************************************/

inline std::vector<std::string>
OGRArrowDataset::GetFieldDomainNames(CSLConstList) const
{
    return m_aosDomainNames;
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

inline const OGRFieldDomain *
OGRArrowDataset::GetFieldDomain(const std::string &name) const
{
    {
        const auto iter = m_oMapFieldDomains.find(name);
        if (iter != m_oMapFieldDomains.end())
            return iter->second.get();
    }
    const auto iter = m_oMapDomainNameToCol.find(name);
    if (iter == m_oMapDomainNameToCol.end())
        return nullptr;
    return m_oMapFieldDomains
        .insert(std::pair(name, m_poLayer->BuildDomain(name, iter->second)))
        .first->second.get();
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

inline int OGRArrowDataset::GetLayerCount()
{
    return m_poLayer ? 1 : 0;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

inline OGRLayer *OGRArrowDataset::GetLayer(int idx)
{
    return idx == 0 ? m_poLayer.get() : nullptr;
}

#endif /* OGARROWDATASET_HPP_INCLUDED */
