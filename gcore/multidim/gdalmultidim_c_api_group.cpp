/******************************************************************************
 *
 * Name:     gdalmultidim_c_api_dimension.cpp
 * Project:  GDAL Core
 * Purpose:  C API for GDALGroup
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdalmultidim_priv.h"
#include "gdal_fwd.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                          GDALGroupRelease()                          */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALGroupH.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALGroupRelease(GDALGroupH hGroup)
{
    delete hGroup;
}

/************************************************************************/
/*                          GDALGroupGetName()                          */
/************************************************************************/

/** Return the name of the group.
 *
 * The returned pointer is valid until hGroup is released.
 *
 * This is the same as the C++ method GDALGroup::GetName().
 */
const char *GDALGroupGetName(GDALGroupH hGroup)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    return hGroup->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                        GDALGroupGetFullName()                        */
/************************************************************************/

/** Return the full name of the group.
 *
 * The returned pointer is valid until hGroup is released.
 *
 * This is the same as the C++ method GDALGroup::GetFullName().
 */
const char *GDALGroupGetFullName(GDALGroupH hGroup)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    return hGroup->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                      GDALGroupGetMDArrayNames()                      */
/************************************************************************/

/** Return the list of multidimensional array names contained in this group.
 *
 * This is the same as the C++ method GDALGroup::GetGroupNames().
 *
 * @return the array names, to be freed with CSLDestroy()
 */
char **GDALGroupGetMDArrayNames(GDALGroupH hGroup, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    auto names = hGroup->m_poImpl->GetMDArrayNames(papszOptions);
    CPLStringList res;
    for (const auto &name : names)
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*               GDALGroupGetMDArrayFullNamesRecursive()                */
/************************************************************************/

/** Return the list of multidimensional array full names contained in this
 * group and its subgroups.
 *
 * This is the same as the C++ method GDALGroup::GetMDArrayFullNamesRecursive().
 *
 * @return the array names, to be freed with CSLDestroy()
 *
 * @since 3.11
 */
char **GDALGroupGetMDArrayFullNamesRecursive(GDALGroupH hGroup,
                                             CSLConstList papszGroupOptions,
                                             CSLConstList papszArrayOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    auto names = hGroup->m_poImpl->GetMDArrayFullNamesRecursive(
        papszGroupOptions, papszArrayOptions);
    CPLStringList res;
    for (const auto &name : names)
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*                        GDALGroupOpenMDArray()                        */
/************************************************************************/

/** Open and return a multidimensional array.
 *
 * This is the same as the C++ method GDALGroup::OpenMDArray().
 *
 * @return the array, to be freed with GDALMDArrayRelease(), or nullptr.
 */
GDALMDArrayH GDALGroupOpenMDArray(GDALGroupH hGroup, const char *pszMDArrayName,
                                  CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszMDArrayName, __func__, nullptr);
    auto array = hGroup->m_poImpl->OpenMDArray(std::string(pszMDArrayName),
                                               papszOptions);
    if (!array)
        return nullptr;
    return new GDALMDArrayHS(array);
}

/************************************************************************/
/*                  GDALGroupOpenMDArrayFromFullname()                  */
/************************************************************************/

/** Open and return a multidimensional array from its fully qualified name.
 *
 * This is the same as the C++ method GDALGroup::OpenMDArrayFromFullname().
 *
 * @return the array, to be freed with GDALMDArrayRelease(), or nullptr.
 *
 * @since GDAL 3.2
 */
GDALMDArrayH GDALGroupOpenMDArrayFromFullname(GDALGroupH hGroup,
                                              const char *pszFullname,
                                              CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszFullname, __func__, nullptr);
    auto array = hGroup->m_poImpl->OpenMDArrayFromFullname(
        std::string(pszFullname), papszOptions);
    if (!array)
        return nullptr;
    return new GDALMDArrayHS(array);
}

/************************************************************************/
/*                      GDALGroupResolveMDArray()                       */
/************************************************************************/

/** Locate an array in a group and its subgroups by name.
 *
 * See GDALGroup::ResolveMDArray() for description of the behavior.
 * @since GDAL 3.2
 */
GDALMDArrayH GDALGroupResolveMDArray(GDALGroupH hGroup, const char *pszName,
                                     const char *pszStartingPoint,
                                     CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(pszStartingPoint, __func__, nullptr);
    auto array = hGroup->m_poImpl->ResolveMDArray(
        std::string(pszName), std::string(pszStartingPoint), papszOptions);
    if (!array)
        return nullptr;
    return new GDALMDArrayHS(array);
}

/************************************************************************/
/*                       GDALGroupGetGroupNames()                       */
/************************************************************************/

