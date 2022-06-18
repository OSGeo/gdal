/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
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

#include "ogr_arrow.h"

/************************************************************************/
/*                         OGRArrowDataset()                            */
/************************************************************************/

inline OGRArrowDataset::OGRArrowDataset(const std::shared_ptr<arrow::MemoryPool>& poMemoryPool):
    m_poMemoryPool(poMemoryPool)
{
}

/************************************************************************/
/*                            SetLayer()                                */
/************************************************************************/

inline void OGRArrowDataset::SetLayer(std::unique_ptr<OGRArrowLayer>&& poLayer)
{
    m_poLayer = std::move(poLayer);
}

/************************************************************************/
/*                          RegisterDomainName()                        */
/************************************************************************/

inline void OGRArrowDataset::RegisterDomainName(const std::string& osDomainName, int iFieldIndex)
{
    m_aosDomainNames.push_back(osDomainName);
    m_oMapDomainNameToCol[osDomainName] = iFieldIndex;
}

/************************************************************************/
/*                          GetFieldDomainNames()                       */
/************************************************************************/

inline std::vector<std::string> OGRArrowDataset::GetFieldDomainNames(CSLConstList) const
{
    return m_aosDomainNames;
}

/************************************************************************/
/*                          GetFieldDomain()                            */
/************************************************************************/

inline const OGRFieldDomain* OGRArrowDataset::GetFieldDomain(const std::string& name) const
{
    {
        const auto iter = m_oMapFieldDomains.find(name);
        if( iter != m_oMapFieldDomains.end() )
            return iter->second.get();
    }
    const auto iter = m_oMapDomainNameToCol.find(name);
    if( iter == m_oMapDomainNameToCol.end() )
        return nullptr;
    return m_oMapFieldDomains.insert(
        std::pair<std::string, std::unique_ptr<OGRFieldDomain>>(
            name, m_poLayer->BuildDomain(name, iter->second))).first->second.get();
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

inline OGRLayer* OGRArrowDataset::GetLayer(int idx)
{
    return idx == 0 ? m_poLayer.get() : nullptr;
}
