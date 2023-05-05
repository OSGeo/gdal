/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "zarr.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                          ~ZarrGroupBase()                            */
/************************************************************************/

ZarrGroupBase::~ZarrGroupBase()
{
    // We need to explicitly flush arrays so that the _ARRAY_DIMENSIONS
    // is properly written. As it relies on checking if the dimensions of the
    // array have an indexing variable, then still need to be all alive.
    for (auto &kv : m_oMapMDArrays)
    {
        kv.second->Flush();
    }
}

/************************************************************************/
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetMDArrayNames(CSLConstList) const
{
    if (!m_bDirectoryExplored)
        ExploreDirectory();

    return m_aosArrays;
}

/************************************************************************/
/*                            RegisterArray()                           */
/************************************************************************/

void ZarrGroupBase::RegisterArray(const std::shared_ptr<ZarrArray> &array) const
{
    m_oMapMDArrays[array->GetName()] = array;
    if (std::find(m_aosArrays.begin(), m_aosArrays.end(), array->GetName()) ==
        m_aosArrays.end())
    {
        m_aosArrays.emplace_back(array->GetName());
    }
    array->RegisterGroup(m_pSelf);
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetGroupNames(CSLConstList) const
{
    if (!m_bDirectoryExplored)
        ExploreDirectory();

    return m_aosGroups;
}

/************************************************************************/
/*                  ZarrGroupBase::CreateAttribute()                    */
/************************************************************************/

std::shared_ptr<GDALAttribute> ZarrGroupBase::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (anDimensions.size() >= 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create attributes of dimension >= 2");
        return nullptr;
    }
    LoadAttributes();
    return m_oAttrGroup.CreateAttribute(osName, anDimensions, oDataType,
                                        papszOptions);
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
ZarrGroupBase::GetDimensions(CSLConstList) const
{
    if (!m_bReadFromZMetadata && !m_bDimensionsInstantiated)
    {
        m_bDimensionsInstantiated = true;
        // We need to instantiate arrays to discover dimensions
        const auto aosArrays = GetMDArrayNames();
        for (const auto &osArray : aosArrays)
        {
            OpenMDArray(osArray);
        }
    }

    std::vector<std::shared_ptr<GDALDimension>> oRes;
    for (const auto &oIter : m_oMapDimensions)
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension> ZarrGroupBase::CreateDimension(
    const std::string &osName, const std::string &osType,
    const std::string &osDirection, GUInt64 nSize, CSLConstList)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty dimension name not supported");
        return nullptr;
    }
    GetDimensions(nullptr);

    if (m_oMapDimensions.find(osName) != m_oMapDimensions.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dimension with same name already exists");
        return nullptr;
    }
    auto newDim(std::make_shared<ZarrDimension>(m_poSharedResource, m_pSelf,
                                                GetFullName(), osName, osType,
                                                osDirection, nSize));
    newDim->SetXArrayDimension();
    m_oMapDimensions[osName] = newDim;
    return newDim;
}

/************************************************************************/
/*                          RenameDimension()                           */
/************************************************************************/

bool ZarrGroupBase::RenameDimension(const std::string &osOldName,
                                    const std::string &osNewName)
{
    if (m_oMapDimensions.find(osNewName) != m_oMapDimensions.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dimension with same name already exists");
        return false;
    }
    auto oIter = m_oMapDimensions.find(osOldName);
    auto poDim = oIter->second;
    CPLAssert(oIter != m_oMapDimensions.end());
    m_oMapDimensions.erase(oIter);
    m_oMapDimensions[osNewName] = poDim;
    return true;
}

/************************************************************************/
/*                  ZarrGroupBase::UpdateDimensionSize()                */
/************************************************************************/

void ZarrGroupBase::UpdateDimensionSize(
    const std::shared_ptr<GDALDimension> &poUpdatedDim)
{
    const auto aosGroupNames = GetGroupNames();
    for (const auto &osGroupName : aosGroupNames)
    {
        auto poSubGroup = OpenZarrGroup(osGroupName);
        if (poSubGroup)
        {
            poSubGroup->UpdateDimensionSize(poUpdatedDim);
        }
    }
    const auto aosArrayNames = GetMDArrayNames();
    for (const auto &osArrayName : aosArrayNames)
    {
        // Disable checks that size of variables referenced by _ARRAY_DIMENSIONS
        // are consistent with array shapes, as we are in the middle of updating
        // things
        m_bDimSizeInUpdate = true;
        auto poArray = OpenZarrArray(osArrayName);
        m_bDimSizeInUpdate = false;
        if (poArray)
        {
            for (auto &poDim : poArray->GetDimensions())
            {
                if (poDim->GetFullName() == poUpdatedDim->GetFullName())
                {
                    auto poModifiableDim =
                        std::dynamic_pointer_cast<ZarrDimension>(poDim);
                    CPLAssert(poModifiableDim);
                    poModifiableDim->SetSize(poUpdatedDim->GetSize());
                    poArray->SetDefinitionModified(true);
                }
            }
        }
    }
}

/************************************************************************/
/*                  ZarrGroupBase::NotifyArrayRenamed()                 */
/************************************************************************/

void ZarrGroupBase::NotifyArrayRenamed(const std::string &osOldName,
                                       const std::string &osNewName)
{
    for (auto &osName : m_aosArrays)
    {
        if (osName == osOldName)
        {
            osName = osNewName;
            break;
        }
    }

    auto oIter = m_oMapMDArrays.find(osOldName);
    if (oIter != m_oMapMDArrays.end())
    {
        auto poArray = oIter->second;
        m_oMapMDArrays.erase(oIter);
        m_oMapMDArrays[osNewName] = poArray;
    }
}

/************************************************************************/
/*                         IsValidObjectName()                          */
/************************************************************************/

/* static */
bool ZarrGroupBase::IsValidObjectName(const std::string &osName)
{
    return !(osName.empty() || osName == "." || osName == ".." ||
             osName.find('/') != std::string::npos ||
             osName.find('\\') != std::string::npos ||
             osName.find(':') != std::string::npos ||
             STARTS_WITH(osName.c_str(), ".z"));
}