/** Return the list of sub-groups contained in this group.
 *
 * This is the same as the C++ method GDALGroup::GetGroupNames().
 *
 * @return the group names, to be freed with CSLDestroy()
 */
char **GDALGroupGetGroupNames(GDALGroupH hGroup, CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    auto names = hGroup->m_poImpl->GetGroupNames(papszOptions);
    CPLStringList res;
    for (const auto &name : names)
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*                         GDALGroupOpenGroup()                         */
/************************************************************************/

/** Open and return a sub-group.
 *
 * This is the same as the C++ method GDALGroup::OpenGroup().
 *
 * @return the sub-group, to be freed with GDALGroupRelease(), or nullptr.
 */
GDALGroupH GDALGroupOpenGroup(GDALGroupH hGroup, const char *pszSubGroupName,
                              CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszSubGroupName, __func__, nullptr);
    auto subGroup =
        hGroup->m_poImpl->OpenGroup(std::string(pszSubGroupName), papszOptions);
    if (!subGroup)
        return nullptr;
    return new GDALGroupHS(subGroup);
}

/************************************************************************/
/*                    GDALGroupGetVectorLayerNames()                    */
/************************************************************************/

/** Return the list of layer names contained in this group.
 *
 * This is the same as the C++ method GDALGroup::GetVectorLayerNames().
 *
 * @return the group names, to be freed with CSLDestroy()
 * @since 3.4
 */
char **GDALGroupGetVectorLayerNames(GDALGroupH hGroup,
                                    CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    auto names = hGroup->m_poImpl->GetVectorLayerNames(papszOptions);
    CPLStringList res;
    for (const auto &name : names)
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*                      GDALGroupOpenVectorLayer()                      */
/************************************************************************/

/** Open and return a vector layer.
 *
 * This is the same as the C++ method GDALGroup::OpenVectorLayer().
 *
 * Note that the vector layer is owned by its parent GDALDatasetH, and thus
 * the returned handled if only valid while the parent GDALDatasetH is kept
 * opened.
 *
 * @return the vector layer, or nullptr.
 * @since 3.4
 */
OGRLayerH GDALGroupOpenVectorLayer(GDALGroupH hGroup,
                                   const char *pszVectorLayerName,
                                   CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszVectorLayerName, __func__, nullptr);
    return OGRLayer::ToHandle(hGroup->m_poImpl->OpenVectorLayer(
        std::string(pszVectorLayerName), papszOptions));
}

/************************************************************************/
/*                  GDALGroupOpenMDArrayFromFullname()                  */
/************************************************************************/

/** Open and return a sub-group from its fully qualified name.
 *
 * This is the same as the C++ method GDALGroup::OpenGroupFromFullname().
 *
 * @return the sub-group, to be freed with GDALGroupRelease(), or nullptr.
 *
 * @since GDAL 3.2
 */
GDALGroupH GDALGroupOpenGroupFromFullname(GDALGroupH hGroup,
                                          const char *pszFullname,
                                          CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszFullname, __func__, nullptr);
    auto subGroup = hGroup->m_poImpl->OpenGroupFromFullname(
        std::string(pszFullname), papszOptions);
    if (!subGroup)
        return nullptr;
    return new GDALGroupHS(subGroup);
}

/************************************************************************/
/*                       GDALGroupGetDimensions()                       */
/************************************************************************/

/** Return the list of dimensions contained in this group and used by its
 * arrays.
 *
 * The returned array must be freed with GDALReleaseDimensions().  If only the
 * array itself needs to be freed, CPLFree() should be called (and
 * GDALDimensionRelease() on individual array members).
 *
 * This is the same as the C++ method GDALGroup::GetDimensions().
 *
 * @param hGroup Group.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how dimensions
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return an array of *pnCount dimensions.
 */
