/******************************************************************************
 *
 * Name:     gdalmultidim_group.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALGroup class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"
#include "gdal_multidim.h"

#include <list>
#include <map>
#include <queue>
#include <set>

/************************************************************************/
/*                             GDALGroup()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALGroup::GDALGroup(const std::string &osParentName, const std::string &osName,
                     const std::string &osContext)
    : m_osName(osParentName.empty() ? "/" : osName),
      m_osFullName(
          !osParentName.empty()
              ? ((osParentName == "/" ? "/" : osParentName + "/") + osName)
              : "/"),
      m_osContext(osContext)
{
}

//! @endcond

/************************************************************************/
/*                             ~GDALGroup()                             */
/************************************************************************/

GDALGroup::~GDALGroup() = default;

/************************************************************************/
/*                          GetMDArrayNames()                           */
/************************************************************************/

/** Return the list of multidimensional array names contained in this group.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * OpenMDArray() should also be implemented.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetMDArrayNames().
 *
 * @param papszOptions Driver specific options determining how arrays
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the array names.
 */
std::vector<std::string>
GDALGroup::GetMDArrayNames(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                    GetMDArrayFullNamesRecursive()                    */
/************************************************************************/

/** Return the list of multidimensional array full names contained in this
 * group and its subgroups.
 *
 * This is the same as the C function GDALGroupGetMDArrayFullNamesRecursive().
 *
 * @param papszGroupOptions Driver specific options determining how groups
 * should be retrieved. Pass nullptr for default behavior.
 * @param papszArrayOptions Driver specific options determining how arrays
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the array full names.
 *
 * @since 3.11
 */
std::vector<std::string>
GDALGroup::GetMDArrayFullNamesRecursive(CSLConstList papszGroupOptions,
                                        CSLConstList papszArrayOptions) const
{
    std::vector<std::string> ret;
    std::list<std::shared_ptr<GDALGroup>> stackGroups;
    stackGroups.push_back(nullptr);  // nullptr means this
    while (!stackGroups.empty())
    {
        std::shared_ptr<GDALGroup> groupPtr = std::move(stackGroups.front());
        stackGroups.erase(stackGroups.begin());
        const GDALGroup *poCurGroup = groupPtr ? groupPtr.get() : this;
        for (const std::string &arrayName :
             poCurGroup->GetMDArrayNames(papszArrayOptions))
        {
            std::string osFullName = poCurGroup->GetFullName();
            if (!osFullName.empty() && osFullName.back() != '/')
                osFullName += '/';
            osFullName += arrayName;
            ret.push_back(std::move(osFullName));
        }
        auto insertionPoint = stackGroups.begin();
        for (const auto &osSubGroup :
             poCurGroup->GetGroupNames(papszGroupOptions))
        {
            auto poSubGroup = poCurGroup->OpenGroup(osSubGroup);
            if (poSubGroup)
                stackGroups.insert(insertionPoint, std::move(poSubGroup));
        }
    }

    return ret;
}

/************************************************************************/
/*                            OpenMDArray()                             */
/************************************************************************/

/** Open and return a multidimensional array.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * GetMDArrayNames() should also be implemented.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupOpenMDArray().
 *
 * @param osName Array name.
 * @param papszOptions Driver specific options determining how the array should
 * be opened.  Pass nullptr for default behavior.
 *
 * @return the array, or nullptr.
 */
std::shared_ptr<GDALMDArray>
GDALGroup::OpenMDArray(CPL_UNUSED const std::string &osName,
                       CPL_UNUSED CSLConstList papszOptions) const
{
    return nullptr;
}

/************************************************************************/
/*                           GetGroupNames()                            */
/************************************************************************/

/** Return the list of sub-groups contained in this group.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * OpenGroup() should also be implemented.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetGroupNames().
 *
 * @param papszOptions Driver specific options determining how groups
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the group names.
 */
std::vector<std::string>
GDALGroup::GetGroupNames(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

/** Open and return a sub-group.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * GetGroupNames() should also be implemented.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupOpenGroup().
 *
 * @param osName Sub-group name.
 * @param papszOptions Driver specific options determining how the sub-group
 * should be opened.  Pass nullptr for default behavior.
 *
 * @return the group, or nullptr.
 */
std::shared_ptr<GDALGroup>
GDALGroup::OpenGroup(CPL_UNUSED const std::string &osName,
                     CPL_UNUSED CSLConstList papszOptions) const
{
    return nullptr;
}

/************************************************************************/
/*                        GetVectorLayerNames()                         */
/************************************************************************/

/** Return the list of layer names contained in this group.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * OpenVectorLayer() should also be implemented.
 *
 * Drivers known to implement it: OpenFileGDB, FileGDB
 *
 * Other drivers will return an empty list. GDALDataset::GetLayerCount() and
 * GDALDataset::GetLayer() should then be used.
 *
 * This is the same as the C function GDALGroupGetVectorLayerNames().
 *
 * @param papszOptions Driver specific options determining how layers
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the vector layer names.
 * @since GDAL 3.4
 */
std::vector<std::string>
GDALGroup::GetVectorLayerNames(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                          OpenVectorLayer()                           */
/************************************************************************/

/** Open and return a vector layer.
 *
 * Due to the historical ownership of OGRLayer* by GDALDataset*, the
 * lifetime of the returned OGRLayer* is linked to the one of the owner
 * dataset (contrary to the general design of this class where objects can be
 * used independently of the object that returned them)
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * GetVectorLayerNames() should also be implemented.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupOpenVectorLayer().
 *
 * @param osName Vector layer name.
 * @param papszOptions Driver specific options determining how the layer should
 * be opened.  Pass nullptr for default behavior.
 *
 * @return the group, or nullptr.
 */
OGRLayer *GDALGroup::OpenVectorLayer(CPL_UNUSED const std::string &osName,
                                     CPL_UNUSED CSLConstList papszOptions) const
{
    return nullptr;
}

/************************************************************************/
/*                           GetDimensions()                            */
/************************************************************************/

/** Return the list of dimensions contained in this group and used by its
 * arrays.
 *
 * This is for dimensions that can potentially be used by several arrays.
 * Not all drivers might implement this. To retrieve the dimensions used by
 * a specific array, use GDALMDArray::GetDimensions().
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupGetDimensions().
 *
 * @param papszOptions Driver specific options determining how groups
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return the dimensions.
 */
std::vector<std::shared_ptr<GDALDimension>>
GDALGroup::GetDimensions(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

/** Return structural information on the group.
 *
 * This may be the compression, etc..
 *
 * The return value should not be freed and is valid until GDALGroup is
 * released or this function called again.
 *
 * This is the same as the C function GDALGroupGetStructuralInfo().
 */
CSLConstList GDALGroup::GetStructuralInfo() const
{
    return nullptr;
}

/************************************************************************/
/*                            CreateGroup()                             */
/************************************************************************/

/** Create a sub-group within a group.
 *
 * Optionally implemented by drivers.
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateGroup().
 *
 * @param osName Sub-group name.
 * @param papszOptions Driver specific options determining how the sub-group
 * should be created.
 *
 * @return the new sub-group, or nullptr in case of error.
 */
std::shared_ptr<GDALGroup>
GDALGroup::CreateGroup(CPL_UNUSED const std::string &osName,
                       CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateGroup() not implemented");
    return nullptr;
}

/************************************************************************/
/*                            DeleteGroup()                             */
/************************************************************************/

/** Delete a sub-group from a group.
 *
 * Optionally implemented.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * Drivers known to implement it: MEM, Zarr
 *
 * This is the same as the C function GDALGroupDeleteGroup().
 *
 * @param osName Sub-group name.
 * @param papszOptions Driver specific options determining how the group.
 * should be deleted.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALGroup::DeleteGroup(CPL_UNUSED const std::string &osName,
                            CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "DeleteGroup() not implemented");
    return false;
}

/************************************************************************/
/*                          CreateDimension()                           */
/************************************************************************/

/** Create a dimension within a group.
 *
 * @note Driver implementation: drivers supporting CreateDimension() should
 * implement this method, but do not have necessarily to implement
 * GDALGroup::GetDimensions().
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateDimension().
 *
 * @param osName Dimension name.
 * @param osType Dimension type (might be empty, and ignored by drivers)
 * @param osDirection Dimension direction (might be empty, and ignored by
 * drivers)
 * @param nSize  Number of values indexed by this dimension. Should be > 0.
 * @param papszOptions Driver specific options determining how the dimension
 * should be created.
 *
 * @return the new dimension, or nullptr if case of error
 */
std::shared_ptr<GDALDimension> GDALGroup::CreateDimension(
    CPL_UNUSED const std::string &osName, CPL_UNUSED const std::string &osType,
    CPL_UNUSED const std::string &osDirection, CPL_UNUSED GUInt64 nSize,
    CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateDimension() not implemented");
    return nullptr;
}

/************************************************************************/
/*                           CreateMDArray()                            */
/************************************************************************/

/** Create a multidimensional array within a group.
 *
 * It is recommended that the GDALDimension objects passed in aoDimensions
 * belong to this group, either by retrieving them with GetDimensions()
 * or creating a new one with CreateDimension().
 *
 * Optionally implemented.
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateMDArray().
 *
 * @note Driver implementation: drivers should take into account the possibility
 * that GDALDimension object passed in aoDimensions might belong to a different
 * group / dataset / driver and act accordingly.
 *
 * @param osName Array name.
 * @param aoDimensions List of dimensions, ordered from the slowest varying
 *                     dimension first to the fastest varying dimension last.
 *                     Might be empty for a scalar array (if supported by
 * driver)
 * @param oDataType  Array data type.
 * @param papszOptions Driver specific options determining how the array
 * should be created.
 *
 * @return the new array, or nullptr in case of error
 */
std::shared_ptr<GDALMDArray> GDALGroup::CreateMDArray(
    CPL_UNUSED const std::string &osName,
    CPL_UNUSED const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    CPL_UNUSED const GDALExtendedDataType &oDataType,
    CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateMDArray() not implemented");
    return nullptr;
}

/************************************************************************/
/*                           DeleteMDArray()                            */
/************************************************************************/

/** Delete an array from a group.
 *
 * Optionally implemented.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * Drivers known to implement it: MEM, Zarr
 *
 * This is the same as the C function GDALGroupDeleteMDArray().
 *
 * @param osName Arrayname.
 * @param papszOptions Driver specific options determining how the array.
 * should be deleted.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALGroup::DeleteMDArray(CPL_UNUSED const std::string &osName,
                              CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "DeleteMDArray() not implemented");
    return false;
}

/************************************************************************/
/*                          GetTotalCopyCost()                          */
/************************************************************************/

/** Return a total "cost" to copy the group.
 *
 * Used as a parameter for CopFrom()
 */
GUInt64 GDALGroup::GetTotalCopyCost() const
{
    GUInt64 nCost = COPY_COST;
    nCost += GetAttributes().size() * GDALAttribute::COPY_COST;

    auto groupNames = GetGroupNames();
    for (const auto &name : groupNames)
    {
        auto subGroup = OpenGroup(name);
        if (subGroup)
        {
            nCost += subGroup->GetTotalCopyCost();
        }
    }

    auto arrayNames = GetMDArrayNames();
    for (const auto &name : arrayNames)
    {
        auto array = OpenMDArray(name);
        if (array)
        {
            nCost += array->GetTotalCopyCost();
        }
    }
    return nCost;
}

/************************************************************************/
/*                              CopyFrom()                              */
/************************************************************************/

/** Copy the content of a group into a new (generally empty) group.
 *
 * @param poDstRootGroup Destination root group. Must NOT be nullptr.
 * @param poSrcDS    Source dataset. Might be nullptr (but for correct behavior
 *                   of some output drivers this is not recommended)
 * @param poSrcGroup Source group. Must NOT be nullptr.
 * @param bStrict Whether to enable strict mode. In strict mode, any error will
 *                stop the copy. In relaxed mode, the copy will be attempted to
 *                be pursued.
 * @param nCurCost  Should be provided as a variable initially set to 0.
 * @param nTotalCost Total cost from GetTotalCopyCost().
 * @param pfnProgress Progress callback, or nullptr.
 * @param pProgressData Progress user data, or nullptr.
 * @param papszOptions Creation options. Currently, only array creation
 *                     options are supported. They must be prefixed with
 * "ARRAY:" . The scope may be further restricted to arrays of a certain
 *                     dimension by adding "IF(DIM={ndims}):" after "ARRAY:".
 *                     For example, "ARRAY:IF(DIM=2):BLOCKSIZE=256,256" will
 *                     restrict BLOCKSIZE=256,256 to arrays of dimension 2.
 *                     Restriction to arrays of a given name is done with adding
 *                     "IF(NAME={name}):" after "ARRAY:". {name} can also be
 *                     a full qualified name.
 *                     A non-driver specific ARRAY option, "AUTOSCALE=YES" can
 * be used to ask (non indexing) variables of type Float32 or Float64 to be
 * scaled to UInt16 with scale and offset values being computed from the minimum
 * and maximum of the source array. The integer data type used can be set with
 *                     AUTOSCALE_DATA_TYPE=Byte/UInt16/Int16/UInt32/Int32.
 *
 * @return true in case of success (or partial success if bStrict == false).
 */
bool GDALGroup::CopyFrom(const std::shared_ptr<GDALGroup> &poDstRootGroup,
                         GDALDataset *poSrcDS,
                         const std::shared_ptr<GDALGroup> &poSrcGroup,
                         bool bStrict, GUInt64 &nCurCost,
                         const GUInt64 nTotalCost, GDALProgressFunc pfnProgress,
                         void *pProgressData, CSLConstList papszOptions)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

#define EXIT_OR_CONTINUE_IF_NULL(x)                                            \
    if (!(x))                                                                  \
    {                                                                          \
        if (bStrict)                                                           \
            return false;                                                      \
        continue;                                                              \
    }                                                                          \
    (void)0

    try
    {
        nCurCost += GDALGroup::COPY_COST;

        const auto srcDims = poSrcGroup->GetDimensions();
        std::map<std::string, std::shared_ptr<GDALDimension>>
            mapExistingDstDims;
        std::map<std::string, std::string> mapSrcVariableNameToIndexedDimName;
        for (const auto &dim : srcDims)
        {
            auto dstDim = CreateDimension(dim->GetName(), dim->GetType(),
                                          dim->GetDirection(), dim->GetSize());
            EXIT_OR_CONTINUE_IF_NULL(dstDim);
            mapExistingDstDims[dim->GetName()] = std::move(dstDim);
            auto poIndexingVarSrc(dim->GetIndexingVariable());
            if (poIndexingVarSrc)
            {
                mapSrcVariableNameToIndexedDimName[poIndexingVarSrc
                                                       ->GetName()] =
                    dim->GetName();
            }
        }

        auto attrs = poSrcGroup->GetAttributes();
        for (const auto &attr : attrs)
        {
            auto dstAttr =
                CreateAttribute(attr->GetName(), attr->GetDimensionsSize(),
                                attr->GetDataType());
            EXIT_OR_CONTINUE_IF_NULL(dstAttr);
            auto raw(attr->ReadAsRaw());
            if (!dstAttr->Write(raw.data(), raw.size()) && bStrict)
                return false;
        }
        if (!attrs.empty())
        {
            nCurCost += attrs.size() * GDALAttribute::COPY_COST;
            if (!pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData))
                return false;
        }

        const auto CopyArray =
            [this, &poSrcDS, &poDstRootGroup, &mapExistingDstDims,
             &mapSrcVariableNameToIndexedDimName, pfnProgress, pProgressData,
             papszOptions, bStrict, &nCurCost,
             nTotalCost](const std::shared_ptr<GDALMDArray> &srcArray)
        {
            // Map source dimensions to target dimensions
            std::vector<std::shared_ptr<GDALDimension>> dstArrayDims;
            const auto &srcArrayDims(srcArray->GetDimensions());
            for (const auto &dim : srcArrayDims)
            {
                auto dstDim = poDstRootGroup->OpenDimensionFromFullname(
                    dim->GetFullName());
                if (dstDim && dstDim->GetSize() == dim->GetSize())
                {
                    dstArrayDims.emplace_back(dstDim);
                }
                else
                {
                    auto oIter = mapExistingDstDims.find(dim->GetName());
                    if (oIter != mapExistingDstDims.end() &&
                        oIter->second->GetSize() == dim->GetSize())
                    {
                        dstArrayDims.emplace_back(oIter->second);
                    }
                    else
                    {
                        std::string newDimName;
                        if (oIter == mapExistingDstDims.end())
                        {
                            newDimName = dim->GetName();
                        }
                        else
                        {
                            std::string newDimNamePrefix(srcArray->GetName() +
                                                         '_' + dim->GetName());
                            newDimName = newDimNamePrefix;
                            int nIterCount = 2;
                            while (
                                cpl::contains(mapExistingDstDims, newDimName))
                            {
                                newDimName = newDimNamePrefix +
                                             CPLSPrintf("_%d", nIterCount);
                                nIterCount++;
                            }
                        }
                        dstDim = CreateDimension(newDimName, dim->GetType(),
                                                 dim->GetDirection(),
                                                 dim->GetSize());
                        if (!dstDim)
                            return false;
                        mapExistingDstDims[newDimName] = dstDim;
                        dstArrayDims.emplace_back(dstDim);
                    }
                }
            }

            CPLStringList aosArrayCO;
            bool bAutoScale = false;
            GDALDataType eAutoScaleType = GDT_UInt16;
            for (const char *pszItem : cpl::Iterate(papszOptions))
            {
                if (STARTS_WITH_CI(pszItem, "ARRAY:"))
                {
                    const char *pszOption = pszItem + strlen("ARRAY:");
                    if (STARTS_WITH_CI(pszOption, "IF(DIM="))
                    {
                        const char *pszNext = strchr(pszOption, ':');
                        if (pszNext != nullptr)
                        {
                            int nDim = atoi(pszOption + strlen("IF(DIM="));
                            if (static_cast<size_t>(nDim) ==
                                dstArrayDims.size())
                            {
                                pszOption = pszNext + 1;
                            }
                            else
                            {
                                pszOption = nullptr;
                            }
                        }
                    }
                    else if (STARTS_WITH_CI(pszOption, "IF(NAME="))
                    {
                        const char *pszName = pszOption + strlen("IF(NAME=");
                        const char *pszNext = strchr(pszName, ':');
                        if (pszNext != nullptr && pszNext > pszName &&
                            pszNext[-1] == ')')
                        {
                            CPLString osName;
                            osName.assign(pszName, pszNext - pszName - 1);
                            if (osName == srcArray->GetName() ||
                                osName == srcArray->GetFullName())
                            {
                                pszOption = pszNext + 1;
                            }
                            else
                            {
                                pszOption = nullptr;
                            }
                        }
                    }
                    if (pszOption)
                    {
                        if (STARTS_WITH_CI(pszOption, "AUTOSCALE="))
                        {
                            bAutoScale =
                                CPLTestBool(pszOption + strlen("AUTOSCALE="));
                        }
                        else if (STARTS_WITH_CI(pszOption,
                                                "AUTOSCALE_DATA_TYPE="))
                        {
                            const char *pszDataType =
                                pszOption + strlen("AUTOSCALE_DATA_TYPE=");
                            eAutoScaleType = GDALGetDataTypeByName(pszDataType);
                            if (GDALDataTypeIsComplex(eAutoScaleType) ||
                                GDALDataTypeIsFloating(eAutoScaleType))
                            {
                                CPLError(CE_Failure, CPLE_NotSupported,
                                         "Unsupported value for "
                                         "AUTOSCALE_DATA_TYPE");
                                return false;
                            }
                        }
                        else
                        {
                            aosArrayCO.AddString(pszOption);
                        }
                    }
                }
            }

            if (aosArrayCO.FetchNameValue("BLOCKSIZE") == nullptr)
            {
                const auto anBlockSize = srcArray->GetBlockSize();
                std::string osBlockSize;
                for (auto v : anBlockSize)
                {
                    if (v == 0)
                    {
                        osBlockSize.clear();
                        break;
                    }
                    if (!osBlockSize.empty())
                        osBlockSize += ',';
                    osBlockSize += std::to_string(v);
                }
                if (!osBlockSize.empty())
                    aosArrayCO.SetNameValue("BLOCKSIZE", osBlockSize.c_str());
            }

            auto oIterDimName =
                mapSrcVariableNameToIndexedDimName.find(srcArray->GetName());
            const auto &srcArrayType = srcArray->GetDataType();

            std::shared_ptr<GDALMDArray> dstArray;

            // Only autoscale non-indexing variables
            bool bHasOffset = false;
            bool bHasScale = false;
            if (bAutoScale && srcArrayType.GetClass() == GEDTC_NUMERIC &&
                (srcArrayType.GetNumericDataType() == GDT_Float16 ||
                 srcArrayType.GetNumericDataType() == GDT_Float32 ||
                 srcArrayType.GetNumericDataType() == GDT_Float64) &&
                srcArray->GetOffset(&bHasOffset) == 0.0 && !bHasOffset &&
                srcArray->GetScale(&bHasScale) == 1.0 && !bHasScale &&
                oIterDimName == mapSrcVariableNameToIndexedDimName.end())
            {
                constexpr bool bApproxOK = false;
                constexpr bool bForce = true;
                double dfMin = 0.0;
                double dfMax = 0.0;
                if (srcArray->GetStatistics(bApproxOK, bForce, &dfMin, &dfMax,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr) != CE_None)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Could not retrieve statistics for array %s",
                             srcArray->GetName().c_str());
                    return false;
                }
                double dfDTMin = 0;
                double dfDTMax = 0;
#define setDTMinMax(ctype)                                                     \
    do                                                                         \
    {                                                                          \
        dfDTMin = static_cast<double>(cpl::NumericLimits<ctype>::lowest());    \
        dfDTMax = static_cast<double>(cpl::NumericLimits<ctype>::max());       \
    } while (0)

                switch (eAutoScaleType)
                {
                    case GDT_UInt8:
                        setDTMinMax(GByte);
                        break;
                    case GDT_Int8:
                        setDTMinMax(GInt8);
                        break;
                    case GDT_UInt16:
                        setDTMinMax(GUInt16);
                        break;
                    case GDT_Int16:
                        setDTMinMax(GInt16);
                        break;
                    case GDT_UInt32:
                        setDTMinMax(GUInt32);
                        break;
                    case GDT_Int32:
                        setDTMinMax(GInt32);
                        break;
                    case GDT_UInt64:
                        setDTMinMax(std::uint64_t);
                        break;
                    case GDT_Int64:
                        setDTMinMax(std::int64_t);
                        break;
                    case GDT_Float16:
                    case GDT_Float32:
                    case GDT_Float64:
                    case GDT_Unknown:
                    case GDT_CInt16:
                    case GDT_CInt32:
                    case GDT_CFloat16:
                    case GDT_CFloat32:
                    case GDT_CFloat64:
                    case GDT_TypeCount:
                        CPLAssert(false);
                }

                dstArray =
                    CreateMDArray(srcArray->GetName(), dstArrayDims,
                                  GDALExtendedDataType::Create(eAutoScaleType),
                                  aosArrayCO.List());
                if (!dstArray)
                    return !bStrict;

                if (srcArray->GetRawNoDataValue() != nullptr)
                {
                    // If there's a nodata value in the source array, reserve
                    // DTMax for that purpose in the target scaled array
                    if (!dstArray->SetNoDataValue(dfDTMax))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot set nodata value");
                        return false;
                    }
                    dfDTMax--;
                }
                const double dfScale =
                    dfMax > dfMin ? (dfMax - dfMin) / (dfDTMax - dfDTMin) : 1.0;
                const double dfOffset = dfMin - dfDTMin * dfScale;

                if (!dstArray->SetOffset(dfOffset) ||
                    !dstArray->SetScale(dfScale))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot set scale/offset");
                    return false;
                }

                auto poUnscaled = dstArray->GetUnscaled();
                if (srcArray->GetRawNoDataValue() != nullptr)
                {
                    poUnscaled->SetNoDataValue(
                        srcArray->GetNoDataValueAsDouble());
                }

                // Copy source array into unscaled array
                if (!poUnscaled->CopyFrom(poSrcDS, srcArray.get(), bStrict,
                                          nCurCost, nTotalCost, pfnProgress,
                                          pProgressData))
                {
                    return false;
                }
            }
            else
            {
                dstArray = CreateMDArray(srcArray->GetName(), dstArrayDims,
                                         srcArrayType, aosArrayCO.List());
                if (!dstArray)
                    return !bStrict;

                if (!dstArray->CopyFrom(poSrcDS, srcArray.get(), bStrict,
                                        nCurCost, nTotalCost, pfnProgress,
                                        pProgressData))
                {
                    return false;
                }
            }

            // If this array is the indexing variable of a dimension, link them
            // together.
            if (oIterDimName != mapSrcVariableNameToIndexedDimName.end())
            {
                auto oCorrespondingDimIter =
                    mapExistingDstDims.find(oIterDimName->second);
                if (oCorrespondingDimIter != mapExistingDstDims.end())
                {
                    CPLErrorStateBackuper oErrorStateBackuper(
                        CPLQuietErrorHandler);
                    oCorrespondingDimIter->second->SetIndexingVariable(
                        std::move(dstArray));
                }
            }

            return true;
        };

        const auto arrayNames = poSrcGroup->GetMDArrayNames();

        // Start by copying arrays that are indexing variables of dimensions
        for (const auto &name : arrayNames)
        {
            auto srcArray = poSrcGroup->OpenMDArray(name);
            EXIT_OR_CONTINUE_IF_NULL(srcArray);

            if (cpl::contains(mapSrcVariableNameToIndexedDimName,
                              srcArray->GetName()) &&
                !OpenMDArray(name))
            {
                if (!CopyArray(srcArray))
                    return false;
            }
        }

        // Then copy regular arrays
        for (const auto &name : arrayNames)
        {
            auto srcArray = poSrcGroup->OpenMDArray(name);
            EXIT_OR_CONTINUE_IF_NULL(srcArray);

            if (!cpl::contains(mapSrcVariableNameToIndexedDimName,
                               srcArray->GetName()) &&
                !OpenMDArray(name))
            {
                if (!CopyArray(srcArray))
                    return false;
            }
        }

        const auto groupNames = poSrcGroup->GetGroupNames();
        for (const auto &name : groupNames)
        {
            auto srcSubGroup = poSrcGroup->OpenGroup(name);
            EXIT_OR_CONTINUE_IF_NULL(srcSubGroup);
            auto dstSubGroup = CreateGroup(name);
            EXIT_OR_CONTINUE_IF_NULL(dstSubGroup);
            if (!dstSubGroup->CopyFrom(
                    poDstRootGroup, poSrcDS, srcSubGroup, bStrict, nCurCost,
                    nTotalCost, pfnProgress, pProgressData, papszOptions))
                return false;
        }

        if (!pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData))
            return false;

        return true;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return false;
    }
}

