/******************************************************************************
 * $Id$
 *
 * Name:     vrtmultidim.cpp
 * Purpose:  Implementation of VRTDriver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/*! @cond Doxygen_Suppress */

#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <utility>

#include "cpl_mem_cache.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "vrtdataset.h"

static std::shared_ptr<GDALMDArray> ParseArray(const CPLXMLNode *psTree,
                                               const char *pszVRTPath,
                                               const char *pszParentXMLNode);

struct VRTArrayDatasetWrapper
{
    VRTArrayDatasetWrapper(const VRTArrayDatasetWrapper &) = delete;
    VRTArrayDatasetWrapper &operator=(const VRTArrayDatasetWrapper &) = delete;

    GDALDataset *m_poDS;

    explicit VRTArrayDatasetWrapper(GDALDataset *poDS) : m_poDS(poDS)
    {
        CPLDebug("VRT", "Open %s", poDS->GetDescription());
    }

    ~VRTArrayDatasetWrapper()
    {
        CPLDebug("VRT", "Close %s", m_poDS->GetDescription());
        delete m_poDS;
    }

    GDALDataset *get() const
    {
        return m_poDS;
    }
};

typedef std::pair<std::shared_ptr<VRTArrayDatasetWrapper>,
                  std::unordered_set<const void *>>
    CacheEntry;
static std::mutex g_cacheLock;
static lru11::Cache<std::string, CacheEntry> g_cacheSources(100);

/************************************************************************/
/*                            GetRootGroup()                            */
/************************************************************************/

std::shared_ptr<GDALGroup> VRTDataset::GetRootGroup() const
{
    return m_poRootGroup;
}

/************************************************************************/
/*                              VRTGroup()                              */
/************************************************************************/

VRTGroup::VRTGroup(const char *pszVRTPath)
    : GDALGroup(std::string(), std::string()),
      m_poRefSelf(std::make_shared<Ref>(this)), m_osVRTPath(pszVRTPath)
{
}

/************************************************************************/
/*                              VRTGroup()                              */
/************************************************************************/

VRTGroup::VRTGroup(const std::string &osParentName, const std::string &osName)
    : GDALGroup(osParentName, osName), m_poRefSelf(std::make_shared<Ref>(this))
{
}

/************************************************************************/
/*                             ~VRTGroup()                              */
/************************************************************************/

VRTGroup::~VRTGroup()
{
    if (m_poSharedRefRootGroup)
    {
        VRTGroup::Serialize();
    }
}

/************************************************************************/
/*                         SetIsRootGroup()                             */
/************************************************************************/

void VRTGroup::SetIsRootGroup()
{
    m_poSharedRefRootGroup = std::make_shared<Ref>(this);
}

/************************************************************************/
/*                         SetRootGroupRef()                            */
/************************************************************************/

void VRTGroup::SetRootGroupRef(const std::weak_ptr<Ref> &rgRef)
{
    m_poWeakRefRootGroup = rgRef;
}

/************************************************************************/
/*                          GetRootGroupRef()                           */
/************************************************************************/

std::weak_ptr<VRTGroup::Ref> VRTGroup::GetRootGroupRef() const
{
    return m_poSharedRefRootGroup ? m_poSharedRefRootGroup
                                  : m_poWeakRefRootGroup;
}

/************************************************************************/
/*                           GetRootGroup()                             */
/************************************************************************/

VRTGroup *VRTGroup::GetRootGroup() const
{
    if (m_poSharedRefRootGroup)
        return m_poSharedRefRootGroup->m_ptr;
    auto ref(m_poWeakRefRootGroup.lock());
    return ref ? ref->m_ptr : nullptr;
}

/************************************************************************/
/*                       GetRootGroupSharedPtr()                        */
/************************************************************************/

std::shared_ptr<GDALGroup> VRTGroup::GetRootGroupSharedPtr() const
{
    auto group = GetRootGroup();
    if (group)
        return group->m_pSelf.lock();
    return nullptr;
}

/************************************************************************/
/*                               XMLInit()                              */
/************************************************************************/

bool VRTGroup::XMLInit(const std::shared_ptr<VRTGroup> &poRoot,
                       const std::shared_ptr<VRTGroup> &poThisGroup,
                       const CPLXMLNode *psNode, const char *pszVRTPath)
{
    if (pszVRTPath != nullptr)
        m_osVRTPath = pszVRTPath;

    for (const auto *psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Group") == 0)
        {
            const char *pszSubGroupName =
                CPLGetXMLValue(psIter, "name", nullptr);
            if (pszSubGroupName == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing name attribute on Group");
                m_bDirty = false;
                return false;
            }
            auto poSubGroup(std::dynamic_pointer_cast<VRTGroup>(
                CreateGroup(pszSubGroupName)));
            if (poSubGroup == nullptr ||
                !poSubGroup->XMLInit(poRoot, poSubGroup, psIter,
                                     m_osVRTPath.c_str()))
            {
                m_bDirty = false;
                return false;
            }
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Dimension") == 0)
        {
            auto poDim = VRTDimension::Create(
                poThisGroup, poThisGroup->GetFullName(), psIter);
            if (!poDim)
            {
                m_bDirty = false;
                return false;
            }
            m_oMapDimensions[poDim->GetName()] = poDim;
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Attribute") == 0)
        {
            auto poAttr =
                VRTAttribute::Create(poThisGroup->GetFullName(), psIter);
            if (!poAttr)
            {
                m_bDirty = false;
                return false;
            }
            m_oMapAttributes[poAttr->GetName()] = poAttr;
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Array") == 0)
        {
            auto poArray = VRTMDArray::Create(
                poThisGroup, poThisGroup->GetFullName(), psIter);
            if (!poArray)
            {
                m_bDirty = false;
                return false;
            }
            m_oMapMDArrays[poArray->GetName()] = poArray;
        }
    }

    m_bDirty = false;
    return true;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

bool VRTGroup::Serialize() const
{
    if (!m_bDirty || m_osFilename.empty())
        return true;
    m_bDirty = false;

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpVRT = VSIFOpenL(m_osFilename.c_str(), "w");
    if (fpVRT == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to write .vrt file in Serialize().");
        return false;
    }

    CPLXMLNode *psDSTree = SerializeToXML(m_osVRTPath.c_str());
    char *pszXML = CPLSerializeXMLTree(psDSTree);

    CPLDestroyXMLNode(psDSTree);

    bool bOK = true;
    if (pszXML)
    {
        /* ------------------------------------------------------------------ */
        /*      Write to disk.                                                */
        /* ------------------------------------------------------------------ */
        bOK &= VSIFWriteL(pszXML, 1, strlen(pszXML), fpVRT) == strlen(pszXML);
        CPLFree(pszXML);
    }
    if (VSIFCloseL(fpVRT) != 0)
        bOK = false;
    if (!bOK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to write .vrt file in Serialize().");
    }
    return bOK;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTGroup::SerializeToXML(const char *pszVRTPath) const
{
    CPLXMLNode *psDSTree = CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset");
    Serialize(psDSTree, pszVRTPath);
    return psDSTree;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTGroup::Serialize(CPLXMLNode *psParent, const char *pszVRTPath) const
{
    CPLXMLNode *psGroup = CPLCreateXMLNode(psParent, CXT_Element, "Group");
    CPLAddXMLAttributeAndValue(psGroup, "name", GetName().c_str());
    for (const auto &iter : m_oMapDimensions)
    {
        iter.second->Serialize(psGroup);
    }
    for (const auto &iter : m_oMapAttributes)
    {
        iter.second->Serialize(psGroup);
    }
    for (const auto &iter : m_oMapMDArrays)
    {
        iter.second->Serialize(psGroup, pszVRTPath);
    }
    for (const auto &iter : m_oMapGroups)
    {
        iter.second->Serialize(psGroup, pszVRTPath);
    }
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> VRTGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> names;
    for (const auto &iter : m_oMapGroups)
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                         OpenGroupInternal()                          */
/************************************************************************/

std::shared_ptr<VRTGroup>
VRTGroup::OpenGroupInternal(const std::string &osName) const
{
    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
VRTGroup::GetDimensions(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALDimension>> oRes;
    for (const auto &oIter : m_oMapDimensions)
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                    GetDimensionFromFullName()                   */
/************************************************************************/

std::shared_ptr<VRTDimension>
VRTGroup::GetDimensionFromFullName(const std::string &name,
                                   bool bEmitError) const
{
    if (name[0] != '/')
    {
        auto poDim(GetDimension(name));
        if (!poDim)
        {
            if (bEmitError)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find dimension %s in this group",
                         name.c_str());
            }
            return nullptr;
        }
        return poDim;
    }
    else
    {
        auto curGroup(GetRootGroup());
        if (curGroup == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot access root group");
            return nullptr;
        }
        CPLStringList aosTokens(CSLTokenizeString2(name.c_str(), "/", 0));
        for (int i = 0; i < aosTokens.size() - 1; i++)
        {
            curGroup = curGroup->OpenGroupInternal(aosTokens[i]).get();
            if (!curGroup)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find group %s",
                         aosTokens[i]);
                return nullptr;
            }
        }
        auto poDim(curGroup->GetDimension(aosTokens.back()));
        if (!poDim)
        {
            if (bEmitError)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find dimension %s", name.c_str());
            }
            return nullptr;
        }
        return poDim;
    }
}

/************************************************************************/
/*                            GetAttributes()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
VRTGroup::GetAttributes(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALAttribute>> oRes;
    for (const auto &oIter : m_oMapAttributes)
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> VRTGroup::GetMDArrayNames(CSLConstList) const
{
    std::vector<std::string> names;
    for (const auto &iter : m_oMapMDArrays)
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                             OpenMDArray()                            */
/************************************************************************/

std::shared_ptr<GDALMDArray> VRTGroup::OpenMDArray(const std::string &osName,
                                                   CSLConstList) const
{
    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                             SetDirty()                               */
/************************************************************************/

void VRTGroup::SetDirty()
{
    auto poRootGroup(GetRootGroup());
    if (poRootGroup)
        poRootGroup->m_bDirty = true;
}

/************************************************************************/
/*                             CreateGroup()                            */
/************************************************************************/

std::shared_ptr<GDALGroup> VRTGroup::CreateGroup(const std::string &osName,
                                                 CSLConstList /*papszOptions*/)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty group name not supported");
        return nullptr;
    }
    if (m_oMapGroups.find(osName) != m_oMapGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name (%s) already exists", osName.c_str());
        return nullptr;
    }
    SetDirty();
    auto newGroup(VRTGroup::Create(GetFullName(), osName.c_str()));
    newGroup->SetRootGroupRef(GetRootGroupRef());
    m_oMapGroups[osName] = newGroup;
    return newGroup;
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension>
VRTGroup::CreateDimension(const std::string &osName, const std::string &osType,
                          const std::string &osDirection, GUInt64 nSize,
                          CSLConstList)
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
                 "A dimension with same name (%s) already exists",
                 osName.c_str());
        return nullptr;
    }
    SetDirty();
    auto newDim(std::make_shared<VRTDimension>(GetRef(), GetFullName(), osName,
                                               osType, osDirection, nSize,
                                               std::string()));
    m_oMapDimensions[osName] = newDim;
    return newDim;
}

/************************************************************************/
/*                           CreateAttribute()                          */
/************************************************************************/

std::shared_ptr<GDALAttribute>
VRTGroup::CreateAttribute(const std::string &osName,
                          const std::vector<GUInt64> &anDimensions,
                          const GDALExtendedDataType &oDataType, CSLConstList)
{
    if (!VRTAttribute::CreationCommonChecks(osName, anDimensions,
                                            m_oMapAttributes))
    {
        return nullptr;
    }
    SetDirty();
    auto newAttr(std::make_shared<VRTAttribute>(
        (GetFullName() == "/" ? "/" : GetFullName() + "/") + "_GLOBAL_", osName,
        anDimensions.empty() ? 0 : anDimensions[0], oDataType));
    m_oMapAttributes[osName] = newAttr;
    return newAttr;
}

/************************************************************************/
/*                            CreateMDArray()                           */
/************************************************************************/

std::shared_ptr<GDALMDArray> VRTGroup::CreateMDArray(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oType, CSLConstList)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty array name not supported");
        return nullptr;
    }
    if (m_oMapMDArrays.find(osName) != m_oMapMDArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name (%s) already exists", osName.c_str());
        return nullptr;
    }
    for (auto &poDim : aoDimensions)
    {
        auto poFoundDim(
            dynamic_cast<const VRTDimension *>(poDim.get())
                ? GetDimensionFromFullName(poDim->GetFullName(), false)
                : nullptr);
        if (poFoundDim == nullptr || poFoundDim->GetSize() != poDim->GetSize())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "One input dimension is not a VRTDimension "
                     "or a VRTDimension of this dataset");
            return nullptr;
        }
    }
    auto newArray(std::make_shared<VRTMDArray>(GetRef(), GetFullName(), osName,
                                               aoDimensions, oType));
    newArray->SetSelf(newArray);
    m_oMapMDArrays[osName] = newArray;
    return newArray;
}