GDALDimensionH *GDALGroupGetDimensions(GDALGroupH hGroup, size_t *pnCount,
                                       CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    auto dims = hGroup->m_poImpl->GetDimensions(papszOptions);
    auto ret = static_cast<GDALDimensionH *>(
        CPLMalloc(sizeof(GDALDimensionH) * dims.size()));
    for (size_t i = 0; i < dims.size(); i++)
    {
        ret[i] = new GDALDimensionHS(dims[i]);
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                       GDALGroupGetAttribute()                        */
/************************************************************************/

/** Return an attribute by its name.
 *
 * This is the same as the C++ method GDALIHasAttribute::GetAttribute()
 *
 * The returned attribute must be freed with GDALAttributeRelease().
 */
GDALAttributeH GDALGroupGetAttribute(GDALGroupH hGroup, const char *pszName)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    auto attr = hGroup->m_poImpl->GetAttribute(std::string(pszName));
    if (attr)
        return new GDALAttributeHS(attr);
    return nullptr;
}

/************************************************************************/
/*                       GDALGroupGetAttributes()                       */
/************************************************************************/

/** Return the list of attributes contained in this group.
 *
 * The returned array must be freed with GDALReleaseAttributes(). If only the
 * array itself needs to be freed, CPLFree() should be called (and
 * GDALAttributeRelease() on individual array members).
 *
 * This is the same as the C++ method GDALGroup::GetAttributes().
 *
 * @param hGroup Group.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behavior.
 *
 * @return an array of *pnCount attributes.
 */
GDALAttributeH *GDALGroupGetAttributes(GDALGroupH hGroup, size_t *pnCount,
                                       CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    auto attrs = hGroup->m_poImpl->GetAttributes(papszOptions);
    auto ret = static_cast<GDALAttributeH *>(
        CPLMalloc(sizeof(GDALAttributeH) * attrs.size()));
    for (size_t i = 0; i < attrs.size(); i++)
    {
        ret[i] = new GDALAttributeHS(attrs[i]);
    }
    *pnCount = attrs.size();
    return ret;
}

/************************************************************************/
/*                     GDALGroupGetStructuralInfo()                     */
/************************************************************************/

/** Return structural information on the group.
 *
 * This may be the compression, etc..
 *
 * The return value should not be freed and is valid until GDALGroup is
 * released or this function called again.
 *
 * This is the same as the C++ method GDALGroup::GetStructuralInfo().
 */
CSLConstList GDALGroupGetStructuralInfo(GDALGroupH hGroup)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    return hGroup->m_poImpl->GetStructuralInfo();
}

/************************************************************************/
/*                     GDALGroupGetDataTypeCount()                      */
/************************************************************************/

/** Return the number of data types associated with the group
 * (typically enumerations).
 *
 * This is the same as the C++ method GDALGroup::GetDataTypes().size().
 *
 * @since 3.12
 */
size_t GDALGroupGetDataTypeCount(GDALGroupH hGroup)
{
    VALIDATE_POINTER1(hGroup, __func__, 0);
    return hGroup->m_poImpl->GetDataTypes().size();
}

/************************************************************************/
/*                        GDALGroupGetDataType()                        */
/************************************************************************/

/** Return one of the data types associated with the group.
 *
 * This is the same as the C++ method GDALGroup::GetDataTypes()[].
 *
 * @return a type to release with GDALExtendedDataTypeRelease() once done,
 * or nullptr in case of error.
 * @since 3.12
 */
GDALExtendedDataTypeH GDALGroupGetDataType(GDALGroupH hGroup, size_t nIdx)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    if (nIdx >= hGroup->m_poImpl->GetDataTypes().size())
        return nullptr;
    return new GDALExtendedDataTypeHS(new GDALExtendedDataType(
        *(hGroup->m_poImpl->GetDataTypes()[nIdx].get())));
}

/************************************************************************/
/*                        GDALGroupCreateGroup()                        */
/************************************************************************/

/** Create a sub-group within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateGroup().
 *
 * @return the sub-group, to be freed with GDALGroupRelease(), or nullptr.
 */
GDALGroupH GDALGroupCreateGroup(GDALGroupH hGroup, const char *pszSubGroupName,
                                CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszSubGroupName, __func__, nullptr);
    auto ret = hGroup->m_poImpl->CreateGroup(std::string(pszSubGroupName),
                                             papszOptions);
    if (!ret)
        return nullptr;
    return new GDALGroupHS(ret);
}

/************************************************************************/
/*                        GDALGroupDeleteGroup()                        */
/************************************************************************/

/** Delete a sub-group from a group.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * This is the same as the C++ method GDALGroup::DeleteGroup().
 *
 * @return true in case of success.
 * @since GDAL 3.8
 */
bool GDALGroupDeleteGroup(GDALGroupH hGroup, const char *pszSubGroupName,
                          CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, false);
    VALIDATE_POINTER1(pszSubGroupName, __func__, false);
    return hGroup->m_poImpl->DeleteGroup(std::string(pszSubGroupName),
                                         papszOptions);
}

/************************************************************************/
/*                      GDALGroupCreateDimension()                      */
/************************************************************************/

/** Create a dimension within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateDimension().
 *
 * @return the dimension, to be freed with GDALDimensionRelease(), or nullptr.
 */
GDALDimensionH GDALGroupCreateDimension(GDALGroupH hGroup, const char *pszName,
                                        const char *pszType,
                                        const char *pszDirection, GUInt64 nSize,
                                        CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    auto ret = hGroup->m_poImpl->CreateDimension(
        std::string(pszName), std::string(pszType ? pszType : ""),
        std::string(pszDirection ? pszDirection : ""), nSize, papszOptions);
    if (!ret)
        return nullptr;
    return new GDALDimensionHS(ret);
}