/************************************************************************/
/*                         GetInnerMostGroup()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
const GDALGroup *
GDALGroup::GetInnerMostGroup(const std::string &osPathOrArrayOrDim,
                             std::shared_ptr<GDALGroup> &curGroupHolder,
                             std::string &osLastPart) const
{
    if (osPathOrArrayOrDim.empty() || osPathOrArrayOrDim[0] != '/')
        return nullptr;
    const GDALGroup *poCurGroup = this;
    CPLStringList aosTokens(
        CSLTokenizeString2(osPathOrArrayOrDim.c_str(), "/", 0));
    if (aosTokens.size() == 0)
    {
        // "/" case: the root group itself is the innermost group
        osLastPart.clear();
        return poCurGroup;
    }

    for (int i = 0; i < aosTokens.size() - 1; i++)
    {
        curGroupHolder = poCurGroup->OpenGroup(aosTokens[i], nullptr);
        if (!curGroupHolder)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find group %s",
                     aosTokens[i]);
            return nullptr;
        }
        poCurGroup = curGroupHolder.get();
    }
    osLastPart = aosTokens[aosTokens.size() - 1];
    return poCurGroup;
}

//! @endcond

/************************************************************************/
/*                      OpenMDArrayFromFullname()                       */
/************************************************************************/