/************************************************************************/
/*                          ParseDataType()                             */
/************************************************************************/

static GDALExtendedDataType ParseDataType(const CPLXMLNode *psNode)
{
    const auto *psType = CPLGetXMLNode(psNode, "DataType");
    if (psType == nullptr || psType->psChild == nullptr ||
        psType->psChild->eType != CXT_Text)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled content for DataType or Missing");
        return GDALExtendedDataType::Create(GDT_Unknown);
    }
    GDALExtendedDataType dt(GDALExtendedDataType::CreateString());
    if (EQUAL(psType->psChild->pszValue, "String"))
    {
        // done
    }
    else
    {
        const auto eDT = GDALGetDataTypeByName(psType->psChild->pszValue);
        dt = GDALExtendedDataType::Create(eDT);
    }
    return dt;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<VRTDimension>
VRTDimension::Create(const std::shared_ptr<VRTGroup> &poThisGroup,
                     const std::string &osParentName, const CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", nullptr);
    if (pszName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing name attribute on Dimension");
        return nullptr;
    }
    const char *pszType = CPLGetXMLValue(psNode, "type", "");
    const char *pszDirection = CPLGetXMLValue(psNode, "direction", "");
    const char *pszSize = CPLGetXMLValue(psNode, "size", "");
    GUInt64 nSize = static_cast<GUInt64>(
        CPLScanUIntBig(pszSize, static_cast<int>(strlen(pszSize))));
    if (nSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for size attribute on Dimension");
        return nullptr;
    }
    const char *pszIndexingVariable =
        CPLGetXMLValue(psNode, "indexingVariable", "");
    return std::make_shared<VRTDimension>(poThisGroup->GetRef(), osParentName,
                                          pszName, pszType, pszDirection, nSize,
                                          pszIndexingVariable);
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTDimension::Serialize(CPLXMLNode *psParent) const
{
    CPLXMLNode *psDimension =
        CPLCreateXMLNode(psParent, CXT_Element, "Dimension");
    CPLAddXMLAttributeAndValue(psDimension, "name", GetName().c_str());
    if (!m_osType.empty())
    {
        CPLAddXMLAttributeAndValue(psDimension, "type", m_osType.c_str());
    }
    if (!m_osDirection.empty())
    {
        CPLAddXMLAttributeAndValue(psDimension, "direction",
                                   m_osDirection.c_str());
    }
    CPLAddXMLAttributeAndValue(
        psDimension, "size",
        CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(m_nSize)));
    if (!m_osIndexingVariableName.empty())
    {
        CPLAddXMLAttributeAndValue(psDimension, "indexingVariable",
                                   m_osIndexingVariableName.c_str());
    }
}

/************************************************************************/
/*                                GetGroup()                            */
/************************************************************************/

VRTGroup *VRTDimension::GetGroup() const
{
    auto ref = m_poGroupRef.lock();
    return ref ? ref->m_ptr : nullptr;
}

/************************************************************************/
/*                         GetIndexingVariable()                        */
/************************************************************************/

std::shared_ptr<GDALMDArray> VRTDimension::GetIndexingVariable() const
{
    if (m_osIndexingVariableName.empty())
        return nullptr;
    auto poGroup = GetGroup();
    if (poGroup == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot access group");
        return nullptr;
    }
    std::shared_ptr<GDALMDArray> poVar;
    if (m_osIndexingVariableName[0] != '/')
    {
        poVar = poGroup->OpenMDArray(m_osIndexingVariableName);
    }
    else
    {
        poGroup = poGroup->GetRootGroup();
        if (poGroup == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot access root group");
            return nullptr;
        }
        poVar = poGroup->OpenMDArrayFromFullname(m_osIndexingVariableName);
    }
    if (!poVar)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find variable %s",
                 m_osIndexingVariableName.c_str());
    }
    return poVar;
}

/************************************************************************/
/*                         SetIndexingVariable()                        */
/************************************************************************/

bool VRTDimension::SetIndexingVariable(
    std::shared_ptr<GDALMDArray> poIndexingVariable)
{
    if (poIndexingVariable == nullptr)
    {
        m_osIndexingVariableName.clear();
        return true;
    }

    auto poGroup = GetGroup();
    if (poGroup == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot access group");
        return false;
    }
    poGroup = poGroup->GetRootGroup();
    if (poGroup == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot access root group");
        return false;
    }
    auto poVar(std::dynamic_pointer_cast<VRTMDArray>(
        poGroup->OpenMDArrayFromFullname(poIndexingVariable->GetFullName())));
    if (!poVar)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find variable %s",
                 poIndexingVariable->GetFullName().c_str());
        return false;
    }
    if (poVar->GetGroup() == GetGroup())
    {
        m_osIndexingVariableName = poIndexingVariable->GetName();
    }
    else
    {
        m_osIndexingVariableName = poIndexingVariable->GetFullName();
    }
    return true;
}

/************************************************************************/
/*                       CreationCommonChecks()                         */
/************************************************************************/

bool VRTAttribute::CreationCommonChecks(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const std::map<std::string, std::shared_ptr<VRTAttribute>> &oMapAttributes)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty attribute name not supported");
        return false;
    }
    if (oMapAttributes.find(osName) != oMapAttributes.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An attribute with same name (%s) already exists",
                 osName.c_str());
        return false;
    }
    if (anDimensions.size() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only single dimensional attribute handled");
        return false;
    }
    if (anDimensions.size() == 1 &&
        anDimensions[0] > static_cast<GUInt64>(INT_MAX))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too large attribute");
        return false;
    }
    return true;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<VRTAttribute>
VRTAttribute::Create(const std::string &osParentName, const CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", nullptr);
    if (pszName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing name attribute on Attribute");
        return nullptr;
    }
    GDALExtendedDataType dt(ParseDataType(psNode));
    if (dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown)
    {
        return nullptr;
    }
    std::vector<std::string> aosValues;
    for (const auto *psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Value") == 0)
        {
            aosValues.push_back(CPLGetXMLValue(psIter, nullptr, ""));
        }
    }
    return std::make_shared<VRTAttribute>(osParentName, pszName, dt,
                                          std::move(aosValues));
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool VRTAttribute::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                         const GInt64 *arrayStep,
                         const GPtrDiff_t *bufferStride,
                         const GDALExtendedDataType &bufferDataType,
                         void *pDstBuffer) const
{
    const auto stringDT(GDALExtendedDataType::CreateString());
    if (m_aosList.empty())
    {
        const char *pszStr = nullptr;
        GDALExtendedDataType::CopyValue(&pszStr, stringDT, pDstBuffer,
                                        bufferDataType);
    }
    else
    {
        GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
        for (size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++)
        {
            const int idx =
                m_dims.empty()
                    ? 0
                    : static_cast<int>(arrayStartIdx[0] + i * arrayStep[0]);
            const char *pszStr = m_aosList[idx].data();
            GDALExtendedDataType::CopyValue(&pszStr, stringDT, pabyDstBuffer,
                                            bufferDataType);
            if (!m_dims.empty())
            {
                pabyDstBuffer += bufferStride[0] * bufferDataType.GetSize();
            }
        }
    }
    return true;
}

/************************************************************************/
/*                                  IWrite()                            */
/************************************************************************/

