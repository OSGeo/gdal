/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB multidimensional support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#include "tiledbmultidim.h"

#include "memmultidim.h"

/************************************************************************/
/*                   TileDBGroup::~TileDBGroup()                        */
/************************************************************************/

TileDBGroup::~TileDBGroup()
{
    m_oMapGroups.clear();
    m_oMapArrays.clear();
    if (m_poTileDBGroup)
    {
        try
        {
            m_poTileDBGroup->close();
            m_poTileDBGroup.reset();
        }
        catch (const std::exception &e)
        {
            // Will leak memory, but better that than crashing
            // Cf https://github.com/TileDB-Inc/TileDB/issues/4101
            m_poTileDBGroup.release();
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TileDBGroup::~TileDBGroup(): %s", e.what());
        }
    }
}

/************************************************************************/
/*                   TileDBGroup::GetGroupNames()                       */
/************************************************************************/

std::vector<std::string>
TileDBGroup::GetGroupNames(CSLConstList /*papszOptions*/) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return {};

    std::vector<std::string> aosNames;
    for (uint64_t i = 0; i < m_poTileDBGroup->member_count(); ++i)
    {
        auto obj = m_poTileDBGroup->member(i);
        if (obj.type() == tiledb::Object::Type::Group)
        {
            if (obj.name().has_value())
                aosNames.push_back(*(obj.name()));
            else
                aosNames.push_back(CPLGetFilename(obj.uri().c_str()));
        }
    }
    return aosNames;
}

/************************************************************************/
/*                   TileDBGroup::OpenFromDisk()                        */
/************************************************************************/

/* static */
std::shared_ptr<TileDBGroup> TileDBGroup::OpenFromDisk(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::string &osPath)
{
    const auto eType =
        tiledb::Object::object(poSharedResource->GetCtx(), osPath).type();
    if (eType != tiledb::Object::Type::Group)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s is not a TileDB group",
                 osPath.c_str());
        return nullptr;
    }

    auto poTileDBGroup = std::make_unique<tiledb::Group>(
        poSharedResource->GetCtx(), osPath, TILEDB_READ);

    auto poGroup =
        TileDBGroup::Create(poSharedResource, osParentName, osName, osPath);
    poGroup->m_poTileDBGroup = std::move(poTileDBGroup);
    return poGroup;
}

/************************************************************************/
/*                   TileDBGroup::CreateOnDisk()                        */
/************************************************************************/

/* static */
std::shared_ptr<TileDBGroup> TileDBGroup::CreateOnDisk(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::string &osPath)
{
    try
    {
        tiledb::create_group(poSharedResource->GetCtx(), osPath);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return nullptr;
    }

    auto poTileDBGroup = std::make_unique<tiledb::Group>(
        poSharedResource->GetCtx(), osPath, TILEDB_WRITE);

    auto poGroup =
        TileDBGroup::Create(poSharedResource, osParentName, osName, osPath);
    poGroup->m_poTileDBGroup = std::move(poTileDBGroup);
    return poGroup;
}

/************************************************************************/
/*                   TileDBGroup::EnsureOpenAs()                        */
/************************************************************************/

bool TileDBGroup::EnsureOpenAs(tiledb_query_type_t mode) const
{
    if (!m_poTileDBGroup)
        return false;
    if (m_poTileDBGroup->query_type() == mode && m_poTileDBGroup->is_open())
        return true;
    try
    {
        m_poTileDBGroup->close();
        m_poTileDBGroup->open(mode);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        m_poTileDBGroup.reset();
        return false;
    }
    return true;
}

/************************************************************************/
/*                 TileDBGroup::HasObjectOfSameName()                   */
/************************************************************************/

bool TileDBGroup::HasObjectOfSameName(const std::string &osName) const
{
    if (m_oMapGroups.find(osName) != m_oMapGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "A group named %s already exists",
                 osName.c_str());
        return true;
    }
    if (m_oMapArrays.find(osName) != m_oMapArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array named %s already exists", osName.c_str());
        return true;
    }

    if (!EnsureOpenAs(TILEDB_READ))
        return {};
    for (uint64_t i = 0; i < m_poTileDBGroup->member_count(); ++i)
    {
        auto obj = m_poTileDBGroup->member(i);
        std::string osObjName = obj.name().has_value()
                                    ? *(obj.name())
                                    : CPLGetFilename(obj.uri().c_str());
        if (osName == osObjName)
        {
            if (obj.type() == tiledb::Object::Type::Group)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "A group named %s already exists", osName.c_str());
                return true;
            }
            else if (obj.type() == tiledb::Object::Type::Array)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "An array named %s already exists", osName.c_str());
                return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*                   TileDBGroup::OpenGroup()                           */
/************************************************************************/