/** Get an array from its fully qualified name */
std::shared_ptr<GDALMDArray>
GDALGroup::OpenMDArrayFromFullname(const std::string &osFullName,
                                   CSLConstList papszOptions) const
{
    std::string osName;
    std::shared_ptr<GDALGroup> curGroupHolder;
    auto poGroup(GetInnerMostGroup(osFullName, curGroupHolder, osName));
    if (poGroup == nullptr)
        return nullptr;
    return poGroup->OpenMDArray(osName, papszOptions);
}

/************************************************************************/
/*                     OpenAttributeFromFullname()                      */
/************************************************************************/

/** Get an attribute from its fully qualified name */
std::shared_ptr<GDALAttribute>
GDALGroup::OpenAttributeFromFullname(const std::string &osFullName,
                                     CSLConstList papszOptions) const
{
    const auto pos = osFullName.rfind('/');
    if (pos == std::string::npos)
        return nullptr;
    const std::string attrName = osFullName.substr(pos + 1);
    if (pos == 0)
        return GetAttribute(attrName);
    const std::string container = osFullName.substr(0, pos);
    auto poArray = OpenMDArrayFromFullname(container, papszOptions);
    if (poArray)
        return poArray->GetAttribute(attrName);
    auto poGroup = OpenGroupFromFullname(container, papszOptions);
    if (poGroup)
        return poGroup->GetAttribute(attrName);
    return nullptr;
}