bool VRTAttribute::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                          const GInt64 *arrayStep,
                          const GPtrDiff_t *bufferStride,
                          const GDALExtendedDataType &bufferDataType,
                          const void *pSrcBuffer)
{
    m_aosList.resize(m_dims.empty() ? 1
                                    : static_cast<int>(m_dims[0]->GetSize()));
    const GByte *pabySrcBuffer = static_cast<const GByte *>(pSrcBuffer);
    const auto stringDT(GDALExtendedDataType::CreateString());
    for (size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++)
    {
        const int idx =
            m_dims.empty()
                ? 0
                : static_cast<int>(arrayStartIdx[0] + i * arrayStep[0]);
        char *pszStr = nullptr;
        GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType, &pszStr,
                                        stringDT);
        m_aosList[idx] = pszStr ? pszStr : "";
        CPLFree(pszStr);
        if (!m_dims.empty())
        {
            pabySrcBuffer += bufferStride[0] * bufferDataType.GetSize();
        }
    }
    return true;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTAttribute::Serialize(CPLXMLNode *psParent) const
{
    CPLXMLNode *psAttr = CPLCreateXMLNode(psParent, CXT_Element, "Attribute");
    CPLAddXMLAttributeAndValue(psAttr, "name", GetName().c_str());
    CPLXMLNode *psDataType = CPLCreateXMLNode(psAttr, CXT_Element, "DataType");
    if (m_dt.GetClass() == GEDTC_STRING)
        CPLCreateXMLNode(psDataType, CXT_Text, "String");
    else
        CPLCreateXMLNode(psDataType, CXT_Text,
                         GDALGetDataTypeName(m_dt.GetNumericDataType()));
    CPLXMLNode *psLast = psDataType;
    for (const auto &str : m_aosList)
    {
        CPLXMLNode *psValue = CPLCreateXMLNode(nullptr, CXT_Element, "Value");
        CPLCreateXMLNode(psValue, CXT_Text, str.c_str());
        psLast->psNext = psValue;
        psLast = psValue;
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<VRTMDArray>
VRTMDArray::Create(const std::shared_ptr<VRTGroup> &poThisGroup,
                   const std::string &osParentName, const CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", nullptr);
    if (pszName == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing name attribute on Array");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check for an SRS node.                                          */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *psSRSNode = CPLGetXMLNode(psNode, "SRS");
    std::unique_ptr<OGRSpatialReference> poSRS;
    if (psSRSNode)
    {
        poSRS = std::make_unique<OGRSpatialReference>();
        poSRS->SetFromUserInput(
            CPLGetXMLValue(psSRSNode, nullptr, ""),
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get());
        const char *pszMapping =
            CPLGetXMLValue(psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
        if (pszMapping)
        {
            char **papszTokens =
                CSLTokenizeStringComplex(pszMapping, ",", FALSE, FALSE);
            std::vector<int> anMapping;
            for (int i = 0; papszTokens && papszTokens[i]; i++)
            {
                anMapping.push_back(atoi(papszTokens[i]));
            }
            CSLDestroy(papszTokens);
            poSRS->SetDataAxisToSRSAxisMapping(anMapping);
        }
    }

    GDALExtendedDataType dt(ParseDataType(psNode));
    if (dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown)
    {
        return nullptr;
    }
    std::vector<std::shared_ptr<GDALDimension>> dims;
    std::map<std::string, std::shared_ptr<VRTAttribute>> oMapAttributes;
    for (const auto *psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Dimension") == 0)
        {
            auto poDim =
                VRTDimension::Create(poThisGroup, std::string(), psIter);
            if (!poDim)
                return nullptr;
            dims.emplace_back(poDim);
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "DimensionRef") == 0)
        {
            const char *pszRef = CPLGetXMLValue(psIter, "ref", nullptr);
            if (pszRef == nullptr || pszRef[0] == '\0')
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing ref attribute on DimensionRef");
                return nullptr;
            }
            auto poDim(poThisGroup->GetDimensionFromFullName(pszRef, true));
            if (!poDim)
                return nullptr;
            dims.emplace_back(poDim);
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Attribute") == 0)
        {
            auto poAttr =
                VRTAttribute::Create(osParentName + "/" + pszName, psIter);
            if (!poAttr)
                return nullptr;
            oMapAttributes[poAttr->GetName()] = poAttr;
        }
    }

    auto array(std::make_shared<VRTMDArray>(poThisGroup->GetRef(), osParentName,
                                            pszName, dt, std::move(dims),
                                            std::move(oMapAttributes)));
    array->SetSelf(array);
    array->SetSpatialRef(poSRS.get());

    const char *pszNoDataValue = CPLGetXMLValue(psNode, "NoDataValue", nullptr);
    if (pszNoDataValue)
        array->SetNoDataValue(CPLAtof(pszNoDataValue));

    const char *pszUnit = CPLGetXMLValue(psNode, "Unit", nullptr);
    if (pszUnit)
        array->SetUnit(pszUnit);

    const char *pszOffset = CPLGetXMLValue(psNode, "Offset", nullptr);
    if (pszOffset)
        array->SetOffset(CPLAtof(pszOffset));

    const char *pszScale = CPLGetXMLValue(psNode, "Scale", nullptr);
    if (pszScale)
        array->SetScale(CPLAtof(pszScale));

    for (const auto *psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "RegularlySpacedValues") == 0)
        {
            if (dt.GetClass() != GEDTC_NUMERIC)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "RegularlySpacedValues only supported for numeric "
                         "data types");
                return nullptr;
            }
            if (array->GetDimensionCount() != 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "RegularlySpacedValues only supported with single "
                         "dimension array");
                return nullptr;
            }
            const char *pszStart = CPLGetXMLValue(psIter, "start", nullptr);
            if (pszStart == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "start attribute missing");
                return nullptr;
            }
            const char *pszIncrement =
                CPLGetXMLValue(psIter, "increment", nullptr);
            if (pszIncrement == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "increment attribute missing");
                return nullptr;
            }
            std::unique_ptr<VRTMDArraySourceRegularlySpaced> poSource(
                new VRTMDArraySourceRegularlySpaced(CPLAtof(pszStart),
                                                    CPLAtof(pszIncrement)));
            array->AddSource(std::move(poSource));
        }
        else if (psIter->eType == CXT_Element &&
                 (strcmp(psIter->pszValue, "InlineValues") == 0 ||
                  strcmp(psIter->pszValue, "InlineValuesWithValueElement") ==
                      0 ||
                  strcmp(psIter->pszValue, "ConstantValue") == 0))
        {
            auto poSource(
                VRTMDArraySourceInlinedValues::Create(array.get(), psIter));
            if (!poSource)
                return nullptr;
            array->AddSource(std::move(poSource));
        }
        else if (psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Source") == 0)
        {
            auto poSource(
                VRTMDArraySourceFromArray::Create(array.get(), psIter));
            if (!poSource)
                return nullptr;
            array->AddSource(std::move(poSource));
        }
    }

    return array;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<VRTMDArray> VRTMDArray::Create(const char *pszVRTPath,
                                               const CPLXMLNode *psNode)
{
    auto poDummyGroup =
        std::shared_ptr<VRTGroup>(new VRTGroup(pszVRTPath ? pszVRTPath : ""));
    auto poArray = Create(poDummyGroup, std::string(), psNode);
    if (poArray)
        poArray->m_poDummyOwningGroup = std::move(poDummyGroup);
    return poArray;
}

/************************************************************************/
/*                            GetAttributes()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
VRTMDArray::GetAttributes(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALAttribute>> oRes;
    for (const auto &oIter : m_oMapAttributes)
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                                  Read()                              */
/************************************************************************/

bool VRTMDArraySourceRegularlySpaced::Read(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    void *pDstBuffer) const
{
    GDALExtendedDataType dtFloat64(GDALExtendedDataType::Create(GDT_Float64));
    GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
    for (size_t i = 0; i < count[0]; i++)
    {
        const double dfVal =
            m_dfStart + (arrayStartIdx[0] + i * arrayStep[0]) * m_dfIncrement;
        GDALExtendedDataType::CopyValue(&dfVal, dtFloat64, pabyDstBuffer,
                                        bufferDataType);
        pabyDstBuffer += bufferStride[0] * bufferDataType.GetSize();
    }
    return true;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTMDArraySourceRegularlySpaced::Serialize(CPLXMLNode *psParent,
                                                const char *) const
{
    CPLXMLNode *psSource =
        CPLCreateXMLNode(psParent, CXT_Element, "RegularlySpacedValues");
    CPLAddXMLAttributeAndValue(psSource, "start",
                               CPLSPrintf("%.17g", m_dfStart));
    CPLAddXMLAttributeAndValue(psSource, "increment",
                               CPLSPrintf("%.17g", m_dfIncrement));
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::unique_ptr<VRTMDArraySourceInlinedValues>
VRTMDArraySourceInlinedValues::Create(const VRTMDArray *array,
                                      const CPLXMLNode *psNode)
{
    const bool bIsConstantValue =
        strcmp(psNode->pszValue, "ConstantValue") == 0;
    const auto &dt(array->GetDataType());
    const size_t nDTSize = dt.GetSize();
    if (nDTSize == 0)
        return nullptr;
    if (strcmp(psNode->pszValue, "InlineValuesWithValueElement") == 0)
    {
        if (dt.GetClass() != GEDTC_NUMERIC && dt.GetClass() != GEDTC_STRING)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Only numeric or string data type handled for "
                     "InlineValuesWithValueElement");
            return nullptr;
        }
    }
    else if (dt.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only numeric data type handled for InlineValues");
        return nullptr;
    }

    const int nDimCount = static_cast<int>(array->GetDimensionCount());
    std::vector<GUInt64> anOffset(nDimCount);
    std::vector<size_t> anCount(nDimCount);
    size_t nArrayByteSize = nDTSize;
    if (nDimCount > 0)
    {
        const auto &dims(array->GetDimensions());

        const char *pszOffset = CPLGetXMLValue(psNode, "offset", nullptr);
        if (pszOffset != nullptr)
        {
            CPLStringList aosTokensOffset(
                CSLTokenizeString2(pszOffset, ", ", 0));
            if (aosTokensOffset.size() != nDimCount)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong number of values in offset");
                return nullptr;
            }
            for (int i = 0; i < nDimCount; ++i)
            {
                anOffset[i] = static_cast<GUInt64>(CPLScanUIntBig(
                    aosTokensOffset[i],
                    static_cast<int>(strlen(aosTokensOffset[i]))));
                if (aosTokensOffset[i][0] == '-' ||
                    anOffset[i] >= dims[i]->GetSize())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value in offset");
                    return nullptr;
                }
            }
        }

        const char *pszCount = CPLGetXMLValue(psNode, "count", nullptr);
        if (pszCount != nullptr)
        {
            CPLStringList aosTokensCount(CSLTokenizeString2(pszCount, ", ", 0));
            if (aosTokensCount.size() != nDimCount)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong number of values in count");
                return nullptr;
            }
            for (int i = 0; i < nDimCount; ++i)
            {
                anCount[i] = static_cast<size_t>(CPLScanUIntBig(
                    aosTokensCount[i],
                    static_cast<int>(strlen(aosTokensCount[i]))));
                if (aosTokensCount[i][0] == '-' || anCount[i] == 0 ||
                    anOffset[i] + anCount[i] > dims[i]->GetSize())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value in count");
                    return nullptr;
                }
            }
        }
        else
        {
            for (int i = 0; i < nDimCount; ++i)
            {
                anCount[i] =
                    static_cast<size_t>(dims[i]->GetSize() - anOffset[i]);
            }
        }
        if (!bIsConstantValue)
        {
            for (int i = 0; i < nDimCount; ++i)
            {
                if (anCount[i] >
                    std::numeric_limits<size_t>::max() / nArrayByteSize)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                    return nullptr;
                }
                nArrayByteSize *= anCount[i];
            }
        }
    }

    const size_t nExpectedVals = nArrayByteSize / nDTSize;
    CPLStringList aosValues;

    if (strcmp(psNode->pszValue, "InlineValuesWithValueElement") == 0)
    {
        for (auto psIter = psNode->psChild; psIter; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "Value") == 0)
            {
                aosValues.AddString(CPLGetXMLValue(psIter, nullptr, ""));
            }
        }
    }
    else
    {
        const char *pszValue = CPLGetXMLValue(psNode, nullptr, nullptr);
        if (pszValue == nullptr ||
            (!bIsConstantValue && nExpectedVals > strlen(pszValue)))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content");
            return nullptr;
        }
        aosValues.Assign(CSLTokenizeString2(pszValue, ", \r\n", 0), true);
    }

    if (static_cast<size_t>(aosValues.size()) != nExpectedVals)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of values. Got %u, expected %u",
                 static_cast<unsigned>(aosValues.size()),
                 static_cast<unsigned>(nExpectedVals));
        return nullptr;
    }
    std::vector<GByte> abyValues;
    try
    {
        abyValues.resize(nArrayByteSize);
    }
    catch (const std::exception &ex)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", ex.what());
        return nullptr;
    }

    const auto dtString(GDALExtendedDataType::CreateString());
    GByte *pabyPtr = &abyValues[0];
    for (int i = 0; i < aosValues.size(); ++i)
    {
        const char *pszVal = &aosValues[i][0];
        GDALExtendedDataType::CopyValue(&pszVal, dtString, pabyPtr, dt);
        pabyPtr += nDTSize;
    }

    return std::make_unique<VRTMDArraySourceInlinedValues>(
        array, bIsConstantValue, std::move(anOffset), std::move(anCount),
        std::move(abyValues));
}

