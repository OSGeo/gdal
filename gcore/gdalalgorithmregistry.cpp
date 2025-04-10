/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALAlgorithm class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalgorithm.h"
#include "gdalalg_main.h"

#include "cpl_vsi.h"

#include "gdal_priv.h"

#include <cassert>

/************************************************************************/
/*              GDALAlgorithmRegistry::~GDALAlgorithmRegistry()         */
/************************************************************************/

GDALAlgorithmRegistry::~GDALAlgorithmRegistry() = default;

/************************************************************************/
/*                GDALAlgorithmRegistry::Register()                     */
/************************************************************************/

bool GDALAlgorithmRegistry::Register(const GDALAlgorithmRegistry::AlgInfo &info)
{
    if (cpl::contains(m_mapNameToInfo, info.m_name))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDAL algorithm '%s' already registered!",
                 info.m_name.c_str());
        return false;
    }
    for (const std::string &alias : info.m_aliases)
    {
        if (cpl::contains(m_mapAliasToInfo, alias) ||
            cpl::contains(m_mapHiddenAliasToInfo, alias))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "An algorithm with alias '%s' is already registered!",
                     alias.c_str());
            return false;
        }
    }
    m_mapNameToInfo[info.m_name] = info;
    bool hidden = false;
    for (const std::string &alias : info.m_aliases)
    {
        if (alias == HIDDEN_ALIAS_SEPARATOR)
            hidden = true;
        else if (hidden)
            m_mapAliasToInfo[alias] = info;
        else
            m_mapHiddenAliasToInfo[alias] = info;
    }
    return true;
}

/************************************************************************/
/*                   GDALAlgorithmRegistry::Instantiate()               */
/************************************************************************/

std::unique_ptr<GDALAlgorithm>
GDALAlgorithmRegistry::Instantiate(const std::string &name) const
{
    auto iter = m_mapNameToInfo.find(name);
    if (iter == m_mapNameToInfo.end())
    {
        iter = m_mapAliasToInfo.find(name);
        if (iter == m_mapAliasToInfo.end())
        {
            iter = m_mapHiddenAliasToInfo.find(name);
            if (iter == m_mapHiddenAliasToInfo.end())
            {
                return nullptr;
            }
        }
    }
    auto alg = iter->second.m_creationFunc();
    alg->m_aliases = iter->second.m_aliases;
    return alg;
}

/************************************************************************/
/*                GDALAlgorithmRegistry::GetNames()                     */
/************************************************************************/

std::vector<std::string> GDALAlgorithmRegistry::GetNames() const
{
    std::vector<std::string> res;
    for (const auto &iter : m_mapNameToInfo)
    {
        res.push_back(iter.first);
    }
    return res;
}

GDALGlobalAlgorithmRegistry::GDALGlobalAlgorithmRegistry() = default;

GDALGlobalAlgorithmRegistry::~GDALGlobalAlgorithmRegistry() = default;

/************************************************************************/
/*              GDALGlobalAlgorithmRegistry::GetSingleton()             */
/************************************************************************/

/* static */ GDALGlobalAlgorithmRegistry &
GDALGlobalAlgorithmRegistry::GetSingleton()
{
    static GDALGlobalAlgorithmRegistry singleton;
    return singleton;
}

/************************************************************************/
/*               GDALGlobalAlgorithmRegistry::Instantiate()             */
/************************************************************************/

std::unique_ptr<GDALAlgorithm>
GDALGlobalAlgorithmRegistry::Instantiate(const std::string &name) const
{
    if (name == GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME)
        return std::make_unique<GDALMainAlgorithm>();
    auto alg = GDALAlgorithmRegistry::Instantiate(name);
    if (!alg)
    {
        alg = InstantiateDeclaredSubAlgorithm(
            {GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME, name});
    }
    if (alg)
    {
        alg->SetCallPath({GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME, name});
    }
    return alg;
}

/************************************************************************/
/*             GDALGlobalAlgorithmRegistry::DeclareAlgorithm()          */
/************************************************************************/

void GDALGlobalAlgorithmRegistry::DeclareAlgorithm(
    const std::vector<std::string> &path, InstantiateFunc instantiateFunc)
{
    Node *curNode = &m_root;
    for (size_t i = 0; i < path.size(); ++i)
    {
        const std::string &name = path[i];
        auto iter = curNode->children.find(name);
        if (iter == curNode->children.end())
        {
            Node newNode;
            if (i + 1 == path.size())
            {
                newNode.instantiateFunc = instantiateFunc;
            }
            else
            {
                newNode.instantiateFunc =
                    [name]() -> std::unique_ptr<GDALAlgorithm>
                {
                    return std::make_unique<GDALContainerAlgorithm>(
                        name, std::string("Command for ").append(name));
                };
            }
            curNode =
                &(curNode->children.insert(std::pair(name, std::move(newNode)))
                      .first->second);
        }
        else
        {
            curNode = &(iter->second);
        }
    }
}

/************************************************************************/
/*            GDALGlobalAlgorithmRegistry::GetNodeFromPath()            */
/************************************************************************/