/************************************************************************/
/*                           ResolveMDArray()                           */
/************************************************************************/

/** Locate an array in a group and its subgroups by name.
 *
 * If osName is a fully qualified name, then OpenMDArrayFromFullname() is first
 * used
 * Otherwise the search will start from the group identified by osStartingPath,
 * and an array whose name is osName will be looked for in this group (if
 * osStartingPath is empty or "/", then the current group is used). If there
 * is no match, then a recursive descendant search will be made in its
 * subgroups. If there is no match in the subgroups, then the parent (if
 * existing) of the group pointed by osStartingPath will be used as the new
 * starting point for the search.
 *
 * @param osName name, qualified or not
 * @param osStartingPath fully qualified name of the (sub-)group from which
 *                       the search should be started. If this is a non-empty
 *                       string, the group on which this method is called should
 *                       nominally be the root group (otherwise the path will
 *                       be interpreted as from the current group)
 * @param papszOptions options to pass to OpenMDArray()
 * @since GDAL 3.2
 */
std::shared_ptr<GDALMDArray>
GDALGroup::ResolveMDArray(const std::string &osName,
                          const std::string &osStartingPath,
                          CSLConstList papszOptions) const
{
    if (!osName.empty() && osName[0] == '/')
    {
        auto poArray = OpenMDArrayFromFullname(osName, papszOptions);
        if (poArray)
            return poArray;
    }
    std::string osPath(osStartingPath);
    std::set<std::string> oSetAlreadyVisited;

    while (true)
    {
        std::shared_ptr<GDALGroup> curGroupHolder;
        std::shared_ptr<GDALGroup> poGroup;

        std::queue<std::shared_ptr<GDALGroup>> oQueue;
        bool goOn = false;
        if (osPath.empty() || osPath == "/")
        {
            goOn = true;
        }
        else
        {
            std::string osLastPart;
            const GDALGroup *poGroupPtr =
                GetInnerMostGroup(osPath, curGroupHolder, osLastPart);
            if (poGroupPtr)
                poGroup = poGroupPtr->OpenGroup(osLastPart);
            if (poGroup &&
                !cpl::contains(oSetAlreadyVisited, poGroup->GetFullName()))
            {
                oQueue.push(poGroup);
                goOn = true;
            }
        }

        if (goOn)
        {
            do
            {
                const GDALGroup *groupPtr;
                if (!oQueue.empty())
                {
                    poGroup = oQueue.front();
                    oQueue.pop();
                    groupPtr = poGroup.get();
                }
                else
                {
                    groupPtr = this;
                }

                auto poArray = groupPtr->OpenMDArray(osName, papszOptions);
                if (poArray)
                    return poArray;

                const auto aosGroupNames = groupPtr->GetGroupNames();
                for (const auto &osGroupName : aosGroupNames)
                {
                    auto poSubGroup = groupPtr->OpenGroup(osGroupName);
                    if (poSubGroup && !cpl::contains(oSetAlreadyVisited,
                                                     poSubGroup->GetFullName()))
                    {
                        oQueue.push(poSubGroup);
                        oSetAlreadyVisited.insert(poSubGroup->GetFullName());
                    }
                }
            } while (!oQueue.empty());
        }

        if (osPath.empty() || osPath == "/")
            break;

        const auto nPos = osPath.rfind('/');
        if (nPos == 0)
            osPath = "/";
        else
        {
            if (nPos == std::string::npos)
                break;
            osPath.resize(nPos);
        }
    }
    return nullptr;
}