/************************************************************************/
/*                  ~VRTMDArraySourceInlinedValues()                    */
/************************************************************************/

VRTMDArraySourceInlinedValues::~VRTMDArraySourceInlinedValues()
{
    if (m_dt.NeedsFreeDynamicMemory())
    {
        const size_t nDTSize = m_dt.GetSize();
        const size_t nValueCount = m_abyValues.size() / nDTSize;
        GByte *pabyPtr = &m_abyValues[0];
        for (size_t i = 0; i < nValueCount; ++i)
        {
            m_dt.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
    }
}

/************************************************************************/
/*                                   Read()                             */
/************************************************************************/
static inline void IncrPointer(const GByte *&ptr, GInt64 nInc, size_t nIncSize)
{
    if (nInc < 0)
        ptr -= (-nInc) * nIncSize;
    else
        ptr += nInc * nIncSize;
}

static inline void IncrPointer(GByte *&ptr, GPtrDiff_t nInc, size_t nIncSize)
{
    if (nInc < 0)
        ptr -= (-nInc) * nIncSize;
    else
        ptr += nInc * nIncSize;
}

bool VRTMDArraySourceInlinedValues::Read(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    void *pDstBuffer) const
{
    const auto nDims(m_poDstArray->GetDimensionCount());
    std::vector<GUInt64> anReqStart(nDims);
    std::vector<size_t> anReqCount(nDims);
    // Compute the intersection between the inline value slab and the
    // request slab.
    for (size_t i = 0; i < nDims; i++)
    {
        auto start_i = arrayStartIdx[i];
        auto step_i = arrayStep[i] == 0 ? 1 : arrayStep[i];
        if (arrayStep[i] < 0)
        {
            // For negative step request, temporarily simulate a positive step
            // and fix up the start at the end of the loop.
            // Use double negation so that operations occur only on
            // positive quantities to avoid an artificial negative signed
            // integer to unsigned conversion.
            start_i = start_i - ((count[i] - 1) * (-step_i));
            step_i = -step_i;
        }

        const auto nRightDstOffsetFromConfig = m_anOffset[i] + m_anCount[i];
        if (start_i >= nRightDstOffsetFromConfig ||
            start_i + (count[i] - 1) * step_i < m_anOffset[i])
        {
            return true;
        }
        if (start_i < m_anOffset[i])
        {
            anReqStart[i] =
                m_anOffset[i] +
                (step_i - ((m_anOffset[i] - start_i) % step_i)) % step_i;
        }
        else
        {
            anReqStart[i] = start_i;
        }
        anReqCount[i] = 1 + static_cast<size_t>(
                                (std::min(nRightDstOffsetFromConfig - 1,
                                          start_i + (count[i] - 1) * step_i) -
                                 anReqStart[i]) /
                                step_i);
        if (arrayStep[i] < 0)
        {
            anReqStart[i] = anReqStart[i] + (anReqCount[i] - 1) * step_i;
        }
    }

    size_t nSrcOffset = 0;
    GPtrDiff_t nDstOffset = 0;
    const auto nBufferDataTypeSize(bufferDataType.GetSize());
    for (size_t i = 0; i < nDims; i++)
    {
        const size_t nRelStartSrc =
            static_cast<size_t>(anReqStart[i] - m_anOffset[i]);
        nSrcOffset += nRelStartSrc * m_anInlinedArrayStrideInBytes[i];
        const size_t nRelStartDst =
            static_cast<size_t>(anReqStart[i] - arrayStartIdx[i]);
        nDstOffset += nRelStartDst * bufferStride[i] * nBufferDataTypeSize;
    }
    std::vector<const GByte *> abyStackSrcPtr(nDims + 1);
    abyStackSrcPtr[0] = m_abyValues.data() + nSrcOffset;
    std::vector<GByte *> abyStackDstPtr(nDims + 1);
    abyStackDstPtr[0] = static_cast<GByte *>(pDstBuffer) + nDstOffset;

    const auto &dt(m_poDstArray->GetDataType());
    std::vector<size_t> anStackCount(nDims);
    size_t iDim = 0;

lbl_next_depth:
    if (iDim == nDims)
    {
        GDALExtendedDataType::CopyValue(abyStackSrcPtr[nDims], dt,
                                        abyStackDstPtr[nDims], bufferDataType);
    }
    else
    {
        anStackCount[iDim] = anReqCount[iDim];
        while (true)
        {
            ++iDim;
            abyStackSrcPtr[iDim] = abyStackSrcPtr[iDim - 1];
            abyStackDstPtr[iDim] = abyStackDstPtr[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            IncrPointer(abyStackSrcPtr[iDim], arrayStep[iDim],
                        m_anInlinedArrayStrideInBytes[iDim]);
            IncrPointer(abyStackDstPtr[iDim], bufferStride[iDim],
                        nBufferDataTypeSize);
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTMDArraySourceInlinedValues::Serialize(CPLXMLNode *psParent,
                                              const char *) const
{
    const auto &dt(m_poDstArray->GetDataType());
    CPLXMLNode *psSource = CPLCreateXMLNode(psParent, CXT_Element,
                                            m_bIsConstantValue ? "ConstantValue"
                                            : dt.GetClass() == GEDTC_STRING
                                                ? "InlineValuesWithValueElement"
                                                : "InlineValues");

    std::string osOffset;
    for (auto nOffset : m_anOffset)
    {
        if (!osOffset.empty())
            osOffset += ',';
        osOffset += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nOffset));
    }
    if (!osOffset.empty())
    {
        CPLAddXMLAttributeAndValue(psSource, "offset", osOffset.c_str());
    }

    std::string osCount;
    size_t nValues = 1;
    for (auto nCount : m_anCount)
    {
        if (!osCount.empty())
            osCount += ',';
        nValues *= nCount;
        osCount += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nCount));
    }
    if (!osCount.empty())
    {
        CPLAddXMLAttributeAndValue(psSource, "count", osCount.c_str());
    }

    const auto dtString(GDALExtendedDataType::CreateString());
    const size_t nDTSize(dt.GetSize());
    if (dt.GetClass() == GEDTC_STRING)
    {
        CPLXMLNode *psLast = psSource->psChild;
        if (psLast)
        {
            while (psLast->psNext)
                psLast = psLast->psNext;
        }
        for (size_t i = 0; i < (m_bIsConstantValue ? 1 : nValues); ++i)
        {
            char *pszStr = nullptr;
            GDALExtendedDataType::CopyValue(&m_abyValues[i * nDTSize], dt,
                                            &pszStr, dtString);
            if (pszStr)
            {
                auto psNode =
                    CPLCreateXMLElementAndValue(nullptr, "Value", pszStr);
                if (psLast)
                    psLast->psNext = psNode;
                else
                    psSource->psChild = psNode;
                psLast = psNode;
                CPLFree(pszStr);
            }
        }
    }
    else
    {
        std::string osValues;
        for (size_t i = 0; i < (m_bIsConstantValue ? 1 : nValues); ++i)
        {
            if (i > 0)
                osValues += ' ';
            char *pszStr = nullptr;
            GDALExtendedDataType::CopyValue(&m_abyValues[i * nDTSize], dt,
                                            &pszStr, dtString);
            if (pszStr)
            {
                osValues += pszStr;
                CPLFree(pszStr);
            }
        }
        CPLCreateXMLNode(psSource, CXT_Text, osValues.c_str());
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::unique_ptr<VRTMDArraySourceFromArray>
VRTMDArraySourceFromArray::Create(const VRTMDArray *poDstArray,
                                  const CPLXMLNode *psNode)
{
    const char *pszFilename = CPLGetXMLValue(psNode, "SourceFilename", nullptr);
    if (pszFilename == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "SourceFilename element missing");
        return nullptr;
    }
    const char *pszRelativeToVRT =
        CPLGetXMLValue(psNode, "SourceFilename.relativetoVRT", nullptr);
    const bool bRelativeToVRTSet = pszRelativeToVRT != nullptr;
    const bool bRelativeToVRT =
        pszRelativeToVRT ? CPL_TO_BOOL(atoi(pszRelativeToVRT)) : false;
    const char *pszArray = CPLGetXMLValue(psNode, "SourceArray", "");
    const char *pszSourceBand = CPLGetXMLValue(psNode, "SourceBand", "");
    if (pszArray[0] == '\0' && pszSourceBand[0] == '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SourceArray or SourceBand element missing or empty");
        return nullptr;
    }
    if (pszArray[0] != '\0' && pszSourceBand[0] != '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SourceArray and SourceBand are exclusive");
        return nullptr;
    }

    const char *pszTranspose = CPLGetXMLValue(psNode, "SourceTranspose", "");
    std::vector<int> anTransposedAxis;
    CPLStringList aosTransposedAxis(CSLTokenizeString2(pszTranspose, ",", 0));
    for (int i = 0; i < aosTransposedAxis.size(); i++)
        anTransposedAxis.push_back(atoi(aosTransposedAxis[i]));

    const char *pszView = CPLGetXMLValue(psNode, "SourceView", "");

    const int nDimCount = static_cast<int>(poDstArray->GetDimensionCount());
    std::vector<GUInt64> anSrcOffset(nDimCount);
    std::vector<GUInt64> anCount(nDimCount);
    std::vector<GUInt64> anStep(nDimCount, 1);
    std::vector<GUInt64> anDstOffset(nDimCount);

    if (nDimCount > 0)
    {
        const CPLXMLNode *psSourceSlab = CPLGetXMLNode(psNode, "SourceSlab");
        if (psSourceSlab)
        {
            const char *pszOffset =
                CPLGetXMLValue(psSourceSlab, "offset", nullptr);
            if (pszOffset != nullptr)
            {
                CPLStringList aosTokensOffset(
                    CSLTokenizeString2(pszOffset, ", ", 0));
                if (aosTokensOffset.size() != nDimCount)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong number of values in offset");
                    return nullptr;
                }
                for (int i = 0; i < nDimCount; ++i)
                {
                    anSrcOffset[i] = static_cast<GUInt64>(CPLScanUIntBig(
                        aosTokensOffset[i],
                        static_cast<int>(strlen(aosTokensOffset[i]))));
                    if (aosTokensOffset[i][0] == '-')
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong value in offset");
                        return nullptr;
                    }
                }
            }

            const char *pszStep = CPLGetXMLValue(psSourceSlab, "step", nullptr);
            if (pszStep != nullptr)
            {
                CPLStringList aosTokensStep(
                    CSLTokenizeString2(pszStep, ", ", 0));
                if (aosTokensStep.size() != nDimCount)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong number of values in step");
                    return nullptr;
                }
                for (int i = 0; i < nDimCount; ++i)
                {
                    anStep[i] = static_cast<GUInt64>(CPLScanUIntBig(
                        aosTokensStep[i],
                        static_cast<int>(strlen(aosTokensStep[i]))));
                    if (aosTokensStep[i][0] == '-')
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong value in step");
                        return nullptr;
                    }
                }
            }

            const char *pszCount =
                CPLGetXMLValue(psSourceSlab, "count", nullptr);
            if (pszCount != nullptr)
            {
                CPLStringList aosTokensCount(
                    CSLTokenizeString2(pszCount, ", ", 0));
                if (aosTokensCount.size() != nDimCount)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong number of values in count");
                    return nullptr;
                }
                for (int i = 0; i < nDimCount; ++i)
                {
                    anCount[i] = static_cast<GUInt64>(CPLScanUIntBig(
                        aosTokensCount[i],
                        static_cast<int>(strlen(aosTokensCount[i]))));
                    if (aosTokensCount[i][0] == '-')
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong value in count");
                        return nullptr;
                    }
                }
            }
        }

        const CPLXMLNode *psDestSlab = CPLGetXMLNode(psNode, "DestSlab");
        if (psDestSlab)
        {
            const auto &dims(poDstArray->GetDimensions());
            const char *pszOffset =
                CPLGetXMLValue(psDestSlab, "offset", nullptr);
            if (pszOffset != nullptr)
            {
                CPLStringList aosTokensOffset(
                    CSLTokenizeString2(pszOffset, ", ", 0));
                if (aosTokensOffset.size() != nDimCount)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong number of values in offset");
                    return nullptr;
                }
                for (int i = 0; i < nDimCount; ++i)
                {
                    anDstOffset[i] = static_cast<GUInt64>(CPLScanUIntBig(
                        aosTokensOffset[i],
                        static_cast<int>(strlen(aosTokensOffset[i]))));
                    if (aosTokensOffset[i][0] == '-' ||
                        anDstOffset[i] >= dims[i]->GetSize())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong value in offset");
                        return nullptr;
                    }
                }
            }
        }
    }

    return std::make_unique<VRTMDArraySourceFromArray>(
        poDstArray, bRelativeToVRTSet, bRelativeToVRT, pszFilename, pszArray,
        pszSourceBand, std::move(anTransposedAxis), pszView,
        std::move(anSrcOffset), std::move(anCount), std::move(anStep),
        std::move(anDstOffset));
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTMDArraySourceFromArray::Serialize(CPLXMLNode *psParent,
                                          const char *pszVRTPath) const
{
    CPLXMLNode *psSource = CPLCreateXMLNode(psParent, CXT_Element, "Source");

    if (m_bRelativeToVRTSet)
    {
        auto psSourceFilename = CPLCreateXMLElementAndValue(
            psSource, "SourceFilename", m_osFilename.c_str());
        if (m_bRelativeToVRT)
        {
            CPLAddXMLAttributeAndValue(psSourceFilename, "relativetoVRT", "1");
        }
    }
    else
    {
        int bRelativeToVRT = FALSE;
        const char *pszSourceFilename = CPLExtractRelativePath(
            pszVRTPath, m_osFilename.c_str(), &bRelativeToVRT);
        auto psSourceFilename = CPLCreateXMLElementAndValue(
            psSource, "SourceFilename", pszSourceFilename);
        if (bRelativeToVRT)
        {
            CPLAddXMLAttributeAndValue(psSourceFilename, "relativetoVRT", "1");
        }
    }

    if (!m_osArray.empty())
        CPLCreateXMLElementAndValue(psSource, "SourceArray", m_osArray.c_str());
    else
        CPLCreateXMLElementAndValue(psSource, "SourceBand", m_osBand.c_str());

    if (!m_anTransposedAxis.empty())
    {
        std::string str;
        for (size_t i = 0; i < m_anTransposedAxis.size(); i++)
        {
            if (i > 0)
                str += ',';
            str += CPLSPrintf("%d", m_anTransposedAxis[i]);
        }
        CPLCreateXMLElementAndValue(psSource, "SourceTranspose", str.c_str());
    }

    if (!m_osViewExpr.empty())
    {
        CPLCreateXMLElementAndValue(psSource, "SourceView",
                                    m_osViewExpr.c_str());
    }

    if (m_poDstArray->GetDimensionCount() > 0)
    {
        CPLXMLNode *psSourceSlab =
            CPLCreateXMLNode(psSource, CXT_Element, "SourceSlab");
        {
            std::string str;
            for (size_t i = 0; i < m_anSrcOffset.size(); i++)
            {
                if (i > 0)
                    str += ',';
                str += CPLSPrintf(CPL_FRMT_GUIB,
                                  static_cast<GUIntBig>(m_anSrcOffset[i]));
            }
            CPLAddXMLAttributeAndValue(psSourceSlab, "offset", str.c_str());
        }
        {
            std::string str;
            for (size_t i = 0; i < m_anCount.size(); i++)
            {
                if (i > 0)
                    str += ',';
                str += CPLSPrintf(CPL_FRMT_GUIB,
                                  static_cast<GUIntBig>(m_anCount[i]));
            }
            CPLAddXMLAttributeAndValue(psSourceSlab, "count", str.c_str());
        }
        {
            std::string str;
            for (size_t i = 0; i < m_anStep.size(); i++)
            {
                if (i > 0)
                    str += ',';
                str += CPLSPrintf(CPL_FRMT_GUIB,
                                  static_cast<GUIntBig>(m_anStep[i]));
            }
            CPLAddXMLAttributeAndValue(psSourceSlab, "step", str.c_str());
        }

        CPLXMLNode *psDestSlab =
            CPLCreateXMLNode(psSource, CXT_Element, "DestSlab");
        {
            std::string str;
            for (size_t i = 0; i < m_anDstOffset.size(); i++)
            {
                if (i > 0)
                    str += ',';
                str += CPLSPrintf(CPL_FRMT_GUIB,
                                  static_cast<GUIntBig>(m_anDstOffset[i]));
            }
            CPLAddXMLAttributeAndValue(psDestSlab, "offset", str.c_str());
        }
    }
}