std::shared_ptr<GDALGroup>
TileDBGroup::OpenGroup(const std::string &osName,
                       CSLConstList /*papszOptions*/) const
{
    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
    {
        return oIter->second;
    }
    if (!m_poTileDBGroup)
        return nullptr;

    // Try to match by member name property first, and if not use the
    // last part of their URI
    std::string osSubPath;
    std::string osSubPathCandidate;
    for (uint64_t i = 0; i < m_poTileDBGroup->member_count(); ++i)
    {
        auto obj = m_poTileDBGroup->member(i);
        if (obj.type() == tiledb::Object::Type::Group)
        {
            if (obj.name().has_value() && *(obj.name()) == osName)
            {
                osSubPath = obj.uri();
                break;
            }
            else if (CPLGetFilename(obj.uri().c_str()) == osName)
            {
                osSubPathCandidate = osSubPath;
            }
        }
    }
    if (osSubPath.empty())
        osSubPath = std::move(osSubPathCandidate);
    if (osSubPath.empty())
        return nullptr;

    auto poSubGroup = TileDBGroup::OpenFromDisk(
        m_poSharedResource, m_osFullName, osName, osSubPath);
    if (!poSubGroup)
        return nullptr;

    m_oMapGroups[osName] = poSubGroup;

    return poSubGroup;
}

/************************************************************************/
/*                   TileDBGroup::CreateGroup()                         */
/************************************************************************/

std::shared_ptr<GDALGroup> TileDBGroup::CreateGroup(const std::string &osName,
                                                    CSLConstList papszOptions)
{
    if (!m_poSharedResource->IsUpdatable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }

    if (HasObjectOfSameName(osName))
        return nullptr;

    std::string osSubPath = m_poTileDBGroup->uri() + "/" +
                            TileDBSharedResource::SanitizeNameForPath(osName);
    const char *pszURI = CSLFetchNameValue(papszOptions, "URI");
    if (pszURI)
        osSubPath = pszURI;
    auto poSubGroup =
        CreateOnDisk(m_poSharedResource, m_osFullName, osName, osSubPath);
    if (!poSubGroup)
        return nullptr;

    if (!AddMember(osSubPath, osName))
        return nullptr;
    m_oMapGroups[osName] = poSubGroup;

    return poSubGroup;
}

/************************************************************************/
/*                   TileDBGroup::AddMember()                           */
/************************************************************************/

bool TileDBGroup::AddMember(const std::string &osPath,
                            const std::string &osName)
{
    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;

    try
    {
        m_poTileDBGroup->add_member(osPath, /* relative= */ false, osName);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AddMember() failed with: %s",
                 e.what());
        return false;
    }
    // Force close() and re-open() to avoid
    // https://github.com/TileDB-Inc/TileDB/issues/4101
    try
    {
        m_poTileDBGroup->close();
        m_poTileDBGroup->open(TILEDB_WRITE);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AddMember() failed with: %s",
                 e.what());
        m_poTileDBGroup.reset();
        return false;
    }
    return true;
}

/************************************************************************/
/*                    TileDBGroup::CreateDimension()                    */
/************************************************************************/

std::shared_ptr<GDALDimension> TileDBGroup::CreateDimension(
    const std::string &osName, const std::string &osType,
    const std::string &osDirection, GUInt64 nSize, CSLConstList)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty dimension name not supported");
        return nullptr;
    }

    if (m_oMapDimensions.find(osName) != m_oMapDimensions.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dimension with same name already exists");
        return nullptr;
    }
    auto newDim(std::make_shared<GDALDimensionWeakIndexingVar>(
        GetFullName(), osName, osType, osDirection, nSize));
    m_oMapDimensions[osName] = newDim;
    return newDim;
}

/************************************************************************/
/*                   TileDBGroup::GetMDArrayNames()                     */
/************************************************************************/

std::vector<std::string>
TileDBGroup::GetMDArrayNames(CSLConstList /*papszOptions */) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return {};

    std::vector<std::string> aosNames;
    for (uint64_t i = 0; i < m_poTileDBGroup->member_count(); ++i)
    {
        auto obj = m_poTileDBGroup->member(i);
        if (obj.type() == tiledb::Object::Type::Array)
        {
            tiledb::ArraySchema schema(m_poSharedResource->GetCtx(), obj.uri());
            if (schema.array_type() == TILEDB_DENSE)
            {
                const std::string osName =
                    obj.name().has_value() ? *(obj.name())
                                           : CPLGetFilename(obj.uri().c_str());
                const auto nAttributes = schema.attribute_num();
                if (nAttributes != 1)
                {
                    for (uint32_t iAttr = 0; iAttr < nAttributes; ++iAttr)
                    {
                        aosNames.push_back(osName + "." +
                                           schema.attribute(iAttr).name());
                    }
                }
                else
                {
                    aosNames.push_back(osName);
                }
            }
        }
    }

    // As array creation is deferred, the above loop didn't get freshly
    // created arrays
    for (const auto &kv : m_oMapArrays)
    {
        if (std::find(aosNames.begin(), aosNames.end(), kv.first) ==
            aosNames.end())
        {
            aosNames.push_back(kv.first);
        }
    }

    return aosNames;
}