const GDALGlobalAlgorithmRegistry::Node *
GDALGlobalAlgorithmRegistry::GetNodeFromPath(
    const std::vector<std::string> &path) const
{
    if (!path.empty())
    {
        const Node *curNode = &m_root;
        bool first = true;
        for (const std::string &name : path)
        {
            if (first && name == GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME)
            {
                first = false;
                continue;
            }
            first = false;
            auto iter = curNode->children.find(name);
            if (iter == curNode->children.end())
                return nullptr;
            curNode = &(iter->second);
        }
        return curNode;
    }
    return nullptr;
}

/************************************************************************/
/*     GDALGlobalAlgorithmRegistry::GetDeclaredSubAlgorithmNames()      */
/************************************************************************/

std::vector<std::string>
GDALGlobalAlgorithmRegistry::GetDeclaredSubAlgorithmNames(
    const std::vector<std::string> &path) const
{
    const GDALGlobalAlgorithmRegistry::Node *node = GetNodeFromPath(path);
    std::vector<std::string> ret;
    if (node)
    {
        for (const auto &[name, subnode] : node->children)
        {
            // If there is an instantiation function, run it, to avoid
            // reporting algorithms that might be in drivers built as
            // deferred loaded plugins, but not available at runtime.
            if (!subnode.instantiateFunc || subnode.instantiateFunc())
            {
                ret.push_back(name);
            }
        }
    }
    return ret;
}

/************************************************************************/
/*       GDALGlobalAlgorithmRegistry::HasDeclaredSubAlgorithm()         */
/************************************************************************/

bool GDALGlobalAlgorithmRegistry::HasDeclaredSubAlgorithm(
    const std::vector<std::string> &path) const
{
    return GetNodeFromPath(path) != nullptr;
}

/************************************************************************/
/*     GDALGlobalAlgorithmRegistry::InstantiateDeclaredSubAlgorithm()   */
/************************************************************************/

std::unique_ptr<GDALAlgorithm>
GDALGlobalAlgorithmRegistry::InstantiateDeclaredSubAlgorithm(
    const std::vector<std::string> &path) const
{
    const GDALGlobalAlgorithmRegistry::Node *node = GetNodeFromPath(path);
    if (node && node->instantiateFunc)
    {
        auto alg = node->instantiateFunc();
        if (alg)
        {
            auto callPath = path;
            if (path[0] != GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME)
                callPath.insert(callPath.begin(),
                                GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);
            alg->SetCallPath(callPath);
        }
        return alg;
    }
    return nullptr;
}

/************************************************************************/
/*                    struct GDALAlgorithmRegistryHS                    */
/************************************************************************/

struct GDALAlgorithmRegistryHS
{
    GDALAlgorithmRegistry *ptr = nullptr;
};

/************************************************************************/
/*                   GDALGetGlobalAlgorithmRegistry()                   */
/************************************************************************/

/** Gets a handle to the GDALGetGlobalAlgorithmRegistry which references
 * all available top-level GDAL algorithms ("raster", "vector", etc.)
 *
 * The handle must be released with GDALAlgorithmRegistryRelease() (but
 * this does not destroy the GDALAlgorithmRegistryRelease singleton).
 *
 * @since 3.11
 */
GDALAlgorithmRegistryH GDALGetGlobalAlgorithmRegistry()
{
    auto ret = std::make_unique<GDALAlgorithmRegistryHS>();
    ret->ptr = &(GDALGlobalAlgorithmRegistry::GetSingleton());
    return ret.release();
}

/************************************************************************/
/*                   GDALAlgorithmRegistryRelease()                     */
/************************************************************************/

/** Release a handle to an algorithm registry, but this does not destroy the
 * registry itself.
 *
 * @since 3.11
 */
void GDALAlgorithmRegistryRelease(GDALAlgorithmRegistryH hReg)
{
    delete hReg;
}

/************************************************************************/
/*                   GDALAlgorithmRegistryGetAlgNames()                 */
/************************************************************************/

/** Return the names of the algorithms registered in the registry passed as
 * parameter.
 *
 * @param hReg Handle to a registry. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()
 *
 * @since 3.11
 */
char **GDALAlgorithmRegistryGetAlgNames(GDALAlgorithmRegistryH hReg)
{
    VALIDATE_POINTER1(hReg, __func__, nullptr);
    return CPLStringList(hReg->ptr->GetNames()).StealList();
}

/************************************************************************/
/*                  GDALAlgorithmRegistryInstantiateAlg()               */
/************************************************************************/

/** Instantiate an algorithm available in a registry from its name.
 *
 * @param hReg Handle to a registry. Must NOT be null.
 * @param pszAlgName Algorithm name. Must NOT be null.
 * @return an handle to the algorithm (to be freed with GDALAlgorithmRelease),
 * or NULL if the algorithm does not exist or another error occurred.
 *
 * @since 3.11
 */
GDALAlgorithmH GDALAlgorithmRegistryInstantiateAlg(GDALAlgorithmRegistryH hReg,
                                                   const char *pszAlgName)
{
    VALIDATE_POINTER1(hReg, __func__, nullptr);
    VALIDATE_POINTER1(pszAlgName, __func__, nullptr);
    auto alg = hReg->ptr->Instantiate(pszAlgName);
    return alg ? std::make_unique<GDALAlgorithmHS>(std::move(alg)).release()
               : nullptr;
}