/************************************************************************/
/*                       OpenGroupFromFullname()                        */
/************************************************************************/

/** Get a group from its fully qualified name.
 * @since GDAL 3.2
 */
std::shared_ptr<GDALGroup>
GDALGroup::OpenGroupFromFullname(const std::string &osFullName,
                                 CSLConstList papszOptions) const
{
    std::string osName;
    std::shared_ptr<GDALGroup> curGroupHolder;
    auto poGroup(GetInnerMostGroup(osFullName, curGroupHolder, osName));
    if (poGroup == nullptr)
        return nullptr;
    if (osName.empty())
        return m_pSelf.lock();
    return poGroup->OpenGroup(osName, papszOptions);
}

/************************************************************************/
/*                     OpenDimensionFromFullname()                      */
/************************************************************************/

/** Get a dimension from its fully qualified name */
std::shared_ptr<GDALDimension>
GDALGroup::OpenDimensionFromFullname(const std::string &osFullName) const
{
    std::string osName;
    std::shared_ptr<GDALGroup> curGroupHolder;
    auto poGroup(GetInnerMostGroup(osFullName, curGroupHolder, osName));
    if (poGroup == nullptr)
        return nullptr;
    auto dims(poGroup->GetDimensions());
    for (auto &dim : dims)
    {
        if (dim->GetName() == osName)
            return dim;
    }
    return nullptr;
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

/**
 * \brief Clear statistics.
 *
 * @since GDAL 3.4
 */
void GDALGroup::ClearStatistics()
{
    auto groupNames = GetGroupNames();
    for (const auto &name : groupNames)
    {
        auto subGroup = OpenGroup(name);
        if (subGroup)
        {
            subGroup->ClearStatistics();
        }
    }

    auto arrayNames = GetMDArrayNames();
    for (const auto &name : arrayNames)
    {
        auto array = OpenMDArray(name);
        if (array)
        {
            array->ClearStatistics();
        }
    }
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

/** Rename the group.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF, ZARR.
 *
 * This is the same as the C function GDALGroupRename().
 *
 * @param osNewName New name.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALGroup::Rename(CPL_UNUSED const std::string &osNewName)
{
    CPLError(CE_Failure, CPLE_NotSupported, "Rename() not implemented");
    return false;
}

/************************************************************************/
/*                             BaseRename()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALGroup::BaseRename(const std::string &osNewName)
{
    m_osFullName.resize(m_osFullName.size() - m_osName.size());
    m_osFullName += osNewName;
    m_osName = osNewName;

    NotifyChildrenOfRenaming();
}

//! @endcond

/************************************************************************/
/*                           ParentRenamed()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALGroup::ParentRenamed(const std::string &osNewParentFullName)
{
    m_osFullName = osNewParentFullName;
    m_osFullName += "/";
    m_osFullName += m_osName;

    NotifyChildrenOfRenaming();
}

//! @endcond

/************************************************************************/
/*                              Deleted()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALGroup::Deleted()
{
    m_bValid = false;

    NotifyChildrenOfDeletion();
}

//! @endcond

/************************************************************************/
/*                           ParentDeleted()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALGroup::ParentDeleted()
{
    Deleted();
}

//! @endcond

/************************************************************************/
/*                     CheckValidAndErrorOutIfNot()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
bool GDALGroup::CheckValidAndErrorOutIfNot() const
{
    if (!m_bValid)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This object has been deleted. No action on it is possible");
    }
    return m_bValid;
}

//! @endcond