/************************************************************************/
/*                   TileDBGroup::OpenMDArray()                         */
/************************************************************************/

std::shared_ptr<GDALMDArray>
TileDBGroup::OpenMDArray(const std::string &osName,
                         CSLConstList papszOptions) const
{
    auto oIter = m_oMapArrays.find(osName);
    if (oIter != m_oMapArrays.end())
    {
        return oIter->second;
    }
    if (!m_poTileDBGroup)
        return nullptr;

    std::string osNamePrefix = osName;
    std::string osNameSuffix;
    const auto nLastDot = osName.rfind('.');
    if (nLastDot != std::string::npos)
    {
        osNamePrefix = osName.substr(0, nLastDot);
        osNameSuffix = osName.substr(nLastDot + 1);
    }

    // Try to match by member name property first, and if not use the
    // last part of their URI
    std::string osSubPath;
    std::string osSubPathCandidate;
    for (uint64_t i = 0; i < m_poTileDBGroup->member_count(); ++i)
    {
        auto obj = m_poTileDBGroup->member(i);

        const auto MatchNameSuffix =
            [this, &obj, &osName, &osNamePrefix,
             &osNameSuffix](const std::string &osObjName)
        {
            tiledb::ArraySchema schema(m_poSharedResource->GetCtx(), obj.uri());
            if (osNameSuffix.empty() && osObjName == osName)
            {
                return true;
            }
            else if (osObjName == osNamePrefix && !osNameSuffix.empty() &&
                     schema.has_attribute(osNameSuffix))
            {
                return true;
            }
            return false;
        };

        if (obj.type() == tiledb::Object::Type::Array)
        {
            if (obj.name().has_value() && MatchNameSuffix(*(obj.name())))
            {
                osSubPath = obj.uri();
                break;
            }
            else if (MatchNameSuffix(CPLGetFilename(obj.uri().c_str())))
            {
                osSubPathCandidate = obj.uri();
            }
        }
    }
    if (osSubPath.empty())
        osSubPath = std::move(osSubPathCandidate);
    if (osSubPath.empty())
        return nullptr;

    if (m_oSetArrayInOpening.find(osName) != m_oSetArrayInOpening.end())
        return nullptr;
    m_oSetArrayInOpening.insert(osName);
    auto poArray = TileDBArray::OpenFromDisk(m_poSharedResource, m_pSelf.lock(),
                                             m_osFullName, osName, osNameSuffix,
                                             osSubPath, papszOptions);
    m_oSetArrayInOpening.erase(osName);
    if (!poArray)
        return nullptr;

    m_oMapArrays[osName] = poArray;

    return poArray;
}

/************************************************************************/
/*                   TileDBGroup::CreateMDArray()                       */
/************************************************************************/

std::shared_ptr<GDALMDArray> TileDBGroup::CreateMDArray(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "IN_MEMORY", "NO")))
    {
        auto poArray =
            MEMMDArray::Create(std::string(), osName, aoDimensions, oDataType);
        if (!poArray || !poArray->Init())
            return nullptr;
        return poArray;
    }

    if (!m_poSharedResource->IsUpdatable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }

    if (HasObjectOfSameName(osName))
        return nullptr;

    if (!EnsureOpenAs(TILEDB_WRITE))
        return nullptr;

    auto poSelf = std::dynamic_pointer_cast<TileDBGroup>(m_pSelf.lock());
    CPLAssert(poSelf);
    auto poArray =
        TileDBArray::CreateOnDisk(m_poSharedResource, poSelf, osName,
                                  aoDimensions, oDataType, papszOptions);
    if (!poArray)
        return nullptr;

    m_oMapArrays[osName] = poArray;
    return poArray;
}

/************************************************************************/
/*                  TileDBGroup::CreateAttribute()                      */
/************************************************************************/

std::shared_ptr<GDALAttribute> TileDBGroup::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    return CreateAttributeImpl(osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                   TileDBGroup::GetAttribute()                        */
/************************************************************************/

std::shared_ptr<GDALAttribute>
TileDBGroup::GetAttribute(const std::string &osName) const
{
    return GetAttributeImpl(osName);
}

/************************************************************************/
/*                   TileDBGroup::GetAttributes()                       */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
TileDBGroup::GetAttributes(CSLConstList /* papszOptions */) const
{
    return GetAttributesImpl();
}

/************************************************************************/
/*                   TileDBGroup::DeleteAttribute()                     */
/************************************************************************/

bool TileDBGroup::DeleteAttribute(const std::string &osName,
                                  CSLConstList papszOptions)
{
    return DeleteAttributeImpl(osName, papszOptions);
}