/************************************************************************/
/*                      ~VRTMDArraySourceFromArray()                    */
/************************************************************************/

VRTMDArraySourceFromArray::~VRTMDArraySourceFromArray()
{
    std::lock_guard<std::mutex> oGuard(g_cacheLock);

    // Remove from the cache datasets that are only used by this array
    // or drop our reference to those datasets
    std::unordered_set<std::string> oSetKeysToRemove;
    std::unordered_set<std::string> oSetKeysToDropReference;
    auto lambda = [&oSetKeysToRemove, &oSetKeysToDropReference,
                   this](const decltype(g_cacheSources)::node_type &key_value)
    {
        auto &listOfArrays(key_value.value.second);
        auto oIter = listOfArrays.find(this);
        if (oIter != listOfArrays.end())
        {
            if (listOfArrays.size() == 1)
                oSetKeysToRemove.insert(key_value.key);
            else
                oSetKeysToDropReference.insert(key_value.key);
        }
    };
    g_cacheSources.cwalk(lambda);
    for (const auto &key : oSetKeysToRemove)
    {
        CPLDebug("VRT", "Dropping %s", key.c_str());
        g_cacheSources.remove(key);
    }
    for (const auto &key : oSetKeysToDropReference)
    {
        CPLDebug("VRT", "Dropping reference to %s", key.c_str());
        CacheEntry oPair;
        g_cacheSources.tryGet(key, oPair);
        oPair.second.erase(this);
        g_cacheSources.insert(key, oPair);
    }
}

/************************************************************************/
/*                                   Read()                             */
/************************************************************************/

static std::string CreateKey(const std::string &filename)
{
    return filename + CPLSPrintf("__thread_" CPL_FRMT_GIB, CPLGetPID());
}