/************************************************************************/
/*                       GDALGroupCreateMDArray()                       */
/************************************************************************/

/** Create a multidimensional array within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateMDArray().
 *
 * @return the array, to be freed with GDALMDArrayRelease(), or nullptr.
 */
GDALMDArrayH GDALGroupCreateMDArray(GDALGroupH hGroup, const char *pszName,
                                    size_t nDimensions,
                                    GDALDimensionH *pahDimensions,
                                    GDALExtendedDataTypeH hEDT,
                                    CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(hEDT, __func__, nullptr);
    std::vector<std::shared_ptr<GDALDimension>> dims;
    dims.reserve(nDimensions);
    for (size_t i = 0; i < nDimensions; i++)
        dims.push_back(pahDimensions[i]->m_poImpl);
    auto ret = hGroup->m_poImpl->CreateMDArray(std::string(pszName), dims,
                                               *(hEDT->m_poImpl), papszOptions);
    if (!ret)
        return nullptr;
    return new GDALMDArrayHS(ret);
}

/************************************************************************/
/*                       GDALGroupDeleteMDArray()                       */
/************************************************************************/

/** Delete an array from a group.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * This is the same as the C++ method GDALGroup::DeleteMDArray().
 *
 * @return true in case of success.
 * @since GDAL 3.8
 */
bool GDALGroupDeleteMDArray(GDALGroupH hGroup, const char *pszName,
                            CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, false);
    VALIDATE_POINTER1(pszName, __func__, false);
    return hGroup->m_poImpl->DeleteMDArray(std::string(pszName), papszOptions);
}

/************************************************************************/
/*                      GDALGroupCreateAttribute()                      */
/************************************************************************/

/** Create a attribute within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateAttribute().
 *
 * @return the attribute, to be freed with GDALAttributeRelease(), or nullptr.
 */
GDALAttributeH GDALGroupCreateAttribute(GDALGroupH hGroup, const char *pszName,
                                        size_t nDimensions,
                                        const GUInt64 *panDimensions,
                                        GDALExtendedDataTypeH hEDT,
                                        CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(hEDT, __func__, nullptr);
    std::vector<GUInt64> dims;
    dims.reserve(nDimensions);
    for (size_t i = 0; i < nDimensions; i++)
        dims.push_back(panDimensions[i]);
    auto ret = hGroup->m_poImpl->CreateAttribute(
        std::string(pszName), dims, *(hEDT->m_poImpl), papszOptions);
    if (!ret)
        return nullptr;
    return new GDALAttributeHS(ret);
}

/************************************************************************/
/*                      GDALGroupDeleteAttribute()                      */
/************************************************************************/

/** Delete an attribute from a group.
 *
 * After this call, if a previously obtained instance of the deleted object
 * is still alive, no method other than for freeing it should be invoked.
 *
 * This is the same as the C++ method GDALGroup::DeleteAttribute().
 *
 * @return true in case of success.
 * @since GDAL 3.8
 */
bool GDALGroupDeleteAttribute(GDALGroupH hGroup, const char *pszName,
                              CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, false);
    VALIDATE_POINTER1(pszName, __func__, false);
    return hGroup->m_poImpl->DeleteAttribute(std::string(pszName),
                                             papszOptions);
}

/************************************************************************/
/*                          GDALGroupRename()                           */
/************************************************************************/

/** Rename the group.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF.
 *
 * This is the same as the C++ method GDALGroup::Rename()
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALGroupRename(GDALGroupH hGroup, const char *pszNewName)
{
    VALIDATE_POINTER1(hGroup, __func__, false);
    VALIDATE_POINTER1(pszNewName, __func__, false);
    return hGroup->m_poImpl->Rename(pszNewName);
}

/************************************************************************/
/*               GDALGroupSubsetDimensionFromSelection()                */
/************************************************************************/

/** Return a virtual group whose one dimension has been subset according to a
 * selection.
 *
 * This is the same as the C++ method GDALGroup::SubsetDimensionFromSelection().
 *
 * @return a virtual group, to be freed with GDALGroupRelease(), or nullptr.
 */
GDALGroupH
GDALGroupSubsetDimensionFromSelection(GDALGroupH hGroup,
                                      const char *pszSelection,
                                      CPL_UNUSED CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hGroup, __func__, nullptr);
    VALIDATE_POINTER1(pszSelection, __func__, nullptr);
    auto hNewGroup = hGroup->m_poImpl->SubsetDimensionFromSelection(
        std::string(pszSelection));
    if (!hNewGroup)
        return nullptr;
    return new GDALGroupHS(hNewGroup);
}
