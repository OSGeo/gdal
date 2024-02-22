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
    if (!CheckValidAndErrorOutIfNot())
        return {};

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
    array->RegisterGroup(
        std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock()));
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> ZarrGroupBase::GetGroupNames(CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return {};

    if (!m_bDirectoryExplored)
        ExploreDirectory();

    return m_aosGroups;
}

/************************************************************************/
/*                             DeleteGroup()                            */
/************************************************************************/

bool ZarrGroupBase::DeleteGroup(const std::string &osName,
                                CSLConstList /*papszOptions*/)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    GetGroupNames();

    auto oIterNames = std::find(m_aosGroups.begin(), m_aosGroups.end(), osName);
    if (oIterNames == m_aosGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Group %s is not a sub-group of this group", osName.c_str());
        return false;
    }

    const std::string osSubDirName =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if (VSIRmdirRecursive(osSubDirName.c_str()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot delete %s",
                 osSubDirName.c_str());
        return false;
    }

    m_poSharedResource->DeleteZMetadataItemRecursive(osSubDirName);

    m_aosGroups.erase(oIterNames);

    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
    {
        oIter->second->Deleted();
        m_oMapGroups.erase(oIter);
    }

    return true;
}

/************************************************************************/
/*                       NotifyChildrenOfDeletion()                     */
/************************************************************************/

void ZarrGroupBase::NotifyChildrenOfDeletion()
{
    for (const auto &oIter : m_oMapGroups)
        oIter.second->ParentDeleted();

    for (const auto &oIter : m_oMapMDArrays)
        oIter.second->ParentDeleted();

    m_oAttrGroup.ParentDeleted();

    for (const auto &oIter : m_oMapDimensions)
        oIter.second->ParentDeleted();
}

/************************************************************************/
/*                  ZarrGroupBase::CreateAttribute()                    */
/************************************************************************/

std::shared_ptr<GDALAttribute> ZarrGroupBase::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

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
/*                  ZarrGroupBase::DeleteAttribute()                   */
/************************************************************************/

bool ZarrGroupBase::DeleteAttribute(const std::string &osName, CSLConstList)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    LoadAttributes();
    return m_oAttrGroup.DeleteAttribute(osName);
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
ZarrGroupBase::GetDimensions(CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return {};

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
/*                            DeleteMDArray()                           */
/************************************************************************/

bool ZarrGroupBase::DeleteMDArray(const std::string &osName,
                                  CSLConstList /*papszOptions*/)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    GetMDArrayNames();

    auto oIterNames = std::find(m_aosArrays.begin(), m_aosArrays.end(), osName);
    if (oIterNames == m_aosArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array %s is not an array of this group", osName.c_str());
        return false;
    }

    const std::string osSubDirName =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if (VSIRmdirRecursive(osSubDirName.c_str()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot delete %s",
                 osSubDirName.c_str());
        return false;
    }

    m_poSharedResource->DeleteZMetadataItemRecursive(osSubDirName);

    m_aosArrays.erase(oIterNames);

    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
    {
        oIter->second->Deleted();
        m_oMapMDArrays.erase(oIter);
    }

    return true;
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension> ZarrGroupBase::CreateDimension(
    const std::string &osName, const std::string &osType,
    const std::string &osDirection, GUInt64 nSize, CSLConstList)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

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
    auto newDim(std::make_shared<ZarrDimension>(
        m_poSharedResource,
        std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock()), GetFullName(),
        osName, osType, osDirection, nSize));
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
    if (oIter == m_oMapDimensions.end())
    {
        CPLAssert(false);
        return false;
    }
    auto poDim = std::move(oIter->second);
    m_oMapDimensions.erase(oIter);
    m_oMapDimensions[osNewName] = std::move(poDim);
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
        auto poArray = std::move(oIter->second);
        m_oMapMDArrays.erase(oIter);
        m_oMapMDArrays[osNewName] = std::move(poArray);
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

/************************************************************************/
/*                 CheckArrayOrGroupWithSameNameDoesNotExist()          */
/************************************************************************/

bool ZarrGroupBase::CheckArrayOrGroupWithSameNameDoesNotExist(
    const std::string &osName) const
{
    const auto groupNames = GetGroupNames();
    if (std::find(groupNames.begin(), groupNames.end(), osName) !=
        groupNames.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return false;
    }

    const auto arrayNames = GetMDArrayNames();
    if (std::find(arrayNames.begin(), arrayNames.end(), osName) !=
        arrayNames.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return false;
    }

    return true;
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool ZarrGroupBase::Rename(const std::string &osNewName)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }
    if (!IsValidObjectName(osNewName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid group name");
        return false;
    }
    if (m_osName == "/")
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot rename root group");
        return false;
    }

    auto pParent = std::dynamic_pointer_cast<ZarrGroupBase>(m_poParent.lock());
    if (pParent)
    {
        if (!pParent->CheckArrayOrGroupWithSameNameDoesNotExist(osNewName))
            return false;
    }

    std::string osNewDirectoryName(m_osDirectoryName);
    osNewDirectoryName.resize(osNewDirectoryName.size() - m_osName.size());
    osNewDirectoryName += osNewName;

    if (VSIRename(m_osDirectoryName.c_str(), osNewDirectoryName.c_str()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Renaming of %s to %s failed",
                 m_osDirectoryName.c_str(), osNewDirectoryName.c_str());
        return false;
    }

    if (pParent)
    {
        auto oIter = pParent->m_oMapGroups.find(m_osName);
        if (oIter != pParent->m_oMapGroups.end())
        {
            pParent->m_oMapGroups.erase(oIter);
            CPLAssert(m_pSelf.lock());
            pParent->m_oMapGroups[osNewName] =
                std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
        }

        for (auto &osName : pParent->m_aosGroups)
        {
            if (osName == m_osName)
            {
                osName = osNewName;
                break;
            }
        }
    }

    m_poSharedResource->RenameZMetadataRecursive(m_osDirectoryName,
                                                 osNewDirectoryName);

    m_osDirectoryName = std::move(osNewDirectoryName);

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                          ParentRenamed()                             */
/************************************************************************/

void ZarrGroupBase::ParentRenamed(const std::string &osNewParentFullName)
{
    auto pParent = std::dynamic_pointer_cast<ZarrGroupBase>(m_poParent.lock());
    // The parent necessarily exist, since it notified us
    CPLAssert(pParent);

    m_osDirectoryName = CPLFormFilename(pParent->m_osDirectoryName.c_str(),
                                        m_osName.c_str(), nullptr);

    GDALGroup::ParentRenamed(osNewParentFullName);
}

/************************************************************************/
/*                       NotifyChildrenOfRenaming()                     */
/************************************************************************/

void ZarrGroupBase::NotifyChildrenOfRenaming()
{
    for (const auto &oIter : m_oMapGroups)
        oIter.second->ParentRenamed(m_osFullName);

    for (const auto &oIter : m_oMapMDArrays)
        oIter.second->ParentRenamed(m_osFullName);

    m_oAttrGroup.ParentRenamed(m_osFullName);

    for (const auto &oIter : m_oMapDimensions)
        oIter.second->ParentRenamed(m_osFullName);
}