bool VRTMDArraySourceFromArray::Read(const GUInt64 *arrayStartIdx,
                                     const size_t *count,
                                     const GInt64 *arrayStep,
                                     const GPtrDiff_t *bufferStride,
                                     const GDALExtendedDataType &bufferDataType,
                                     void *pDstBuffer) const
{
    // Preliminary check without trying to open source array
    const auto nDims(m_poDstArray->GetDimensionCount());
    for (size_t i = 0; i < nDims; i++)
    {
        auto start_i = arrayStartIdx[i];
        auto step_i = arrayStep[i] == 0 ? 1 : arrayStep[i];
        if (arrayStep[i] < 0)
        {
            // For negative step request, temporarily simulate a positive step
            start_i = start_i - (m_anCount[i] - 1) * (-step_i);
            step_i = -step_i;
        }
        if (start_i + (count[i] - 1) * step_i < m_anDstOffset[i])
        {
            return true;
        }
    }

    for (size_t i = 0; i < nDims; i++)
    {
        if (m_anCount[i] == 0)  // we need to open the array...
            break;

        auto start_i = arrayStartIdx[i];
        auto step_i = arrayStep[i] == 0 ? 1 : arrayStep[i];
        if (arrayStep[i] < 0)
        {
            // For negative step request, temporarily simulate a positive step
            start_i = start_i - (m_anCount[i] - 1) * (-step_i);
            // step_i = -step_i;
        }
        if (start_i >= m_anDstOffset[i] + m_anCount[i])
        {
            return true;
        }
    }

    const std::string osFilename =
        m_bRelativeToVRT
            ? std::string(CPLProjectRelativeFilename(
                  m_poDstArray->GetVRTPath().c_str(), m_osFilename.c_str()))
            : m_osFilename;
    const std::string key(CreateKey(osFilename));

    std::shared_ptr<VRTArrayDatasetWrapper> poSrcDSWrapper;
    GDALDataset *poSrcDS;
    CacheEntry oPair;
    {
        std::lock_guard<std::mutex> oGuard(g_cacheLock);
        if (g_cacheSources.tryGet(key, oPair))
        {
            poSrcDSWrapper = oPair.first;
            poSrcDS = poSrcDSWrapper.get()->get();
            if (oPair.second.find(this) == oPair.second.end())
            {
                oPair.second.insert(this);
                g_cacheSources.insert(key, oPair);
            }
        }
        else
        {
            poSrcDS = GDALDataset::Open(
                osFilename.c_str(),
                (m_osBand.empty() ? GDAL_OF_MULTIDIM_RASTER : GDAL_OF_RASTER) |
                    GDAL_OF_INTERNAL | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr);
            if (!poSrcDS)
                return false;
            poSrcDSWrapper = std::make_shared<VRTArrayDatasetWrapper>(poSrcDS);
            oPair.first = std::move(poSrcDSWrapper);
            oPair.second.insert(this);
            g_cacheSources.insert(key, oPair);
        }
    }

    std::shared_ptr<GDALMDArray> poArray;
    if (m_osBand.empty())
    {
        auto rg(poSrcDS->GetRootGroup());
        if (rg == nullptr)
            return false;

        auto curGroup(rg);
        std::string arrayName(m_osArray);
        poArray = m_osArray[0] == '/' ? rg->OpenMDArrayFromFullname(arrayName)
                                      : curGroup->OpenMDArray(arrayName);
        if (poArray == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find array %s",
                     m_osArray.c_str());
            return false;
        }
    }
    else
    {
        int nSrcBand = atoi(m_osBand.c_str());
        auto poBand = poSrcDS->GetRasterBand(nSrcBand);
        if (poBand == nullptr)
            return false;
        poArray = poBand->AsMDArray();
        CPLAssert(poArray);
    }

    std::string osViewExpr = m_osViewExpr;
    if (STARTS_WITH(osViewExpr.c_str(), "resample=true,") ||
        osViewExpr == "resample=true")
    {
        poArray =
            poArray->GetResampled(std::vector<std::shared_ptr<GDALDimension>>(
                                      poArray->GetDimensionCount()),
                                  GRIORA_NearestNeighbour, nullptr, nullptr);
        if (poArray == nullptr)
        {
            return false;
        }
        if (osViewExpr == "resample=true")
            osViewExpr.clear();
        else
            osViewExpr = osViewExpr.substr(strlen("resample=true,"));
    }

    if (!m_anTransposedAxis.empty())
    {
        poArray = poArray->Transpose(m_anTransposedAxis);
        if (poArray == nullptr)
        {
            return false;
        }
    }
    if (!osViewExpr.empty())
    {
        poArray = poArray->GetView(osViewExpr);
        if (poArray == nullptr)
        {
            return false;
        }
    }
    if (m_poDstArray->GetDimensionCount() != poArray->GetDimensionCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent number of dimensions");
        return false;
    }

    const auto &srcDims(poArray->GetDimensions());
    std::vector<GUInt64> anReqDstStart(nDims);
    std::vector<size_t> anReqCount(nDims);
    // Compute the intersection between the inline value slab and the
    // request slab.
    for (size_t i = 0; i < nDims; i++)
    {
        if (m_anSrcOffset[i] >= srcDims[i]->GetSize())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid SourceSlab.offset");
            return false;
        }
        auto start_i = arrayStartIdx[i];
        auto step_i = arrayStep[i] == 0 ? 1 : arrayStep[i];
        if (arrayStep[i] < 0)
        {
            if (m_anCount[i] == 0)
                m_anCount[i] = (m_anSrcOffset[i] + 1) / -step_i;
            // For negative step request, temporarily simulate a positive step
            // and fix up the start at the end of the loop.
            start_i = start_i - (m_anCount[i] - 1) * (-step_i);
            step_i = -step_i;
        }
        else
        {
            if (m_anCount[i] == 0)
                m_anCount[i] =
                    (srcDims[i]->GetSize() - m_anSrcOffset[i]) / step_i;
        }

        const auto nRightDstOffsetFromConfig = m_anDstOffset[i] + m_anCount[i];
        if (start_i >= nRightDstOffsetFromConfig)
        {
            return true;
        }
        if (start_i < m_anDstOffset[i])
        {
            anReqDstStart[i] =
                m_anDstOffset[i] +
                (step_i - ((m_anDstOffset[i] - start_i) % step_i)) % step_i;
        }
        else
        {
            anReqDstStart[i] = start_i;
        }
        anReqCount[i] = 1 + static_cast<size_t>(
                                (std::min(nRightDstOffsetFromConfig - 1,
                                          start_i + (count[i] - 1) * step_i) -
                                 anReqDstStart[i]) /
                                step_i);
        if (arrayStep[i] < 0)
        {
            anReqDstStart[i] = anReqDstStart[i] + (anReqCount[i] - 1) * step_i;
        }
    }

    GPtrDiff_t nDstOffset = 0;
    const auto nBufferDataTypeSize(bufferDataType.GetSize());
    std::vector<GUInt64> anSrcArrayOffset(nDims);
    std::vector<GInt64> anSrcArrayStep(nDims);
    for (size_t i = 0; i < nDims; i++)
    {
        const size_t nRelStartDst =
            static_cast<size_t>(anReqDstStart[i] - arrayStartIdx[i]);
        nDstOffset += nRelStartDst * bufferStride[i] * nBufferDataTypeSize;
        anSrcArrayOffset[i] =
            m_anSrcOffset[i] +
            (anReqDstStart[i] - m_anDstOffset[i]) * m_anStep[i];
        if (arrayStep[i] < 0)
            anSrcArrayStep[i] = -static_cast<GInt64>(
                m_anStep[i] * static_cast<GUInt64>(-arrayStep[i]));
        else
            anSrcArrayStep[i] = m_anStep[i] * arrayStep[i];
    }
    return poArray->Read(anSrcArrayOffset.data(), anReqCount.data(),
                         anSrcArrayStep.data(), bufferStride, bufferDataType,
                         static_cast<GByte *>(pDstBuffer) + nDstOffset);
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool VRTMDArray::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                       const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                       const GDALExtendedDataType &bufferDataType,
                       void *pDstBuffer) const
{
    const auto nDims(m_dims.size());

    // Initialize pDstBuffer
    bool bFullyCompactStride = true;
    std::map<size_t, size_t> mapStrideToIdx;
    for (size_t i = 0; i < nDims; i++)
    {
        if (bufferStride[i] < 0 ||
            mapStrideToIdx.find(static_cast<size_t>(bufferStride[i])) !=
                mapStrideToIdx.end())
        {
            bFullyCompactStride = false;
            break;
        }
        mapStrideToIdx[static_cast<size_t>(bufferStride[i])] = i;
    }
    size_t nAccStride = 1;
    if (bFullyCompactStride)
    {
        for (size_t i = 0; i < nDims; i++)
        {
            auto oIter = mapStrideToIdx.find(nAccStride);
            if (oIter == mapStrideToIdx.end())
            {
                bFullyCompactStride = false;
                break;
            }
            nAccStride = nAccStride * count[oIter->second];
        }
    }

    const auto nDTSize(m_dt.GetSize());
    const auto nBufferDTSize(bufferDataType.GetSize());
    const GByte *pabyNoData = static_cast<const GByte *>(GetRawNoDataValue());
    std::vector<GByte> abyFill;
    if (pabyNoData)
    {
        bool bAllZero = true;
        for (size_t i = 0; i < nDTSize; i++)
        {
            if (pabyNoData[i])
            {
                bAllZero = false;
                break;
            }
        }
        if (bAllZero)
        {
            pabyNoData = nullptr;
        }
        else
        {
            abyFill.resize(nBufferDTSize);
            GDALExtendedDataType::CopyValue(pabyNoData, m_dt, &abyFill[0],
                                            bufferDataType);
        }
    }

    if (bFullyCompactStride)
    {
        if (pabyNoData == nullptr)
        {
            memset(pDstBuffer, 0, nAccStride * nBufferDTSize);
        }
        else if (bufferDataType.NeedsFreeDynamicMemory())
        {
            GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
            for (size_t i = 0; i < nAccStride; i++)
            {
                GDALExtendedDataType::CopyValue(pabyDstBuffer, bufferDataType,
                                                &abyFill[0], bufferDataType);
                pabyDstBuffer += nBufferDTSize;
            }
        }
        else
        {
            GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
            for (size_t i = 0; i < nAccStride; i++)
            {
                memcpy(pabyDstBuffer, &abyFill[0], nBufferDTSize);
                pabyDstBuffer += nBufferDTSize;
            }
        }
    }
    else
    {
        const bool bNeedsDynamicMemory =
            bufferDataType.NeedsFreeDynamicMemory();
        std::vector<size_t> anStackCount(nDims);
        std::vector<GByte *> abyStackDstPtr;
        size_t iDim = 0;
        abyStackDstPtr.push_back(static_cast<GByte *>(pDstBuffer));
        abyStackDstPtr.resize(nDims + 1);
    lbl_next_depth:
        if (iDim == nDims)
        {
            if (pabyNoData == nullptr)
            {
                memset(abyStackDstPtr[nDims], 0, nBufferDTSize);
            }
            else if (bNeedsDynamicMemory)
            {
                GDALExtendedDataType::CopyValue(abyStackDstPtr[nDims],
                                                bufferDataType, &abyFill[0],
                                                bufferDataType);
            }
            else
            {
                memcpy(abyStackDstPtr[nDims], &abyFill[0], nBufferDTSize);
            }
        }
        else
        {
            anStackCount[iDim] = count[iDim];
            while (true)
            {
                ++iDim;
                abyStackDstPtr[iDim] = abyStackDstPtr[iDim - 1];
                goto lbl_next_depth;
            lbl_return_to_caller:
                --iDim;
                --anStackCount[iDim];
                if (anStackCount[iDim] == 0)
                    break;
                abyStackDstPtr[iDim] += bufferStride[iDim] * nBufferDTSize;
            }
        }
        if (iDim > 0)
            goto lbl_return_to_caller;
    }

    if (!abyFill.empty())
    {
        bufferDataType.FreeDynamicMemory(&abyFill[0]);
    }

    for (const auto &poSource : m_sources)
    {
        if (!poSource->Read(arrayStartIdx, count, arrayStep, bufferStride,
                            bufferDataType, pDstBuffer))
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                             SetDirty()                               */
/************************************************************************/

void VRTMDArray::SetDirty()
{
    auto poGroup(GetGroup());
    if (poGroup)
    {
        poGroup->SetDirty();
    }
}

/************************************************************************/
/*                                GetGroup()                            */
/************************************************************************/

VRTGroup *VRTMDArray::GetGroup() const
{
    auto ref = m_poGroupRef.lock();
    return ref ? ref->m_ptr : nullptr;
}

/************************************************************************/
/*                           CreateAttribute()                          */
/************************************************************************/

std::shared_ptr<GDALAttribute>
VRTMDArray::CreateAttribute(const std::string &osName,
                            const std::vector<GUInt64> &anDimensions,
                            const GDALExtendedDataType &oDataType, CSLConstList)
{
    if (!VRTAttribute::CreationCommonChecks(osName, anDimensions,
                                            m_oMapAttributes))
    {
        return nullptr;
    }
    SetDirty();
    auto newAttr(std::make_shared<VRTAttribute>(
        GetFullName(), osName, anDimensions.empty() ? 0 : anDimensions[0],
        oDataType));
    m_oMapAttributes[osName] = newAttr;
    return newAttr;
}

/************************************************************************/
/*                               CopyFrom()                             */
/************************************************************************/

bool VRTMDArray::CopyFrom(GDALDataset *poSrcDS, const GDALMDArray *poSrcArray,
                          bool bStrict, GUInt64 &nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    nCurCost += GDALMDArray::COPY_COST;

    if (!CopyFromAllExceptValues(poSrcArray, bStrict, nCurCost, nTotalCost,
                                 pfnProgress, pProgressData))
    {
        return false;
    }

    nCurCost += GetTotalElementsCount() * GetDataType().GetSize();

    if (poSrcDS)
    {
        const auto nDims(GetDimensionCount());
        if (nDims == 1 && m_dims[0]->GetSize() > 2 &&
            m_dims[0]->GetSize() < 10 * 1000 * 1000)
        {
            std::vector<double> adfTmp(
                static_cast<size_t>(m_dims[0]->GetSize()));
            const GUInt64 anStart[] = {0};
            const size_t nCount = adfTmp.size();
            const size_t anCount[] = {nCount};
            if (poSrcArray->Read(anStart, anCount, nullptr, nullptr,
                                 GDALExtendedDataType::Create(GDT_Float64),
                                 &adfTmp[0]))
            {
                bool bRegular = true;
                const double dfSpacing =
                    (adfTmp.back() - adfTmp[0]) / (nCount - 1);
                for (size_t i = 1; i < nCount; i++)
                {
                    if (fabs((adfTmp[i] - adfTmp[i - 1]) - dfSpacing) >
                        1e-3 * fabs(dfSpacing))
                    {
                        bRegular = false;
                        break;
                    }
                }
                if (bRegular)
                {
                    std::unique_ptr<VRTMDArraySourceRegularlySpaced> poSource(
                        new VRTMDArraySourceRegularlySpaced(adfTmp[0],
                                                            dfSpacing));
                    AddSource(std::move(poSource));
                }
            }
        }

        if (m_sources.empty())
        {
            std::vector<GUInt64> anSrcOffset(nDims);
            std::vector<GUInt64> anCount(nDims);
            std::vector<GUInt64> anStep(nDims, 1);
            std::vector<GUInt64> anDstOffset(nDims);
            for (size_t i = 0; i < nDims; i++)
                anCount[i] = m_dims[i]->GetSize();

            std::unique_ptr<VRTMDArraySource> poSource(
                new VRTMDArraySourceFromArray(
                    this, false, false, poSrcDS->GetDescription(),
                    poSrcArray->GetFullName(),
                    std::string(),       // osBand
                    std::vector<int>(),  // anTransposedAxis,
                    std::string(),       // osViewExpr
                    std::move(anSrcOffset), std::move(anCount),
                    std::move(anStep), std::move(anDstOffset)));
            AddSource(std::move(poSource));
        }
    }

    return true;
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void *VRTMDArray::GetRawNoDataValue() const
{
    return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
}

/************************************************************************/
/*                          SetRawNoDataValue()                         */
/************************************************************************/

bool VRTMDArray::SetRawNoDataValue(const void *pNoData)
{
    SetDirty();

    if (!m_abyNoData.empty())
    {
        m_dt.FreeDynamicMemory(&m_abyNoData[0]);
    }

    if (pNoData == nullptr)
    {
        m_abyNoData.clear();
    }
    else
    {
        const auto nSize = m_dt.GetSize();
        m_abyNoData.resize(nSize);
        memset(&m_abyNoData[0], 0, nSize);
        GDALExtendedDataType::CopyValue(pNoData, m_dt, &m_abyNoData[0], m_dt);
    }
    return true;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

bool VRTMDArray::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    SetDirty();

    m_poSRS.reset();
    if (poSRS)
    {
        m_poSRS = std::shared_ptr<OGRSpatialReference>(poSRS->Clone());
    }
    return true;
}

/************************************************************************/
/*                            AddSource()                               */
/************************************************************************/

void VRTMDArray::AddSource(std::unique_ptr<VRTMDArraySource> &&poSource)
{
    SetDirty();

    m_sources.emplace_back(std::move(poSource));
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void VRTMDArray::Serialize(CPLXMLNode *psParent, const char *pszVRTPath) const
{
    CPLXMLNode *psArray = CPLCreateXMLNode(psParent, CXT_Element, "Array");
    CPLAddXMLAttributeAndValue(psArray, "name", GetName().c_str());
    CPLXMLNode *psDataType = CPLCreateXMLNode(psArray, CXT_Element, "DataType");
    if (m_dt.GetClass() == GEDTC_STRING)
        CPLCreateXMLNode(psDataType, CXT_Text, "String");
    else
        CPLCreateXMLNode(psDataType, CXT_Text,
                         GDALGetDataTypeName(m_dt.GetNumericDataType()));
    for (const auto &dim : m_dims)
    {
        auto vrtDim(std::dynamic_pointer_cast<VRTDimension>(dim));
        CPLAssert(vrtDim);
        auto poGroup(GetGroup());
        bool bSerializeDim = true;
        if (poGroup)
        {
            auto groupDim(
                poGroup->GetDimensionFromFullName(dim->GetFullName(), false));
            if (groupDim && groupDim->GetSize() == dim->GetSize())
            {
                bSerializeDim = false;
                CPLAssert(groupDim->GetGroup());
                CPLXMLNode *psDimRef =
                    CPLCreateXMLNode(psArray, CXT_Element, "DimensionRef");
                CPLAddXMLAttributeAndValue(psDimRef, "ref",
                                           groupDim->GetGroup() == poGroup
                                               ? dim->GetName().c_str()
                                               : dim->GetFullName().c_str());
            }
        }
        if (bSerializeDim)
        {
            vrtDim->Serialize(psArray);
        }
    }

    if (m_poSRS && !m_poSRS->IsEmpty())
    {
        char *pszWKT = nullptr;
        const char *const apszOptions[2] = {"FORMAT=WKT2_2018", nullptr};
        m_poSRS->exportToWkt(&pszWKT, apszOptions);
        CPLXMLNode *psSRSNode =
            CPLCreateXMLElementAndValue(psArray, "SRS", pszWKT);
        CPLFree(pszWKT);
        const auto &mapping = m_poSRS->GetDataAxisToSRSAxisMapping();
        CPLString osMapping;
        for (size_t i = 0; i < mapping.size(); ++i)
        {
            if (!osMapping.empty())
                osMapping += ",";
            osMapping += CPLSPrintf("%d", mapping[i]);
        }
        CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                   osMapping.c_str());
    }

    if (!m_osUnit.empty())
    {
        CPLCreateXMLElementAndValue(psArray, "Unit", m_osUnit.c_str());
    }

    bool bHasNodata = false;
    double dfNoDataValue = GetNoDataValueAsDouble(&bHasNodata);
    if (bHasNodata)
    {
        CPLSetXMLValue(
            psArray, "NoDataValue",
            VRTSerializeNoData(dfNoDataValue, m_dt.GetNumericDataType(), 18)
                .c_str());
    }

    if (m_bHasOffset)
    {
        CPLCreateXMLElementAndValue(psArray, "Offset",
                                    CPLSPrintf("%.17g", m_dfOffset));
    }

    if (m_bHasScale)
    {
        CPLCreateXMLElementAndValue(psArray, "Scale",
                                    CPLSPrintf("%.17g", m_dfScale));
    }

    for (const auto &poSource : m_sources)
    {
        poSource->Serialize(psArray, pszVRTPath);
    }

    for (const auto &iter : m_oMapAttributes)
    {
        iter.second->Serialize(psArray);
    }
}

/************************************************************************/
/*                           VRTArraySource()                           */
/************************************************************************/

class VRTArraySource : public VRTSource
{
    std::unique_ptr<CPLXMLNode, CPLXMLTreeCloserDeleter> m_poXMLTree{};
    std::unique_ptr<GDALDataset> m_poDS{};
    std::unique_ptr<VRTSimpleSource> m_poSimpleSource{};

  public:
    VRTArraySource() = default;

    CPLErr RasterIO(GDALDataType eBandDataType, int nXOff, int nYOff,
                    int nXSize, int nYSize, void *pData, int nBufXSize,
                    int nBufYSize, GDALDataType eBufType, GSpacing nPixelSpace,
                    GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg,
                    WorkingState &oWorkingState) override;

    double GetMinimum(int nXSize, int nYSize, int *pbSuccess) override
    {
        return m_poSimpleSource->GetMinimum(nXSize, nYSize, pbSuccess);
    }

    double GetMaximum(int nXSize, int nYSize, int *pbSuccess) override
    {
        return m_poSimpleSource->GetMaximum(nXSize, nYSize, pbSuccess);
    }

    CPLErr GetHistogram(int nXSize, int nYSize, double dfMin, double dfMax,
                        int nBuckets, GUIntBig *panHistogram,
                        int bIncludeOutOfRange, int bApproxOK,
                        GDALProgressFunc pfnProgress,
                        void *pProgressData) override
    {
        return m_poSimpleSource->GetHistogram(
            nXSize, nYSize, dfMin, dfMax, nBuckets, panHistogram,
            bIncludeOutOfRange, bApproxOK, pfnProgress, pProgressData);
    }

    const char *GetType() const override
    {
        return "ArraySource";
    }

    CPLErr XMLInit(const CPLXMLNode *psTree, const char *pszVRTPath,
                   VRTMapSharedResources &oMapSharedSources) override;
    CPLXMLNode *SerializeToXML(const char *pszVRTPath) override;
};

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr VRTArraySource::RasterIO(GDALDataType eBandDataType, int nXOff,
                                int nYOff, int nXSize, int nYSize, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, GSpacing nPixelSpace,
                                GSpacing nLineSpace,
                                GDALRasterIOExtraArg *psExtraArg,
                                WorkingState &oWorkingState)
{
    return m_poSimpleSource->RasterIO(eBandDataType, nXOff, nYOff, nXSize,
                                      nYSize, pData, nBufXSize, nBufYSize,
                                      eBufType, nPixelSpace, nLineSpace,
                                      psExtraArg, oWorkingState);
}

/************************************************************************/
/*                        ParseSingleSourceArray()                      */
/************************************************************************/

static std::shared_ptr<GDALMDArray>
ParseSingleSourceArray(const CPLXMLNode *psSingleSourceArray,
                       const char *pszVRTPath)
{
    const auto psSourceFileNameNode =
        CPLGetXMLNode(psSingleSourceArray, "SourceFilename");
    if (!psSourceFileNameNode)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <SourceFilename> in <SingleSourceArray>");
        return nullptr;
    }
    const char *pszSourceFilename =
        CPLGetXMLValue(psSourceFileNameNode, nullptr, "");
    const bool bRelativeToVRT = CPL_TO_BOOL(
        atoi(CPLGetXMLValue(psSourceFileNameNode, "relativeToVRT", "0")));

    const char *pszSourceArray =
        CPLGetXMLValue(psSingleSourceArray, "SourceArray", nullptr);
    if (!pszSourceArray)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <SourceArray> in <SingleSourceArray>");
        return nullptr;
    }
    const std::string osSourceFilename(
        bRelativeToVRT
            ? CPLProjectRelativeFilename(pszVRTPath, pszSourceFilename)
            : pszSourceFilename);
    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(osSourceFilename.c_str(),
                          GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR,
                          nullptr, nullptr, nullptr));
    if (!poDS)
        return nullptr;
    auto poRG = poDS->GetRootGroup();
    if (!poRG)
        return nullptr;
    auto poArray = poRG->OpenMDArrayFromFullname(pszSourceArray);
    if (!poArray)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find array '%s' in %s",
                 pszSourceArray, osSourceFilename.c_str());
    }
    return poArray;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTArraySource::XMLInit(const CPLXMLNode *psTree, const char *pszVRTPath,
                               VRTMapSharedResources & /*oMapSharedSources*/)
{
    const auto poArray = ParseArray(psTree, pszVRTPath, "ArraySource");
    if (!poArray)
    {
        return CE_Failure;
    }
    if (poArray->GetDimensionCount() != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Array referenced in <ArraySource> should be a "
                 "two-dimensional array");
        return CE_Failure;
    }

    m_poDS.reset(poArray->AsClassicDataset(1, 0));
    if (!m_poDS)
        return CE_Failure;

    m_poSimpleSource = std::make_unique<VRTSimpleSource>();
    auto poBand = m_poDS->GetRasterBand(1);
    m_poSimpleSource->SetSrcBand(poBand);
    m_poDS->Reference();

    if (m_poSimpleSource->ParseSrcRectAndDstRect(psTree) != CE_None)
        return CE_Failure;
    if (!CPLGetXMLNode(psTree, "SrcRect"))
        m_poSimpleSource->SetSrcWindow(0, 0, poBand->GetXSize(),
                                       poBand->GetYSize());
    if (!CPLGetXMLNode(psTree, "DstRect"))
        m_poSimpleSource->SetDstWindow(0, 0, poBand->GetXSize(),
                                       poBand->GetYSize());

    m_poXMLTree.reset(CPLCloneXMLTree(psTree));
    return CE_None;
}

/************************************************************************/
/*                          SerializeToXML()                            */
/************************************************************************/

CPLXMLNode *VRTArraySource::SerializeToXML(const char * /*pszVRTPath*/)
{
    if (m_poXMLTree)
    {
        return CPLCloneXMLTree(m_poXMLTree.get());
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VRTArraySource::SerializeToXML() not implemented");
        return nullptr;
    }
}

/************************************************************************/
/*                     VRTDerivedArrayCreate()                          */
/************************************************************************/

std::shared_ptr<GDALMDArray> VRTDerivedArrayCreate(const char *pszVRTPath,
                                                   const CPLXMLNode *psTree)
{
    auto poArray = ParseArray(psTree, pszVRTPath, "DerivedArray");

    const auto GetOptions =
        [](const CPLXMLNode *psParent, CPLStringList &aosOptions)
    {
        for (const CPLXMLNode *psOption = CPLGetXMLNode(psParent, "Option");
             psOption; psOption = psOption->psNext)
        {
            if (psOption->eType == CXT_Element &&
                strcmp(psOption->pszValue, "Option") == 0)
            {
                const char *pszName = CPLGetXMLValue(psOption, "name", nullptr);
                if (!pszName)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Cannot find 'name' attribute in <Option> element");
                    return false;
                }
                const char *pszValue = CPLGetXMLValue(psOption, nullptr, "");
                aosOptions.SetNameValue(pszName, pszValue);
            }
        }
        return true;
    };

    for (const CPLXMLNode *psStep = CPLGetXMLNode(psTree, "Step");
         psStep && poArray; psStep = psStep->psNext)
    {
        if (psStep->eType != CXT_Element ||
            strcmp(psStep->pszValue, "Step") != 0)
            continue;

        if (const CPLXMLNode *psView = CPLGetXMLNode(psStep, "View"))
        {
            const char *pszExpr = CPLGetXMLValue(psView, "expr", nullptr);
            if (!pszExpr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find 'expr' attribute in <View> element");
                return nullptr;
            }
            poArray = poArray->GetView(pszExpr);
        }
        else if (const CPLXMLNode *psTranspose =
                     CPLGetXMLNode(psStep, "Transpose"))
        {
            const char *pszOrder =
                CPLGetXMLValue(psTranspose, "newOrder", nullptr);
            if (!pszOrder)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot find 'newOrder' attribute in <Transpose> element");
                return nullptr;
            }
            std::vector<int> anMapNewAxisToOldAxis;
            const CPLStringList aosItems(CSLTokenizeString2(pszOrder, ",", 0));
            for (int i = 0; i < aosItems.size(); ++i)
                anMapNewAxisToOldAxis.push_back(atoi(aosItems[i]));
            poArray = poArray->Transpose(anMapNewAxisToOldAxis);
        }
        else if (const CPLXMLNode *psResample =
                     CPLGetXMLNode(psStep, "Resample"))
        {
            std::vector<std::shared_ptr<GDALDimension>> apoNewDims;
            auto poDummyGroup = std::shared_ptr<VRTGroup>(
                new VRTGroup(pszVRTPath ? pszVRTPath : ""));
            for (const CPLXMLNode *psDimension =
                     CPLGetXMLNode(psResample, "Dimension");
                 psDimension; psDimension = psDimension->psNext)
            {
                if (psDimension->eType == CXT_Element &&
                    strcmp(psDimension->pszValue, "Dimension") == 0)
                {
                    auto apoDim = VRTDimension::Create(
                        poDummyGroup, std::string(), psDimension);
                    if (!apoDim)
                        return nullptr;
                    apoNewDims.emplace_back(std::move(apoDim));
                }
            }
            if (apoNewDims.empty())
                apoNewDims.resize(poArray->GetDimensionCount());

            const char *pszResampleAlg =
                CPLGetXMLValue(psResample, "ResampleAlg", "NEAR");
            const auto eResampleAlg =
                GDALRasterIOGetResampleAlg(pszResampleAlg);

            std::unique_ptr<OGRSpatialReference> poSRS;
            const char *pszSRS = CPLGetXMLValue(psResample, "SRS", nullptr);
            if (pszSRS)
            {
                poSRS = std::make_unique<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->SetFromUserInput(
                        pszSRS, OGRSpatialReference::
                                    SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                    OGRERR_NONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for <SRS>");
                    return nullptr;
                }
            }

            CPLStringList aosOptions;
            if (!GetOptions(psResample, aosOptions))
                return nullptr;

            poArray = poArray->GetResampled(apoNewDims, eResampleAlg,
                                            poSRS.get(), aosOptions.List());
        }
        else if (const CPLXMLNode *psGrid = CPLGetXMLNode(psStep, "Grid"))
        {
            const char *pszGridOptions =
                CPLGetXMLValue(psGrid, "GridOptions", nullptr);
            if (!pszGridOptions)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find <GridOptions> in <Grid> element");
                return nullptr;
            }

            std::shared_ptr<GDALMDArray> poXArray;
            if (const CPLXMLNode *psXArrayNode =
                    CPLGetXMLNode(psGrid, "XArray"))
            {
                poXArray = ParseArray(psXArrayNode, pszVRTPath, "XArray");
                if (!poXArray)
                    return nullptr;
            }

            std::shared_ptr<GDALMDArray> poYArray;
            if (const CPLXMLNode *psYArrayNode =
                    CPLGetXMLNode(psGrid, "YArray"))
            {
                poYArray = ParseArray(psYArrayNode, pszVRTPath, "YArray");
                if (!poYArray)
                    return nullptr;
            }

            CPLStringList aosOptions;
            if (!GetOptions(psGrid, aosOptions))
                return nullptr;

            poArray = poArray->GetGridded(pszGridOptions, poXArray, poYArray,
                                          aosOptions.List());
        }
        else if (const CPLXMLNode *psGetMask = CPLGetXMLNode(psStep, "GetMask"))
        {
            CPLStringList aosOptions;
            if (!GetOptions(psGetMask, aosOptions))
                return nullptr;

            poArray = poArray->GetMask(aosOptions.List());
        }
        else if (CPLGetXMLNode(psStep, "GetUnscaled"))
        {
            poArray = poArray->GetUnscaled();
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown <Step>.<%s> element",
                     psStep->psChild ? psStep->psChild->pszValue : "(null)");
            return nullptr;
        }
    }

    return poArray;
}

/************************************************************************/
/*                              ParseArray()                            */
/************************************************************************/

static std::shared_ptr<GDALMDArray> ParseArray(const CPLXMLNode *psTree,
                                               const char *pszVRTPath,
                                               const char *pszParentXMLNode)
{
    if (const CPLXMLNode *psSingleSourceArrayNode =
            CPLGetXMLNode(psTree, "SingleSourceArray"))
        return ParseSingleSourceArray(psSingleSourceArrayNode, pszVRTPath);

    if (const CPLXMLNode *psArrayNode = CPLGetXMLNode(psTree, "Array"))
    {
        return VRTMDArray::Create(pszVRTPath, psArrayNode);
    }

    if (const CPLXMLNode *psDerivedArrayNode =
            CPLGetXMLNode(psTree, "DerivedArray"))
    {
        return VRTDerivedArrayCreate(pszVRTPath, psDerivedArrayNode);
    }

    CPLError(
        CE_Failure, CPLE_AppDefined,
        "Cannot find a <SimpleSourceArray>, <Array> or <DerivedArray> in <%s>",
        pszParentXMLNode);
    return nullptr;
}

/************************************************************************/
/*                       VRTParseArraySource()                          */
/************************************************************************/

VRTSource *VRTParseArraySource(const CPLXMLNode *psChild,
                               const char *pszVRTPath,
                               VRTMapSharedResources &oMapSharedSources)
{
    VRTSource *poSource = nullptr;

    if (EQUAL(psChild->pszValue, "ArraySource"))
    {
        poSource = new VRTArraySource();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VRTParseArraySource() - Unknown source : %s",
                 psChild->pszValue);
        return nullptr;
    }

    if (poSource->XMLInit(psChild, pszVRTPath, oMapSharedSources) == CE_None)
        return poSource;

    delete poSource;
    return nullptr;
}

/*! @endcond */
