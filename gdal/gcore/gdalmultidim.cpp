/******************************************************************************
 * $Id$
 *
 * Name:     gdalmultidim.cpp
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private implementation for multidimensional support
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

#include <assert.h>
#include <algorithm>

#include "gdal_priv.h"
#include "cpl_safemaths.hpp"

#if defined(__clang__) || defined(_MSC_VER)
#define COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT
#endif

/************************************************************************/
/*                         ~GDALIHasAttribute()                         */
/************************************************************************/

GDALIHasAttribute::~GDALIHasAttribute() = default;

/************************************************************************/
/*                            GetAttribute()                            */
/************************************************************************/

/** Return an attribute by its name.
 *
 * If the attribute does not exist, nullptr should be silently returned.
 *
 * @note Driver implementation: this method will fallback to
 * GetAttributeFromAttributes() is not explicitly implemented
 *
 * Drivers known to implement it for groups and arrays: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetAttribute() or
 * GDALMDArrayGetAttribute().
 *
 * @param osName Attribute name
 * @return the attribute, or nullptr if it does not exist or an error occured.
 */
std::shared_ptr<GDALAttribute> GDALIHasAttribute::GetAttribute(
                                const std::string& osName) const
{
    return GetAttributeFromAttributes(osName);
}

/************************************************************************/
/*                       GetAttributeFromAttributes()                   */
/************************************************************************/

/** Possible fallback implementation for GetAttribute() using GetAttributes().
 */
std::shared_ptr<GDALAttribute> GDALIHasAttribute::GetAttributeFromAttributes(
    const std::string& osName) const
{
    auto attrs(GetAttributes());
    for( const auto& attr: attrs )
    {
        if( attr->GetName() == osName )
            return attr;
    }
    return nullptr;
}


/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

/** Return the list of attributes contained in a GDALMDArray or GDALGroup.
 *
 * If the attribute does not exist, nullptr should be silently returned.
 *
 * @note Driver implementation: optionally implemented. If implemented,
 * GetAttribute() should also be implemented.
 *
 * Drivers known to implement it for groups and arrays: MEM, netCDF.
 *
 * This is the same as the C function GDALGroupGetAttributes() or
 * GDALMDArrayGetAttributes().

 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return the attributes.
 */
std::vector<std::shared_ptr<GDALAttribute>>
GDALIHasAttribute::GetAttributes(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
}

/************************************************************************/
/*                             CreateAttribute()                         */
/************************************************************************/

/** Create an attribute within a GDALMDArray or GDALGroup.
 *
 * The attribute might not be "physically" created until a value is written
 * into it.
 *
 * Optionally implemented.
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateAttribute() or
 * GDALMDArrayCreateAttribute()
 *
 * @param osName Attribute name.
 * @param anDimensions List of dimension sizes, ordered from the slowest varying
 *                     dimension first to the fastest varying dimension last.
 *                     Empty for a scalar attribute (common case)
 * @param oDataType  Attribute data type.
 * @param papszOptions Driver specific options determining how the attribute.
 * should be created.
 *
 * @return the new attribute, or nullptr if case of error
 */
std::shared_ptr<GDALAttribute> GDALIHasAttribute::CreateAttribute(
                                CPL_UNUSED const std::string& osName,
                                CPL_UNUSED const std::vector<GUInt64>& anDimensions,
                                CPL_UNUSED const GDALExtendedDataType& oDataType,
                                CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateAttribute() not implemented");
    return nullptr;
}

/************************************************************************/
/*                            GDALGroup()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALGroup::GDALGroup(const std::string& osParentName, const std::string& osName):
    m_osName(osParentName.empty() ? "/" : osName),
    m_osFullName(!osParentName.empty() ? ((osParentName == "/" ? "/" : osParentName + "/") + osName) : "/")
{}
//! @endcond

/************************************************************************/
/*                            ~GDALGroup()                              */
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
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return the array names.
 */
std::vector<std::string>
GDALGroup::GetMDArrayNames(CPL_UNUSED CSLConstList papszOptions) const
{
    return {};
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
 * be opened.  Pass nullptr for default behaviour.
 *
 * @return the array, or nullptr.
 */
std::shared_ptr<GDALMDArray> GDALGroup::OpenMDArray(CPL_UNUSED const std::string& osName,
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
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return the group names.
 */
std::vector<std::string> GDALGroup::GetGroupNames(CPL_UNUSED CSLConstList papszOptions) const
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
 * @param papszOptions Driver specific options determining how the sub-group should
 * be opened.  Pass nullptr for default behaviour.
 *
 * @return the group, or nullptr.
 */
std::shared_ptr<GDALGroup> GDALGroup::OpenGroup(CPL_UNUSED const std::string& osName,
                                                CPL_UNUSED CSLConstList papszOptions) const
{
    return nullptr;
}

/************************************************************************/
/*                             GetDimensions()                          */
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
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return the dimensions.
 */
std::vector<std::shared_ptr<GDALDimension>> GDALGroup::GetDimensions(
                                    CPL_UNUSED CSLConstList papszOptions) const
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
 * This is the same as the C function GDALGroupGetStruturalInfo().
 */
CSLConstList GDALGroup::GetStructuralInfo() const
{
    return nullptr;
}

/************************************************************************/
/*                              CreateGroup()                           */
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
std::shared_ptr<GDALGroup> GDALGroup::CreateGroup(CPL_UNUSED const std::string& osName,
                                                  CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateGroup() not implemented");
    return nullptr;
}

/************************************************************************/
/*                            CreateDimension()                         */
/************************************************************************/

/** Create a dimension within a group.
 *
 * @note Driver implementation: drivers supporting CreateDimension() should implement
 * this method, but do not have necessarily to implement GDALGroup::GetDimensions().
 *
 * Drivers known to implement it: MEM, netCDF
 *
 * This is the same as the C function GDALGroupCreateDimension().
 *
 * @param osName Dimension name.
 * @param osType Dimension type (might be empty, and ignored by drivers)
 * @param osDirection Dimension direction (might be empty, and ignored by drivers)
 * @param nSize  Number of values indexed by this dimension. Should be > 0.
 * @param papszOptions Driver specific options determining how the dimension
 * should be created.
 *
 * @return the new dimension, or nullptr if case of error
 */
std::shared_ptr<GDALDimension> GDALGroup::CreateDimension(CPL_UNUSED const std::string& osName,
                                                          CPL_UNUSED const std::string& osType,
                                                          CPL_UNUSED const std::string& osDirection,
                                                          CPL_UNUSED GUInt64 nSize,
                                                          CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateDimension() not implemented");
    return nullptr;
}

/************************************************************************/
/*                             CreateMDArray()                          */
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
 *                     Might be empty for a scalar array (if supported by driver)
 * @param oDataType  Array data type.
 * @param papszOptions Driver specific options determining how the array
 * should be created.
 *
 * @return the new array, or nullptr if case of error
 */
std::shared_ptr<GDALMDArray> GDALGroup::CreateMDArray(
    CPL_UNUSED const std::string& osName,
    CPL_UNUSED const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
    CPL_UNUSED const GDALExtendedDataType& oDataType,
    CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported, "CreateMDArray() not implemented");
    return nullptr;
}

/************************************************************************/
/*                           GetTotalCopyCost()                         */
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
    for( const auto& name: groupNames )
    {
        auto subGroup = OpenGroup(name);
        if( subGroup )
        {
            nCost += subGroup->GetTotalCopyCost();
        }
    }

    auto arrayNames = GetMDArrayNames();
    for( const auto& name: arrayNames )
    {
        auto array = OpenMDArray(name);
        if( array )
        {
            nCost += array->GetTotalCopyCost();
        }
    }
    return nCost;
}

/************************************************************************/
/*                               CopyFrom()                             */
/************************************************************************/

/** Copy the content of a group into a new (generally empty) group.
 *
 * @param poDstRootGroup Destination root group. Must NOT be nullptr.
 * @param poSrcDS    Source dataset. Migt be nullptr (but for correct behaviour
 *                   of some output drivers this is not recommended)
 * @param poSrcGroup Source group. Must NOT be nullptr.
 * @param bStrict Whether to enable stict mode. In strict mode, any error will
 *                stop the copy. In relaxed mode, the copy will be attempted to
 *                be pursued.
 * @param nCurCost  Should be provided as a variable initially set to 0.
 * @param nTotalCost Total cost from GetTotalCopyCost().
 * @param pfnProgress Progress callback, or nullptr.
 * @param pProgressData Progress user data, or nulptr.
 *
 * @return true in case of success (or partial success if bStrict == false).
 */
bool GDALGroup::CopyFrom( const std::shared_ptr<GDALGroup>& poDstRootGroup,
                          GDALDataset* poSrcDS,
                          const std::shared_ptr<GDALGroup>& poSrcGroup,
                          bool bStrict,
                          GUInt64& nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress,
                          void * pProgressData)
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

#define EXIT_OR_CONTINUE_IF_NULL(x) \
            if( !(x) ) \
            { \
                if( bStrict ) \
                    return false; \
                continue; \
            } \
            (void)0

    try
    {
        nCurCost += GDALGroup::COPY_COST;

        const auto srcDims = poSrcGroup->GetDimensions();
        std::map<std::string, std::shared_ptr<GDALDimension>> mapExistingDstDims;
        std::map<std::string, std::string> mapSrcVariableNameToIndexedDimName;
        for( const auto& dim: srcDims )
        {
            auto dstDim = CreateDimension(dim->GetName(),
                                                      dim->GetType(),
                                                      dim->GetDirection(),
                                                      dim->GetSize());
            EXIT_OR_CONTINUE_IF_NULL(dstDim);
            mapExistingDstDims[dim->GetName()] = dstDim;
            auto poIndexingVarSrc(dim->GetIndexingVariable());
            if( poIndexingVarSrc )
            {
                mapSrcVariableNameToIndexedDimName[poIndexingVarSrc->GetName()] = dim->GetName();
            }
        }

        auto attrs = poSrcGroup->GetAttributes();
        for( const auto& attr: attrs )
        {
            auto dstAttr = CreateAttribute(attr->GetName(),
                                        attr->GetDimensionsSize(),
                                        attr->GetDataType());
            EXIT_OR_CONTINUE_IF_NULL(dstAttr);
            auto raw(attr->ReadAsRaw());
            if( !dstAttr->Write(raw.data(), raw.size()) && bStrict )
                return false;
        }
        if( !attrs.empty() )
        {
            nCurCost += attrs.size() * GDALAttribute::COPY_COST;
            if( !pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData) )
                return false;
        }

        auto arrayNames = poSrcGroup->GetMDArrayNames();
        for( const auto& name: arrayNames )
        {
            auto srcArray = poSrcGroup->OpenMDArray(name);
            EXIT_OR_CONTINUE_IF_NULL(srcArray);

            // Map source dimensions to target dimensions
            std::vector<std::shared_ptr<GDALDimension>> dstArrayDims;
            const auto& srcArrayDims(srcArray->GetDimensions());
            for( const auto& dim : srcArrayDims )
            {
                auto dstDim = poDstRootGroup->OpenDimensionFromFullname(
                                                        dim->GetFullName());
                if( dstDim )
                {
                    dstArrayDims.emplace_back(dstDim);
                }
                else
                {
                    auto oIter = mapExistingDstDims.find(dim->GetName());
                    if( oIter == mapExistingDstDims.end() )
                    {
                        dstDim = CreateDimension(dim->GetName(),
                                                        dim->GetType(),
                                                        dim->GetDirection(),
                                                        dim->GetSize());
                        if( !dstDim )
                            return false;
                        mapExistingDstDims[dim->GetName()] = dstDim;
                        dstArrayDims.emplace_back(dstDim);
                    }
                    else
                    {
                        dstArrayDims.emplace_back(oIter->second);
                    }
                }
            }
            auto dstArray = CreateMDArray(srcArray->GetName(),
                                                      dstArrayDims,
                                                      srcArray->GetDataType());
            EXIT_OR_CONTINUE_IF_NULL(dstArray);

            if( !dstArray->CopyFrom(poSrcDS,
                                    srcArray.get(), bStrict,
                                    nCurCost, nTotalCost,
                                    pfnProgress, pProgressData) )
                return false;

            // If this array is the indexing variable of a dimension, link them
            // together.
            auto oIterDimName = mapSrcVariableNameToIndexedDimName.find(srcArray->GetName());
            if( oIterDimName != mapSrcVariableNameToIndexedDimName.end() )
            {
                auto oCorrespondingDimIter = mapExistingDstDims.find(oIterDimName->second);
                if( oCorrespondingDimIter != mapExistingDstDims.end() )
                {
                    CPLErrorHandlerPusher oHandlerPusher(CPLQuietErrorHandler);
                    CPLErrorStateBackuper oErrorStateBackuper;
                    oCorrespondingDimIter->second->SetIndexingVariable(dstArray);
                }
            }
        }

        auto groupNames = poSrcGroup->GetGroupNames();
        for( const auto& name: groupNames )
        {
            auto srcSubGroup = poSrcGroup->OpenGroup(name);
            EXIT_OR_CONTINUE_IF_NULL(srcSubGroup);
            auto dstSubGroup = CreateGroup(name);
            EXIT_OR_CONTINUE_IF_NULL(dstSubGroup);
            if( !dstSubGroup->CopyFrom(poDstRootGroup, poSrcDS,
                                       srcSubGroup, bStrict,
                                       nCurCost, nTotalCost,
                                       pfnProgress, pProgressData) )
                return false;
        }

        if( !pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData) )
                return false;

        return true;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return false;
    }
}

/************************************************************************/
/*                         GetInnerMostGroup()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
const GDALGroup* GDALGroup::GetInnerMostGroup(
                                        const std::string& osPathOrArrayOrDim,
                                        std::shared_ptr<GDALGroup>& curGroupHolder,
                                        std::string& osLastPart) const
{
    const GDALGroup* poCurGroup = this;
    CPLStringList aosTokens(CSLTokenizeString2(
        osPathOrArrayOrDim.c_str(), "/", 0));
    for( int i = 0; i < aosTokens.size() - 1; i++ )
    {
        curGroupHolder = poCurGroup->OpenGroup(aosTokens[i], nullptr);
        if( !curGroupHolder )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find group %s", aosTokens[i]);
            return nullptr;
        }
        poCurGroup = curGroupHolder.get();
    }
    osLastPart = aosTokens[aosTokens.size()-1];
    return poCurGroup;
}
//! @endcond

/************************************************************************/
/*                      OpenMDArrayFromFullname()                       */
/************************************************************************/

/** Get an array from its fully qualified name */
std::shared_ptr<GDALMDArray> GDALGroup::OpenMDArrayFromFullname(
                                                const std::string& osFullName,
                                                CSLConstList papszOptions) const
{
    std::string osName;
    std::shared_ptr<GDALGroup> curGroupHolder;
    auto poGroup(GetInnerMostGroup(osFullName, curGroupHolder, osName));
    if( poGroup == nullptr )
        return nullptr;
    return poGroup->OpenMDArray(osName, papszOptions);
}

/************************************************************************/
/*                      OpenDimensionFromFullname()                     */
/************************************************************************/

/** Get a dimension from its fully qualified name */
std::shared_ptr<GDALDimension> GDALGroup::OpenDimensionFromFullname(
                                        const std::string& osFullName) const
{
    std::string osName;
    std::shared_ptr<GDALGroup> curGroupHolder;
    auto poGroup(GetInnerMostGroup(osFullName, curGroupHolder, osName));
    if( poGroup == nullptr )
        return nullptr;
    auto dims(poGroup->GetDimensions());
    for( auto& dim: dims )
    {
        if( dim->GetName() == osName )
            return dim;
    }
    return nullptr;
}

/************************************************************************/
/*                       ~GDALAbstractMDArray()                         */
/************************************************************************/

GDALAbstractMDArray::~GDALAbstractMDArray() = default;

/************************************************************************/
/*                        GDALAbstractMDArray()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALAbstractMDArray::GDALAbstractMDArray(const std::string& osParentName,
                                         const std::string& osName):
    m_osName(osName),
    m_osFullName(!osParentName.empty() ? ((osParentName == "/" ? "/" : osParentName + "/") + osName) : osName)
{}
//! @endcond

/************************************************************************/
/*                           GetDimensions()                            */
/************************************************************************/

/** \fn GDALAbstractMDArray::GetDimensions() const
 * \brief Return the dimensions of an attribute/array.
 *
 * This is the same as the C functions GDALMDArrayGetDimensions() and
 * similar to GDALAttributeGetDimensionsSize().
 */

/************************************************************************/
/*                           GetDataType()                              */
/************************************************************************/

/** \fn GDALAbstractMDArray::GetDataType() const
 * \brief Return the data type of an attribute/array.
 *
 * This is the same as the C functions GDALMDArrayGetDataType() and
 * GDALAttributeGetDataType()
 */

/************************************************************************/
/*                        GetDimensionCount()                           */
/************************************************************************/

/** Return the number of dimensions.
 *
 * Default implementation is GetDimensions().size(), and may be overridden by
 * drivers if they have a faster / less expensive implementations.
 *
 * This is the same as the C function GDALMDArrayGetDimensionCount() or
 * GDALAttributeGetDimensionCount().
 *
 */
size_t GDALAbstractMDArray::GetDimensionCount() const
{
    return GetDimensions().size();
}

/************************************************************************/
/*                             CopyValue()                              */
/************************************************************************/

/** Convert a value from a source type to a destination type.
 *
 * If dstType is GEDTC_STRING, the written value will be a pointer to a char*,
 * that must be freed with CPLFree().
 */
bool GDALExtendedDataType::CopyValue(const void* pSrc,
                                    const GDALExtendedDataType& srcType,
                                    void* pDst,
                                    const GDALExtendedDataType& dstType)
{
    if( srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_NUMERIC )
    {
        GDALCopyWords( pSrc, srcType.GetNumericDataType(), 0,
                       pDst, dstType.GetNumericDataType(), 0,
                       1 );
        return true;
    }
    if( srcType.GetClass() == GEDTC_STRING &&
        dstType.GetClass() == GEDTC_STRING )
    {
        const char* srcStrPtr;
        memcpy(&srcStrPtr, pSrc, sizeof(const char*));
        char* pszDup = srcStrPtr ? CPLStrdup(srcStrPtr) : nullptr;
        memcpy(pDst, &pszDup, sizeof(char*));
        return true;
    }
    if( srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_STRING )
    {
        const char* str = nullptr;
        switch(srcType.GetNumericDataType())
        {
            case GDT_Unknown: break;
            case GDT_Byte:
                str = CPLSPrintf("%d", *static_cast<const GByte*>(pSrc));
                break;
            case GDT_UInt16:
                str = CPLSPrintf("%d", *static_cast<const GUInt16*>(pSrc));
                break;
            case GDT_Int16:
                str = CPLSPrintf("%d", *static_cast<const GInt16*>(pSrc));
                break;
            case GDT_UInt32:
                str = CPLSPrintf("%u", *static_cast<const GUInt32*>(pSrc));
                break;
            case GDT_Int32:
                str = CPLSPrintf("%d", *static_cast<const GInt32*>(pSrc));
                break;
            case GDT_Float32:
                str = CPLSPrintf("%.9g", *static_cast<const float*>(pSrc));
                break;
            case GDT_Float64:
                str = CPLSPrintf("%.18g", *static_cast<const double*>(pSrc));
                break;
            case GDT_CInt16:
            {
                const GInt16* src = static_cast<const GInt16*>(pSrc);
                str = CPLSPrintf("%d+%dj", src[0], src[1]);
                break;
            }
            case GDT_CInt32:
            {
                const GInt32* src = static_cast<const GInt32*>(pSrc);
                str = CPLSPrintf("%d+%dj", src[0], src[1]);
                break;
            }
            case GDT_CFloat32:
            {
                const float* src = static_cast<const float*>(pSrc);
                str = CPLSPrintf("%.9g+%.9gj", src[0], src[1]);
                break;
            }
            case GDT_CFloat64:
            {
                const double* src = static_cast<const double*>(pSrc);
                str = CPLSPrintf("%.18g+%.18gj", src[0], src[1]);
                break;
            }
            case GDT_TypeCount:
                CPLAssert(false);
                break;
        }
        char* pszDup = str ? CPLStrdup(str) : nullptr;
        memcpy(pDst, &pszDup, sizeof(char*));
        return true;
    }
    if( srcType.GetClass() == GEDTC_STRING &&
        dstType.GetClass() == GEDTC_NUMERIC )
    {
        const char* srcStrPtr;
        memcpy(&srcStrPtr, pSrc, sizeof(const char*));
        const double dfVal = srcStrPtr == nullptr ? 0 : CPLAtof(srcStrPtr);
        GDALCopyWords( &dfVal, GDT_Float64, 0,
                       pDst, dstType.GetNumericDataType(), 0,
                       1 );
        return true;
    }
    if( srcType.GetClass() == GEDTC_COMPOUND &&
        dstType.GetClass() == GEDTC_COMPOUND )
    {
        const auto& srcComponents = srcType.GetComponents();
        const auto& dstComponents = dstType.GetComponents();
        const GByte* pabySrc = static_cast<const GByte*>(pSrc);
        GByte* pabyDst = static_cast<GByte*>(pDst);

        std::map<std::string, const std::unique_ptr<GDALEDTComponent>*> srcComponentMap;
        for( const auto& srcComp: srcComponents )
        {
            srcComponentMap[srcComp->GetName()] = &srcComp;
        }
        for( const auto& dstComp: dstComponents )
        {
            auto oIter = srcComponentMap.find(dstComp->GetName());
            if( oIter == srcComponentMap.end() )
                return false;
            const auto& srcComp = *(oIter->second);
            if( !GDALExtendedDataType::CopyValue(pabySrc + srcComp->GetOffset(),
                                srcComp->GetType(),
                                pabyDst + dstComp->GetOffset(),
                                dstComp->GetType()) )
            {
                return false;
            };
        }
        return true;
    }

    return false;
}

/************************************************************************/
/*                       CheckReadWriteParams()                         */
/************************************************************************/
//! @cond Doxygen_Suppress
bool GDALAbstractMDArray::CheckReadWriteParams(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64*& arrayStep,
                              const GPtrDiff_t*& bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              const void* buffer,
                              const void* buffer_alloc_start,
                              size_t buffer_alloc_size,
                              std::vector<GInt64>& tmp_arrayStep,
                              std::vector<GPtrDiff_t>& tmp_bufferStride) const
{
    const auto lamda_error = []() {
        CPLError(CE_Failure, CPLE_AppDefined,
                        "Not all elements pointed by buffer will fit in "
                        "[buffer_alloc_start, "
                        "buffer_alloc_start + buffer_alloc_size[");
    };

    const auto& dims = GetDimensions();
    if( dims.empty() )
    {
        if( buffer_alloc_start )
        {
            const size_t elementSize = bufferDataType.GetSize();
            const GByte* paby_buffer = static_cast<const GByte*>(buffer);
            const GByte* paby_buffer_alloc_start =
                static_cast<const GByte*>(buffer_alloc_start);
            const GByte* paby_buffer_alloc_end =
                paby_buffer_alloc_start + buffer_alloc_size;

            if( paby_buffer < paby_buffer_alloc_start ||
                paby_buffer + elementSize > paby_buffer_alloc_end )
            {
                lamda_error();
                return false;
            }
        }
        return true;
    }

    if( arrayStep == nullptr )
    {
        tmp_arrayStep.resize(dims.size(), 1);
        arrayStep = tmp_arrayStep.data();
    }
    for( size_t i = 0; i < dims.size(); i++ )
    {
        if( count[i] == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "count[%u] = 0 is invalid",
                     static_cast<unsigned>(i));
            return false;
        }
    }
    bool bufferStride_all_positive = true;
    if( bufferStride == nullptr )
    {
        GPtrDiff_t stride = 1;
        // To compute strides we must proceed from the fastest varying dimension
        // (the last one), and then reverse the result
        for( size_t i = dims.size(); i != 0;  )
        {
            --i;
            tmp_bufferStride.push_back(stride);
            GUInt64 newStride = 0;
            bool bOK;
            try
            {
                newStride = (CPLSM(static_cast<GUInt64>(stride)) *
                             CPLSM(static_cast<GUInt64>(count[i]))).v();
                bOK = static_cast<size_t>(newStride) == newStride &&
                      newStride < std::numeric_limits<size_t>::max() / 2;
            }
            catch( ... )
            {
                bOK = false;
            }
            if( !bOK )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too big count values");
                return false;
            }
            stride = static_cast<GPtrDiff_t>(newStride);
        }
        std::reverse(tmp_bufferStride.begin(), tmp_bufferStride.end());
        bufferStride = tmp_bufferStride.data();
    }
    else
    {
        for( size_t i = 0; i < dims.size(); i++ )
        {
            if( bufferStride[i] < 0 )
            {
                bufferStride_all_positive = false;
                break;
            }
        }
    }
    for( size_t i = 0; i < dims.size(); i++ )
    {
        if( arrayStartIdx[i] >= dims[i]->GetSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "arrayStartIdx[%u] = " CPL_FRMT_GUIB " >= " CPL_FRMT_GUIB,
                     static_cast<unsigned>(i),
                     static_cast<GUInt64>(arrayStartIdx[i]),
                     static_cast<GUInt64>(dims[i]->GetSize()));
            return false;
        }
        bool bOverflow;
        if( arrayStep[i] >= 0)
        {
            try
            {
                bOverflow = (CPLSM(static_cast<GUInt64>(arrayStartIdx[i])) +
                            CPLSM(static_cast<GUInt64>(count[i] - 1)) *
                                CPLSM(static_cast<GUInt64>(arrayStep[i]))).v() >= dims[i]->GetSize();
            }
            catch( ... )
            {
                bOverflow = true;
            }
            if( bOverflow )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "arrayStartIdx[%u] + (count[%u]-1) * arrayStep[%u] >= " CPL_FRMT_GUIB,
                        static_cast<unsigned>(i),
                        static_cast<unsigned>(i),
                        static_cast<unsigned>(i),
                        static_cast<GUInt64>(dims[i]->GetSize()));
                return false;
            }
        }
        else
        {
            try
            {
                bOverflow =  arrayStartIdx[i] <
                    (CPLSM(static_cast<GUInt64>(count[i] - 1)) *
                     CPLSM(arrayStep[i] == std::numeric_limits<GInt64>::min() ?
                        (static_cast<GUInt64>(1) << 63) :
                        static_cast<GUInt64>(-arrayStep[i]))).v();
            }
            catch( ... )
            {
                bOverflow = true;
            }
            if( bOverflow )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "arrayStartIdx[%u] + (count[%u]-1) * arrayStep[%u] < 0",
                        static_cast<unsigned>(i),
                        static_cast<unsigned>(i),
                        static_cast<unsigned>(i));
                return false;
            }
        }
    }

    if( buffer_alloc_start )
    {
        const size_t elementSize = bufferDataType.GetSize();
        const GByte* paby_buffer = static_cast<const GByte*>(buffer);
        const GByte* paby_buffer_alloc_start = static_cast<const GByte*>(buffer_alloc_start);
        const GByte* paby_buffer_alloc_end = paby_buffer_alloc_start + buffer_alloc_size;
        if( bufferStride_all_positive )
        {
            if( paby_buffer < paby_buffer_alloc_start )
            {
                lamda_error();
                return false;
            }
            GUInt64 nOffset = elementSize;
            for( size_t i = 0; i < dims.size(); i++ )
            {
                try
                {
                    nOffset = (CPLSM(static_cast<GUInt64>(nOffset)) +
                                CPLSM(static_cast<GUInt64>(bufferStride[i])) *
                                CPLSM(static_cast<GUInt64>(count[i] - 1)) *
                                CPLSM(static_cast<GUInt64>(elementSize))).v();
                }
                catch( ... )
                {
                    lamda_error();
                    return false;
                }
            }
#if SIZEOF_VOID == 4
            if( static_cast<size_t>(nOffset) != nOffset )
            {
                lamda_error();
                return false;
            }
#endif
            if( paby_buffer + nOffset > paby_buffer_alloc_end )
            {
                lamda_error();
                return false;
            }
        }
        else if( dims.size() < 31 )
        {
            // Check all corners of the hypercube
            const unsigned nLoops = 1U << static_cast<unsigned>(dims.size());
            for( unsigned iCornerCode = 0; iCornerCode < nLoops; iCornerCode++ )
            {
                const GByte* paby = paby_buffer;
                for( unsigned i = 0; i < static_cast<unsigned>(dims.size()); i++ )
                {
                    if( iCornerCode & (1U << i) )
                    {
                        // We should check for integer overflows
                        paby += bufferStride[i] * (count[i] - 1) * elementSize;
                    }
                }
                if( paby < paby_buffer_alloc_start ||
                    paby + elementSize > paby_buffer_alloc_end )
                {
                    lamda_error();
                    return false;
                }
            }
        }
    }

    return true;
}
//! @endcond

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

/** Read part or totality of a multidimensional array or attribute.
 *
 * This will extract the content of a hyper-rectangle from the array into
 * a user supplied buffer.
 *
 * If bufferDataType is of type string, the values written in pDstBuffer
 * will be char* pointers and the strings should be freed with CPLFree().
 *
 * This is the same as the C function GDALMDArrayRead().
 *
 * @param arrayStartIdx Values representing the starting index to read
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to extract in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param arrayStep     Spacing between values to extract in each dimension.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      If set to nullptr, [1, 1, ... 1] will be used as a default
 *                      to indicate consecutive elements.
 *
 * @param bufferStride  Spacing between values to store in pDstBuffer.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      Negative values are possible (for example to reorder
 *                      from bottom-to-top to top-to-bottom).
 *                      If set to nullptr, will be set so that pDstBuffer is
 *                      written in a compact way, with elements of the last /
 *                      fastest varying dimension being consecutive.
 *
 * @param bufferDataType Data type of values in pDstBuffer.
 *
 * @param pDstBuffer    User buffer to store the values read. Should be big
 *                      enough to store the number of values indicated by count[] and
 *                      with the spacing of bufferStride[].
 *
 * @param pDstBufferAllocStart Optional pointer that can be used to validate the
 *                             validty of pDstBuffer. pDstBufferAllocStart should
 *                             be the pointer returned by the malloc() or equivalent
 *                             call used to allocate the buffer. It will generally
 *                             be equal to pDstBuffer (when bufferStride[] values are
 *                             all positive), but not necessarily.
 *                             If specified, nDstBufferAllocSize should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, nullptr can be passed.
 *
 * @param nDstBufferAllocSize  Optional buffer size, that can be used to validate the
 *                             validty of pDstBuffer. This is the size of the
 *                             buffer starting at pDstBufferAllocStart.
 *                             If specified, pDstBufferAllocStart should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, 0 can be passed.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::Read(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer,
                      const void* pDstBufferAllocStart,
                      size_t nDstBufferAllocSize) const
{
    if( !GetDataType().CanConvertTo(bufferDataType) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array data type is not convertable to buffer data type");
        return false;
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    if( !CheckReadWriteParams(arrayStartIdx,
                              count,
                              arrayStep,
                              bufferStride,
                              bufferDataType,
                              pDstBuffer,
                              pDstBufferAllocStart,
                              nDstBufferAllocSize,
                              tmp_arrayStep,
                              tmp_bufferStride) )
    {
        return false;
    }

    return IRead(arrayStartIdx, count, arrayStep,
                 bufferStride, bufferDataType, pDstBuffer);
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
bool GDALAbstractMDArray::IWrite(const GUInt64*,
                               const size_t*,
                               const GInt64*,
                               const GPtrDiff_t*,
                               const GDALExtendedDataType&,
                               const void*)
{
    CPLError(CE_Failure, CPLE_AppDefined, "IWrite() not implemented");
    return false;
}
//! @endcond

/************************************************************************/
/*                               Write()                                 */
/************************************************************************/

/** Write part or totality of a multidimensional array or attribute.
 *
 * This will set the content of a hyper-rectangle into the array from
 * a user supplied buffer.
 *
 * If bufferDataType is of type string, the values read from pSrcBuffer
 * will be char* pointers.
 *
 * This is the same as the C function GDALMDArrayWrite().
 *
 * @param arrayStartIdx Values representing the starting index to write
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to write in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param arrayStep     Spacing between values to write in each dimension.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      If set to nullptr, [1, 1, ... 1] will be used as a default
 *                      to indicate consecutive elements.
 *
 * @param bufferStride  Spacing between values to read from pSrcBuffer.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      Negative values are possible (for example to reorder
 *                      from bottom-to-top to top-to-bottom).
 *                      If set to nullptr, will be set so that pSrcBuffer is
 *                      written in a compact way, with elements of the last /
 *                      fastest varying dimension being consecutive.
 *
 * @param bufferDataType Data type of values in pSrcBuffer.
 *
 * @param pSrcBuffer    User buffer to read the values from. Should be big
 *                      enough to store the number of values indicated by count[] and
 *                      with the spacing of bufferStride[].
 *
 * @param pSrcBufferAllocStart Optional pointer that can be used to validate the
 *                             validty of pSrcBuffer. pSrcBufferAllocStart should
 *                             be the pointer returned by the malloc() or equivalent
 *                             call used to allocate the buffer. It will generally
 *                             be equal to pSrcBuffer (when bufferStride[] values are
 *                             all positive), but not necessarily.
 *                             If specified, nSrcBufferAllocSize should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, nullptr can be passed.
 *
 * @param nSrcBufferAllocSize  Optional buffer size, that can be used to validate the
 *                             validty of pSrcBuffer. This is the size of the
 *                             buffer starting at pSrcBufferAllocStart.
 *                             If specified, pDstBufferAllocStart should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, 0 can be passed.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::Write(const GUInt64* arrayStartIdx,
                                const size_t* count,
                                const GInt64* arrayStep,
                                const GPtrDiff_t* bufferStride,
                                const GDALExtendedDataType& bufferDataType,
                                const void* pSrcBuffer,
                                const void* pSrcBufferAllocStart,
                                size_t nSrcBufferAllocSize)
{
    if( !bufferDataType.CanConvertTo(GetDataType()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Buffer data type is not convertable to array data type");
        return false;
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    if( !CheckReadWriteParams(arrayStartIdx,
                              count,
                              arrayStep,
                              bufferStride,
                              bufferDataType,
                              pSrcBuffer,
                              pSrcBufferAllocStart,
                              nSrcBufferAllocSize,
                              tmp_arrayStep,
                              tmp_bufferStride) )
    {
        return false;
    }

    return IWrite(arrayStartIdx, count, arrayStep,
                 bufferStride, bufferDataType, pSrcBuffer);
}

/************************************************************************/
/*                          GetTotalElementsCount()                     */
/************************************************************************/

/** Return the total number of values in the array.
 *
 * This is the same as the C functions GDALMDArrayGetTotalElementsCount()
 * and GDALAttributeGetTotalElementsCount().
 *
 */
GUInt64 GDALAbstractMDArray::GetTotalElementsCount() const
{
    const auto& dims = GetDimensions();
    if( dims.empty() )
        return 1;
    GUInt64 nElts = 1;
    for( const auto& dim: dims )
    {
        try
        {
            nElts = (CPLSM(static_cast<GUInt64>(nElts)) *
                     CPLSM(static_cast<GUInt64>(dim->GetSize()))).v();
        }
        catch( ... )
        {
            return 0;
        }
    }
    return nElts;
}

/************************************************************************/
/*                           GetBlockSize()                             */
/************************************************************************/

/** Return the "natural" block size of the array along all dimensions.
 *
 * Some drivers might organize the array in tiles/blocks and reading/writing
 * aligned on those tile/block boundaries will be more efficient.
 *
 * The returned number of elements in the vector is the same as
 * GetDimensionCount(). A value of 0 should be interpretated as no hint regarding
 * the natural block size along the considered dimension.
 * "Flat" arrays will typically return a vector of values set to 0.
 *
 * The default implementation will return a vector of values set to 0.
 *
 * This method is used by GetProcessingChunkSize().
 *
 * Pedantic note: the returned type is GUInt64, so in the highly unlikeley theoretical
 * case of a 32-bit platform, this might exceed its size_t allocation capabilities.
 *
 * This is the same as the C function GDALMDArrayGetBlockSize().
 *
 * @return the block size, in number of elemnts along each dimension.
 */
std::vector<GUInt64> GDALAbstractMDArray::GetBlockSize() const
{
    return std::vector<GUInt64>(GetDimensionCount());
}

/************************************************************************/
/*                       GetProcessingChunkSize()                       */
/************************************************************************/

/** \brief Return an optimal chunk size for read/write oerations, given the natural
 * block size and memory constraints specified.
 *
 * This method will use GetBlockSize() to define a chunk whose dimensions are
 * multiple of those returned by GetBlockSize() (unless the block define by
 * GetBlockSize() is larger than nMaxChunkMemory, in which case it will be
 * returned by this method).
 *
 * This is the same as the C function GDALMDArrayGetProcessingChunkSize().
 *
 * @param nMaxChunkMemory Maximum amount of memory, in bytes, to use for the chunk.
 *
 * @return the chunk size, in number of elemnts along each dimension.
 */
std::vector<size_t> GDALAbstractMDArray::GetProcessingChunkSize(size_t nMaxChunkMemory) const
{
    const auto& dims = GetDimensions();
    const auto& nDTSize = GetDataType().GetSize();
    std::vector<size_t> anChunkSize;
    auto blockSize = GetBlockSize();
    CPLAssert( blockSize.size() == dims.size() );
    size_t nChunkSize = nDTSize;
    bool bOverflow = false;
    constexpr auto kSIZE_T_MAX = std::numeric_limits<size_t>::max();
    // Initialize anChunkSize[i] with blockSize[i] by properly clamping in
    // [1, min(sizet_max, dim_size[i])]
    // Also make sure that the product of all anChunkSize[i]) fits on size_t
    for( size_t i = 0; i < dims.size(); i++ )
    {
        const auto sizeDimI = std::max(
            static_cast<size_t>(1),
            static_cast<size_t>(
                std::min(
                    static_cast<GUInt64>(kSIZE_T_MAX),
                    std::min(blockSize[i], dims[i]->GetSize()))));
        anChunkSize.push_back(sizeDimI);
        if( nChunkSize > kSIZE_T_MAX / sizeDimI )
        {
            bOverflow = true;
        }
        else
        {
            nChunkSize *= sizeDimI;
        }
    }

    // If the product of all anChunkSize[i] does not fit on size_t, then
    // set lowest anChunkSize[i] to 1.
    if( bOverflow )
    {
        nChunkSize = nDTSize;
        bOverflow = false;
        for( size_t i = dims.size(); i > 0; )
        {
            --i;
            if( bOverflow || nChunkSize > kSIZE_T_MAX / anChunkSize[i] )
            {
                bOverflow = true;
                anChunkSize[i] = 1;
            }
            else
            {
                nChunkSize *= anChunkSize[i];
            }
        }
    }

    nChunkSize = nDTSize;
    std::vector<size_t> anAccBlockSizeFromStart;
    for( size_t i = 0; i < dims.size(); i++ )
    {
        nChunkSize *= anChunkSize[i];
        anAccBlockSizeFromStart.push_back(nChunkSize);
    }
    if( nChunkSize <= nMaxChunkMemory / 2 )
    {
        size_t nVoxelsFromEnd = 1;
        for( size_t i = dims.size(); i > 0; )
        {
            --i;
            const auto nCurBlockSize =
                anAccBlockSizeFromStart[i] * nVoxelsFromEnd;
            const auto nMul = nMaxChunkMemory / nCurBlockSize;
            if( nMul >= 2 )
            {
                const auto nSizeThisDim(dims[i]->GetSize());
                const auto nBlocksThisDim =
                            DIV_ROUND_UP(nSizeThisDim, anChunkSize[i]);
                anChunkSize[i] = static_cast<size_t>(std::min(
                    anChunkSize[i] * std::min(
                        static_cast<GUInt64>(nMul), nBlocksThisDim),
                    nSizeThisDim ));
            }
            nVoxelsFromEnd *= anChunkSize[i];
        }
    }
    return anChunkSize;
}

/************************************************************************/
/*                             SetUnit()                                */
/************************************************************************/

/** Set the variable unit.
 * 
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 * 
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArraySetUnit()
 *
 * @note Driver implementation: optionally implemented.
 *
 * @param osUnit unit name.
 * @return true in case of success.
 */
bool GDALMDArray::SetUnit(CPL_UNUSED const std::string& osUnit)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetUnit() not implemented");
    return false;
}

/************************************************************************/
/*                             GetUnit()                                */
/************************************************************************/

/** Return the array unit.
 * 
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 * 
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArrayGetUnit()
 */
const std::string& GDALMDArray::GetUnit() const
{
    static const std::string emptyString;
    return emptyString;
}

/************************************************************************/
/*                          SetSpatialRef()                             */
/************************************************************************/

/** Assign a spatial reference system object to the the array.
 * 
 * This is the same as the C function GDALMDArraySetSpatialRef().
 */
bool GDALMDArray::SetSpatialRef(CPL_UNUSED const OGRSpatialReference* poSRS)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetSpatialRef() not implemented");
    return false;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

/** Return the spatial reference system object associated with the array.
 * 
 * This is the same as the C function GDALMDArrayGetSpatialRef().
 */
std::shared_ptr<OGRSpatialReference> GDALMDArray::GetSpatialRef() const
{
    return nullptr;
}

/************************************************************************/
/*                        GetRawNoDataValue()                           */
/************************************************************************/

/** Return the nodata value as a "raw" value.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * The returned value should not be modified or freed. It is valid until
 * the array is destroyed, or the next call to GetRawNoDataValue() or
 * SetRawNoDataValue(), or any similar methods.
 *
 * @note Driver implementation: this method shall be implemented if nodata
 * is supported.
 *
 * This is the same as the C function GDALMDArrayGetRawNoDataValue().
 *
 * @return nullptr or a pointer to GetDataType().GetSize() bytes.
 */
const void* GDALMDArray::GetRawNoDataValue() const
{
    return nullptr;
}

/************************************************************************/
/*                        GetNoDataValueAsDouble()                      */
/************************************************************************/

/** Return the nodata value as a double.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * This is the same as the C function GDALMDArrayGetNoDataValueAsDouble().
 *
 * @param pbHasNoData Pointer to a output boolean that will be set to true if
 * a nodata value exists and can be converted to double. Might be nullptr.
 *
 * @return the nodata value as a double. A 0.0 value might also indicate the
 * absence of a nodata value or an error in the conversion (*pbHasNoData will be
 * set to false then).
 */
double GDALMDArray::GetNoDataValueAsDouble(bool* pbHasNoData) const
{
    const void* pNoData = GetRawNoDataValue();
    if( !pNoData )
    {
        if( pbHasNoData )
            *pbHasNoData = false;
        return 0.0;
    }
    double dfNoData = 0.0;
    if( !GDALExtendedDataType::CopyValue(pNoData,
                    GetDataType(),
                    &dfNoData,
                    GDALExtendedDataType::Create(GDT_Float64)) )
    {
        if( pbHasNoData )
            *pbHasNoData = false;
        return 0.0;
    }
    if( pbHasNoData )
        *pbHasNoData = true;
    return dfNoData;
}

/************************************************************************/
/*                        SetRawNoDataValue()                           */
/************************************************************************/

/** Set the nodata value as a "raw" value.
 *
 * The value passed might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr whose size in
 * bytes is GetDataType().GetSize() must be passed.
 *
 * This is the same as the C function GDALMDArraySetRawNoDataValue().
 *
 * @note Driver implementation: this method shall be implemented if setting nodata
 * is supported.

 * @return true in case of success.
 */
bool GDALMDArray::SetRawNoDataValue(CPL_UNUSED const void* pRawNoData)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetRawNoDataValue() not implemented");
    return false;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/** Set the nodata value as a double.
 *
 * If the natural data type of the attribute/array is not double, type conversion
 * will occur to the type returned by GetDataType().
 *
 * This is the same as the C function GDALMDArraySetNoDataValueAsDouble().
 *
 * @return true in case of success.
 */
bool GDALMDArray::SetNoDataValue(double dfNoData)
{
    void* pRawNoData = CPLMalloc(GetDataType().GetSize());
    bool bRet = false;
    if( GDALExtendedDataType::CopyValue(
                    &dfNoData, GDALExtendedDataType::Create(GDT_Float64),
                    pRawNoData, GetDataType()) )
    {
        bRet = SetRawNoDataValue(pRawNoData);
    }
    CPLFree(pRawNoData);
    return bRet;
}

/************************************************************************/
/*                               SetScale()                             */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArraySetScale().
 *
 * @note Driver implementation: this method shall be implemented if setting scale
 * is supported.
 *
 * @return true in case of success.
 */
bool GDALMDArray::SetScale(CPL_UNUSED double dfScale)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetScale() not implemented");
    return false;
}

/************************************************************************/
/*                               SetOffset)                             */
/************************************************************************/

/** Set the offset value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArraySetOffset().
 *
 * @note Driver implementation: this method shall be implemented if setting offset
 * is supported.
 *
 * @return true in case of success.
 */
bool GDALMDArray::SetOffset(CPL_UNUSED double dfOffset)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetOffset() not implemented");
    return false;
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArrayGetScale().
 *
 * @note Driver implementation: this method shall be implemented if gettings scale
 * is supported.
 *
 * @param pbHasScale Pointer to a output boolean that will be set to true if
 * a scale value exists. Might be nullptr.
 *
 * @return the scale value. A 1.0 value might also indicate the
 * absence of a scale value.
 */
double GDALMDArray::GetScale(CPL_UNUSED bool* pbHasScale) const
{
    if( *pbHasScale )
        *pbHasScale = false;
    return 1.0;
}

/************************************************************************/
/*                               GetOffset()                            */
/************************************************************************/

/** Get the offset value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArrayGetOffset().
 *
 * @note Driver implementation: this method shall be implemented if gettings offset
 * is supported.
 *
 * @param pbHasOffset Pointer to a output boolean that will be set to true if
 * a offset value exists. Might be nullptr.
 *
 * @return the offset value. A 0.0 value might also indicate the
 * absence of a offset value.
 */
double GDALMDArray::GetOffset(CPL_UNUSED bool* pbHasOffset) const
{
    if( *pbHasOffset )
        *pbHasOffset = false;
    return 0.0;
}

/************************************************************************/
/*                         ProcessPerChunk()                            */
/************************************************************************/

namespace
{
    enum class Caller
    {
        CALLER_END_OF_LOOP,
        CALLER_IN_LOOP,
    };
}

/** \brief Call a user-provided function to operate on an array chunk by chunk.
 *
 * This method is to be used when doing operations on an array, or a subset of it,
 * in a chunk by chunk way.
 *
 * @param arrayStartIdx Values representing the starting index to use
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to use in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param chunkSize     Values representing the chunk size in each dimension.
 *                      Might typically the outptu of GetProcessingChunkSize().
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param pfnFunc       User-provided function of type FuncProcessPerChunkType.
 *                      Must NOT be nullptr.
 *
 * @param pUserData     Pointer to pass as the value of the pUserData argument of
 *                      FuncProcessPerChunkType. Might be nullptr (depends on
 *                      pfnFunc.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::ProcessPerChunk(const GUInt64* arrayStartIdx,
                                          const GUInt64* count,
                                          const size_t* chunkSize,
                                          FuncProcessPerChunkType pfnFunc,
                                          void* pUserData)
{
    const auto& dims = GetDimensions();
    if( dims.empty() )
    {
        return pfnFunc(this, nullptr, nullptr, 1, 1, pUserData);
    }

    // Sanity check
    size_t nTotalChunkSize = 1;
    for( size_t i = 0; i < dims.size(); i++ )
    {
        const auto nSizeThisDim(dims[i]->GetSize());
        if( count[i] == 0 ||
            count[i] > nSizeThisDim ||
            arrayStartIdx[i] > nSizeThisDim - count[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent arrayStartIdx[] / count[] values "
                     "regarding array size");
            return false;
        }
        if( chunkSize[i] == 0 || chunkSize[i] > nSizeThisDim ||
            chunkSize[i] > std::numeric_limits<size_t>::max() / nTotalChunkSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent chunkSize[] values");
            return false;
        }
        nTotalChunkSize *= chunkSize[i];
    }

    size_t dimIdx = 0;
    std::vector<GUInt64> chunkArrayStartIdx(dims.size());
    std::vector<size_t> chunkCount(dims.size());
    struct Stack
    {
        GUInt64 nBlockCounter = 0;
        GUInt64 nBlocksMinusOne = 0;
        size_t  first_count = 0; // only used if nBlocks > 1
        Caller  return_point = Caller::CALLER_END_OF_LOOP;
    };
    std::vector<Stack> stack(dims.size());
    GUInt64 iCurChunk = 0;
    GUInt64 nChunkCount = 1;
    for( size_t i = 0; i < dims.size(); i++ )
    {
        const auto nStartBlock = arrayStartIdx[i] / chunkSize[i];
        const auto nEndBlock = (arrayStartIdx[i] + count[i] - 1) / chunkSize[i];
        stack[i].nBlocksMinusOne = nEndBlock - nStartBlock;
        nChunkCount *= 1 + stack[i].nBlocksMinusOne;
        if( stack[i].nBlocksMinusOne == 0 )
        {
            chunkArrayStartIdx[i] = arrayStartIdx[i];
            chunkCount[i] = static_cast<size_t>(count[i]);
        }
        else
        {
            stack[i].first_count = static_cast<size_t>(
                (nStartBlock + 1) * chunkSize[i] - arrayStartIdx[i]);
        }
    }

lbl_next_depth:
    if( dimIdx == dims.size() )
    {
        ++ iCurChunk;
        if( !pfnFunc(this, chunkArrayStartIdx.data(), chunkCount.data(),
                     iCurChunk, nChunkCount, pUserData) )
        {
            return false;
        }
    }
    else
    {
        if( stack[dimIdx].nBlocksMinusOne != 0 )
        {
            stack[dimIdx].nBlockCounter = stack[dimIdx].nBlocksMinusOne;
            chunkArrayStartIdx[dimIdx] = arrayStartIdx[dimIdx];
            chunkCount[dimIdx] = stack[dimIdx].first_count;
            stack[dimIdx].return_point = Caller::CALLER_IN_LOOP;
            while(true)
            {
                dimIdx ++;
                goto lbl_next_depth;
lbl_return_to_caller_in_loop:
                -- stack[dimIdx].nBlockCounter;
                if( stack[dimIdx].nBlockCounter == 0 )
                    break;
                chunkArrayStartIdx[dimIdx] += chunkCount[dimIdx];
                chunkCount[dimIdx] = chunkSize[dimIdx];
            }

            chunkArrayStartIdx[dimIdx] += chunkCount[dimIdx];
            chunkCount[dimIdx] = static_cast<size_t>(
                arrayStartIdx[dimIdx] + count[dimIdx] - chunkArrayStartIdx[dimIdx]);
            stack[dimIdx].return_point = Caller::CALLER_END_OF_LOOP;
        }
        dimIdx ++;
        goto lbl_next_depth;
lbl_return_to_caller_end_of_loop:
        if( dimIdx == 0 )
            goto end;
    }

    dimIdx --;
    switch( stack[dimIdx].return_point )
    {
        case Caller::CALLER_END_OF_LOOP: goto lbl_return_to_caller_end_of_loop;
        case Caller::CALLER_IN_LOOP:     goto lbl_return_to_caller_in_loop;
    }
end:
    return true;
}

/************************************************************************/
/*                          GDALAttribute()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALAttribute::GDALAttribute(CPL_UNUSED const std::string& osParentName,
                             CPL_UNUSED const std::string& osName)
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
    : GDALAbstractMDArray(osParentName, osName)
#endif
{}
//! @endcond

/************************************************************************/
/*                        GetDimensionSize()                            */
/************************************************************************/

/** Return the size of the dimensions of the attribute.
 *
 * This will be an empty array for a scalar (single value) attribute.
 *
 * This is the same as the C function GDALAttributeGetDimensionsSize().
 */
std::vector<GUInt64> GDALAttribute::GetDimensionsSize() const
{
    const auto& dims = GetDimensions();
    std::vector<GUInt64> ret;
    ret.reserve(dims.size());
    for( const auto& dim: dims )
        ret.push_back(dim->GetSize());
    return ret;
}

/************************************************************************/
/*                            GDALRawResult()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALRawResult::GDALRawResult(GByte* raw,
                  const GDALExtendedDataType& dt,
                  size_t nEltCount):
    m_dt(dt),
    m_nEltCount(nEltCount),
    m_nSize(nEltCount * dt.GetSize()),
    m_raw(raw)
{
}
//! @endcond

/************************************************************************/
/*                            GDALRawResult()                           */
/************************************************************************/

/** Move constructor. */
GDALRawResult::GDALRawResult(GDALRawResult&& other):
    m_dt(std::move(other.m_dt)),
    m_nEltCount(other.m_nEltCount),
    m_nSize(other.m_nSize),
    m_raw(other.m_raw)
{
    other.m_nEltCount = 0;
    other.m_nSize = 0;
    other.m_raw = nullptr;
}

/************************************************************************/
/*                               FreeMe()                               */
/************************************************************************/

void GDALRawResult::FreeMe()
{
    if( m_raw && m_dt.NeedsFreeDynamicMemory() )
    {
        GByte* pabyPtr = m_raw;
        const auto nDTSize(m_dt.GetSize());
        for( size_t i = 0; i < m_nEltCount; ++i )
        {
            m_dt.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
    }
    VSIFree(m_raw);
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

/** Move assignment. */
GDALRawResult& GDALRawResult::operator=(GDALRawResult&& other)
{
    FreeMe();
    m_dt = std::move(other.m_dt);
    m_nEltCount = other.m_nEltCount;
    m_nSize = other.m_nSize;
    m_raw = other.m_raw;
    other.m_nEltCount = 0;
    other.m_nSize = 0;
    other.m_raw = nullptr;
    return *this;
}

/************************************************************************/
/*                         ~GDALRawResult()                             */
/************************************************************************/

/** Destructor. */
GDALRawResult::~GDALRawResult()
{
    FreeMe();
}

/************************************************************************/
/*                            StealData()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
/** Return buffer to caller which becomes owner of it.
 * Only to be used by GDALAttributeReadAsRaw().
 */
GByte* GDALRawResult::StealData()
{
    GByte* ret = m_raw;
    m_raw = nullptr;
    m_nEltCount = 0;
    m_nSize = 0;
    return ret;
}
//! @endcond

/************************************************************************/
/*                             ReadAsRaw()                              */
/************************************************************************/

/** Return the raw value of an attribute.
 *
 *
 * This is the same as the C function GDALAttributeReadAsRaw().
 */
GDALRawResult GDALAttribute::ReadAsRaw() const
{
    const auto nEltCount(GetTotalElementsCount());
    const auto dt(GetDataType());
    const auto nDTSize(dt.GetSize());
    GByte* res = static_cast<GByte*>(
        VSI_MALLOC2_VERBOSE(static_cast<size_t>(nEltCount), nDTSize));
    if( !res )
        return GDALRawResult(nullptr, dt, 0);
    const auto& dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for( size_t i = 0; i < nDims; i++ )
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    if( !Read(startIdx.data(), count.data(), nullptr, nullptr,
              dt,
              &res[0], &res[0], static_cast<size_t>(nEltCount * nDTSize)) )
    {
        VSIFree(res);
        return GDALRawResult(nullptr, dt, 0);
    }
    return GDALRawResult(res, dt, static_cast<size_t>(nEltCount));
}

/************************************************************************/
/*                            ReadAsString()                            */
/************************************************************************/

/** Return the value of an attribute as a string.
 *
 * The returned string should not be freed, and its lifetime does not
 * excess a next call to ReadAsString() on the same object, or the deletion
 * of the object itself.
 *
 * This function will only return the first element if there are several.
 *
 * This is the same as the C function GDALAttributeReadAsString()
 *
 * @return a string, or nullptr.
 */
const char* GDALAttribute::ReadAsString() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    char* szRet = nullptr;
    if( !Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::CreateString(),
         &szRet, &szRet, sizeof(szRet)) ||
        szRet == nullptr )
    {
        return nullptr;
    }
    m_osCachedVal = szRet;
    CPLFree(szRet);
    return m_osCachedVal.c_str();
}

/************************************************************************/
/*                            ReadAsInt()                               */
/************************************************************************/

/** Return the value of an attribute as a integer.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can be converted to integer.
 *
 * This is the same as the C function GDALAttributeReadAsInt()
 *
 * @return a integer, or INT_MIN in case of error.
 */
int GDALAttribute::ReadAsInt() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    int nRet = INT_MIN;
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int32),
         &nRet, &nRet, sizeof(nRet));
    return nRet;
}

/************************************************************************/
/*                            ReadAsDouble()                            */
/************************************************************************/

/** Return the value of an attribute as a double.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can be converted to double.
 *
 * This is the same as the C function GDALAttributeReadAsInt()
 *
 * @return a double value.
 */
double GDALAttribute::ReadAsDouble() const
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    double dfRet = 0;
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Float64),
         &dfRet, &dfRet, sizeof(dfRet));
    return dfRet;
}

/************************************************************************/
/*                          ReadAsStringArray()                         */
/************************************************************************/

/** Return the value of an attribute as an array of strings.
 *
 * This is the same as the C function GDALAttributeReadAsStringArray()
 */
CPLStringList GDALAttribute::ReadAsStringArray() const
{
    const auto nElts = GetTotalElementsCount();
    if( nElts > static_cast<unsigned>(std::numeric_limits<int>::max() - 1) )
        return CPLStringList();
    char** papszList = static_cast<char**>(
        VSI_CALLOC_VERBOSE(static_cast<int>(nElts) + 1, sizeof(char*)));
    const auto& dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for( size_t i = 0; i < nDims; i++ )
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::CreateString(),
         papszList, papszList, sizeof(char*) * static_cast<int>(nElts));
    for( int i = 0; i < static_cast<int>(nElts); i++ )
    {
        if( papszList[i] == nullptr )
            papszList[i] = CPLStrdup("");
    }
    return CPLStringList(papszList);
}

/************************************************************************/
/*                          ReadAsIntArray()                            */
/************************************************************************/

/** Return the value of an attribute as an array of integers.
 *
 * This is the same as the C function GDALAttributeReadAsIntArray().
 */
std::vector<int> GDALAttribute::ReadAsIntArray() const
{
    const auto nElts = GetTotalElementsCount();
    if( nElts > static_cast<size_t>(nElts) )
        return {};
    std::vector<int> res(static_cast<size_t>(nElts));
    const auto& dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for( size_t i = 0; i < nDims; i++ )
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Int32),
         &res[0], res.data(), res.size() * sizeof(res[0]));
    return res;
}

/************************************************************************/
/*                         ReadAsDoubleArray()                          */
/************************************************************************/

/** Return the value of an attribute as an array of double.
 *
 * This is the same as the C function GDALAttributeReadAsDoubleArray().
 */
std::vector<double> GDALAttribute::ReadAsDoubleArray() const
{
    const auto nElts = GetTotalElementsCount();
    if( nElts > static_cast<size_t>(nElts) )
        return {};
    std::vector<double> res(static_cast<size_t>(nElts));
    const auto& dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for( size_t i = 0; i < nDims; i++ )
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    Read(startIdx.data(), count.data(), nullptr, nullptr,
         GDALExtendedDataType::Create(GDT_Float64),
         &res[0], res.data(), res.size() * sizeof(res[0]));
    return res;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from raw values expressed in GetDataType()
 *
 * The values should be provided in the type of GetDataType() and there should
 * be exactly GetTotalElementsCount() of them.
 * If GetDataType() is a string, each value should be a char* pointer.
 *
 * This is the same as the C function GDALAttributeWriteRaw().
 *
 * @param pabyValue Buffer of nLen bytes.
 * @param nLen Size of pabyValue in bytes. Should be equal to
 *             GetTotalElementsCount() * GetDataType().GetSize()
 * @return true in case of success.
 */
bool GDALAttribute::Write(const void* pabyValue, size_t nLen)
{
    if( nLen != GetTotalElementsCount() * GetDataType().GetSize() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Length is not of expected value");
        return false;
    }
    const auto& dims = GetDimensions();
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    for( size_t i = 0; i < nDims; i++ )
    {
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    }
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GetDataType(),
                 pabyValue, pabyValue, nLen);
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write an attribute from a string value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteString().
 *
 * @param pszValue Pointer to a string.
 * @return true in case of success.
 */
bool GDALAttribute::Write(const char* pszValue)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::CreateString(),
                 &pszValue, &pszValue, sizeof(pszValue));
}

/************************************************************************/
/*                              WriteInt()                              */
/************************************************************************/

/** Write an attribute from a integer value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteInt().
 *
 * @param nVal Value.
 * @return true in case of success.
 */
bool GDALAttribute::WriteInt(int nVal)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Int32),
                 &nVal, &nVal, sizeof(nVal));
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

/** Write an attribute from a double value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C function GDALAttributeWriteDouble().
 *
 * @param dfVal Value.
 * @return true in case of success.
 */
bool GDALAttribute::Write(double dfVal)
{
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims, 1);
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Float64),
                 &dfVal, &dfVal, sizeof(dfVal));
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

/** Write an attribute from an array of strings.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteStringArray().
 *
 * @param vals Array of strings.
 * @return true in case of success.
 */
bool GDALAttribute::Write(CSLConstList vals)
{
    if( static_cast<size_t>(CSLCount(vals)) != GetTotalElementsCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto& dims = GetDimensions();
    for( size_t i = 0; i < nDims; i++ )
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::CreateString(),
                 vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) * sizeof(char*));
}

/************************************************************************/
/*                                Write()                               */
/************************************************************************/

/** Write an attribute from an array of double.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C function GDALAttributeWriteDoubleArray()
 *
 * @param vals Array of double.
 * @param nVals Should be equal to GetTotalElementsCount().
 * @return true in case of success.
 */
bool GDALAttribute::Write(const double *vals, size_t nVals)
{
    if( nVals != GetTotalElementsCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of input values");
        return false;
    }
    const auto nDims = GetDimensionCount();
    std::vector<GUInt64> startIdx(1 + nDims, 0);
    std::vector<size_t> count(1 + nDims);
    const auto& dims = GetDimensions();
    for( size_t i = 0; i < nDims; i++ )
        count[i] = static_cast<size_t>(dims[i]->GetSize());
    return Write(startIdx.data(), count.data(), nullptr, nullptr,
                 GDALExtendedDataType::Create(GDT_Float64),
                 vals, vals,
                 static_cast<size_t>(GetTotalElementsCount()) * sizeof(double));
}

/************************************************************************/
/*                           GDALMDArray()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALMDArray::GDALMDArray(CPL_UNUSED const std::string &osParentName,
                         CPL_UNUSED const std::string& osName)
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
    : GDALAbstractMDArray(osParentName, osName)
#endif
{}
//! @endcond

/************************************************************************/
/*                           GetTotalCopyCost()                         */
/************************************************************************/

/** Return a total "cost" to copy the array.
 *
 * Used as a parameter for CopyFrom()
 */
GUInt64 GDALMDArray::GetTotalCopyCost() const
{
    return COPY_COST +
                GetAttributes().size() * GDALAttribute::COPY_COST +
           GetTotalElementsCount() * GetDataType().GetSize();
}


/************************************************************************/
/*                       CopyFromAllExceptValues()                      */
/************************************************************************/

//! @cond Doxygen_Suppress

bool GDALMDArray::CopyFromAllExceptValues(const GDALMDArray* poSrcArray,
                                          bool bStrict,
                                          GUInt64& nCurCost,
                                          const GUInt64 nTotalCost,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData)
{

    auto attrs = poSrcArray->GetAttributes();
    for( const auto& attr: attrs )
    {
        auto dstAttr = CreateAttribute(attr->GetName(),
                                                   attr->GetDimensionsSize(),
                                                   attr->GetDataType());
        if( !dstAttr )
        {
            if( bStrict )
                return false;
            continue;
        }
        auto raw = attr->ReadAsRaw();
        if( !dstAttr->Write(raw.data(), raw.size()) && bStrict )
            return false;
    }
    if( !attrs.empty() )
    {
        nCurCost += attrs.size() * GDALAttribute::COPY_COST;
        if( pfnProgress &&
            !pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData) )
            return false;
    }

    auto srcSRS = poSrcArray->GetSpatialRef();
    if( srcSRS )
    {
        SetSpatialRef(srcSRS.get());
    }

    const void* pNoData = poSrcArray->GetRawNoDataValue();
    if( pNoData && poSrcArray->GetDataType() == GetDataType() )
    {
        SetRawNoDataValue(pNoData);
    }

    const std::string& osUnit(poSrcArray->GetUnit());
    if( !osUnit.empty() )
    {
        SetUnit(osUnit);
    }

    bool bGotValue = false;
    const double dfOffset = poSrcArray->GetOffset(&bGotValue);
    if( bGotValue )
    {
        SetOffset(dfOffset);
    }

    bGotValue = false;
    const double dfScale = poSrcArray->GetScale(&bGotValue);
    if( bGotValue )
    {
        SetScale(dfScale);
    }

    return true;
}

//! @endcond

/************************************************************************/
/*                               CopyFrom()                             */
/************************************************************************/

/** Copy the content of an array into a new (generally empty) array.
 *
 * @param poSrcDS    Source dataset. Migt be nullptr (but for correct behaviour
 *                   of some output drivers this is not recommended)
 * @param poSrcArray Source array. Should NOT be nullptr.
 * @param bStrict Whether to enable stict mode. In strict mode, any error will
 *                stop the copy. In relaxed mode, the copy will be attempted to
 *                be pursued.
 * @param nCurCost  Should be provided as a variable initially set to 0.
 * @param nTotalCost Total cost from GetTotalCopyCost().
 * @param pfnProgress Progress callback, or nullptr.
 * @param pProgressData Progress user data, or nulptr.
 *
 * @return true in case of success (or partial success if bStrict == false).
 */
bool GDALMDArray::CopyFrom(CPL_UNUSED GDALDataset* poSrcDS,
                           const GDALMDArray* poSrcArray,
                           bool bStrict,
                           GUInt64& nCurCost,
                           const GUInt64 nTotalCost,
                           GDALProgressFunc pfnProgress,
                           void * pProgressData)
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    nCurCost += GDALMDArray::COPY_COST;

    if( !CopyFromAllExceptValues(poSrcArray, bStrict,
                                 nCurCost, nTotalCost,
                                 pfnProgress, pProgressData) )
    {
        return false;
    }

    const auto& dims = poSrcArray->GetDimensions();
    const auto nDTSize = poSrcArray->GetDataType().GetSize();
    if( dims.empty() )
    {
        std::vector<GByte> abyTmp(nDTSize);
        if( !(poSrcArray->Read(nullptr, nullptr, nullptr, nullptr,
                              GetDataType(), &abyTmp[0]) &&
              Write(nullptr, nullptr, nullptr, nullptr,
                               GetDataType(), &abyTmp[0])) &&
            bStrict )
        {
            return false;
        }
        nCurCost += GetTotalElementsCount() * GetDataType().GetSize();
        if( !pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData) )
            return false;
    }
    else
    {
        std::vector<GUInt64> arrayStartIdx(dims.size());
        std::vector<GUInt64> count(dims.size());
        for( size_t i = 0; i < dims.size(); i++ )
        {
            count[i] = static_cast<size_t>(dims[i]->GetSize());
        }

        struct CopyFunc
        {
            GDALMDArray* poDstArray = nullptr;
            std::vector<GByte> abyTmp{};
            GDALProgressFunc pfnProgress = nullptr;
            void* pProgressData = nullptr;
            GUInt64 nCurCost = 0;
            GUInt64 nTotalCost = 0;
            GUInt64 nTotalBytesThisArray = 0;
            bool bStop = false;

            static bool f(GDALAbstractMDArray* l_poSrcArray,
                          const GUInt64* chunkArrayStartIdx,
                          const size_t* chunkCount,
                          GUInt64 iCurChunk,
                          GUInt64 nChunkCount,
                          void* pUserData)
            {
                const auto dt(l_poSrcArray->GetDataType());
                auto data = static_cast<CopyFunc*>(pUserData);
                auto poDstArray = data->poDstArray;
                if( !l_poSrcArray->Read(chunkArrayStartIdx,
                                      chunkCount,
                                      nullptr, nullptr,
                                      dt,
                                      &data->abyTmp[0]) )
                {
                    return false;
                }
                bool bRet = 
                    poDstArray->Write(chunkArrayStartIdx,
                                       chunkCount,
                                       nullptr, nullptr,
                                       dt,
                                       &data->abyTmp[0]);
                if( dt.NeedsFreeDynamicMemory() )
                {
                    const auto l_nDTSize = dt.GetSize();
                    GByte* ptr = &data->abyTmp[0];
                    const size_t l_nDims(l_poSrcArray->GetDimensionCount());
                    size_t nEltCount = 1;
                    for( size_t i = 0; i < l_nDims; ++i )
                    {
                        nEltCount *= chunkCount[i];
                    }
                    for( size_t i = 0; i < nEltCount; i++ )
                    {
                        dt.FreeDynamicMemory(ptr);
                        ptr += l_nDTSize;
                    }
                }
                if( !bRet )
                {
                    return false;
                }

                double dfCurCost = double(data->nCurCost) +
                    double(iCurChunk) / nChunkCount * data->nTotalBytesThisArray;
                if( !data->pfnProgress(dfCurCost / data->nTotalCost, "",
                                       data->pProgressData) )
                {
                    data->bStop = true;
                    return false;
                }

                return true;
            }
        };

        CopyFunc copyFunc;
        copyFunc.poDstArray = this;
        copyFunc.nCurCost = nCurCost;
        copyFunc.nTotalCost = nTotalCost;
        copyFunc.nTotalBytesThisArray = GetTotalElementsCount() * nDTSize;
        copyFunc.pfnProgress = pfnProgress;
        copyFunc.pProgressData = pProgressData;
        const char* pszSwathSize = CPLGetConfigOption("GDAL_SWATH_SIZE", nullptr);
        const size_t nMaxChunkSize = pszSwathSize ?
            static_cast<size_t>(
                std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                         CPLAtoGIntBig(pszSwathSize))) :
            static_cast<size_t>(
                std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                         GDALGetCacheMax64() / 4));
        const auto anChunkSizes(GetProcessingChunkSize(nMaxChunkSize));
        size_t nRealChunkSize = nDTSize;
        for( const auto& nChunkSize: anChunkSizes )
        {
            nRealChunkSize *= nChunkSize;
        }
        try
        {
            copyFunc.abyTmp.resize(nRealChunkSize);
        }
        catch( const std::exception& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate temporary buffer");
            nCurCost += copyFunc.nTotalBytesThisArray;
            return false;
        }
        if( !const_cast<GDALMDArray*>(poSrcArray)->
                ProcessPerChunk(arrayStartIdx.data(), count.data(),
                                anChunkSizes.data(),
                                CopyFunc::f, &copyFunc) &&
            (bStrict || copyFunc.bStop) )
        {
            nCurCost += copyFunc.nTotalBytesThisArray;
            return false;
        }
        nCurCost += copyFunc.nTotalBytesThisArray;
    }

    return true;
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

/** Return structural information on the array.
 * 
 * This may be the compression, etc..
 * 
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 * 
 * This is the same as the C function GDALMDArrayGetStruturalInfo().
 */
CSLConstList GDALMDArray::GetStructuralInfo() const
{
    return nullptr;
}

/************************************************************************/
/*                       GDALSlicedMDArray                              */
/************************************************************************/

class GDALSlicedMDArray final: public GDALMDArray
{
private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    std::vector<size_t> m_mapDimIdxToParentDimIdx{}; // of size m_dims.size()
    std::vector<Range> m_parentRanges{} ; // of size m_poParent->GetDimensionCount()

    mutable std::vector<GUInt64> m_parentStart;
    mutable std::vector<size_t> m_parentCount;
    mutable std::vector<GInt64> m_parentStep;
    mutable std::vector<GPtrDiff_t> m_parentStride;

    void PrepareParentArrays(const GUInt64* arrayStartIdx,
                             const size_t* count,
                             const GInt64* arrayStep,
                             const GPtrDiff_t* bufferStride) const;

protected:
    explicit GDALSlicedMDArray(
        const std::shared_ptr<GDALMDArray>& poParent,
        std::vector<std::shared_ptr<GDALDimension>>&& dims,
        std::vector<size_t>&& mapDimIdxToParentDimIdx,
        std::vector<Range>&& parentRanges)
    :
        GDALAbstractMDArray(std::string(), "Sliced view of " + poParent->GetName()),
        GDALMDArray(std::string(), "Sliced view of " + poParent->GetName()),
        m_poParent(std::move(poParent)),
        m_dims(std::move(dims)),
        m_mapDimIdxToParentDimIdx(std::move(mapDimIdxToParentDimIdx)),
        m_parentRanges(parentRanges),
        m_parentStart(m_poParent->GetDimensionCount()),
        m_parentCount(m_poParent->GetDimensionCount(), 1),
        m_parentStep(m_poParent->GetDimensionCount()),
        m_parentStride(m_poParent->GetDimensionCount())
    {
    }

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;

public:
    static std::shared_ptr<GDALSlicedMDArray> Create(
                    const std::shared_ptr<GDALMDArray>& poParent,
                    std::vector<std::shared_ptr<GDALDimension>>&& dims,
                    std::vector<size_t>&& mapDimIdxToParentDimIdx,
                    std::vector<Range>&& parentRanges)
    {
        CPLAssert(dims.size() == mapDimIdxToParentDimIdx.size());
        CPLAssert(parentRanges.size() == poParent->GetDimensionCount());

        auto newAr(std::shared_ptr<GDALSlicedMDArray>(new GDALSlicedMDArray(
            poParent, std::move(dims), std::move(mapDimIdxToParentDimIdx),
            std::move(parentRanges))));
        newAr->SetSelf(newAr);
        return newAr;
    }

    bool IsWritable() const override { return m_poParent->IsWritable(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_poParent->GetDataType(); }

    const std::string& GetUnit() const override { return m_poParent->GetUnit(); }

    // bool SetUnit(const std::string& osUnit) override  { return m_poParent->SetUnit(osUnit); }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        auto poSrcSRS = m_poParent->GetSpatialRef();
        if( !poSrcSRS )
            return nullptr;
        auto srcMapping = poSrcSRS->GetDataAxisToSRSAxisMapping();
        std::vector<int> dstMapping;
        for( int srcAxis: srcMapping )
        {
            bool bFound = false;
            for( size_t i = 0; i < m_mapDimIdxToParentDimIdx.size(); i++ )
            {
                if( static_cast<int>(m_mapDimIdxToParentDimIdx[i]) == srcAxis - 1 )
                {
                    dstMapping.push_back(static_cast<int>(i) + 1);
                    bFound = true;
                    break;
                }
            }
            if( !bFound )
            {
                dstMapping.push_back(0);
            }
        }
        auto poClone(std::shared_ptr<OGRSpatialReference>(poSrcSRS->Clone()));
        poClone->SetDataAxisToSRSAxisMapping(dstMapping);
        return poClone;
    }

    const void* GetRawNoDataValue() const override { return m_poParent->GetRawNoDataValue(); }

    // bool SetRawNoDataValue(const void* pRawNoData) override { return m_poParent->SetRawNoDataValue(pRawNoData); }

    double GetOffset(bool* pbHasOffset) const override { return m_poParent->GetOffset(pbHasOffset); }

    double GetScale(bool* pbHasScale) const override { return m_poParent->GetScale(pbHasScale); }

    // bool SetOffset(double dfOffset) override { return m_poParent->SetOffset(dfOffset); }

    // bool SetScale(double dfScale) override { return m_poParent->SetScale(dfScale); }

    std::vector<GUInt64> GetBlockSize() const override
    {
        std::vector<GUInt64> ret(GetDimensionCount());
        const auto parentBlockSize(m_poParent->GetBlockSize());
        for( size_t i = 0; i < m_mapDimIdxToParentDimIdx.size(); ++i )
        {
            const auto iOldAxis = m_mapDimIdxToParentDimIdx[i];
            if( iOldAxis != static_cast<size_t>(-1) )
            {
                ret[i] = parentBlockSize[iOldAxis];
            }
        }
        return ret;
    }
};

/************************************************************************/
/*                        PrepareParentArrays()                         */
/************************************************************************/

void GDALSlicedMDArray::PrepareParentArrays(const GUInt64* arrayStartIdx,
                                            const size_t* count,
                                            const GInt64* arrayStep,
                                            const GPtrDiff_t* bufferStride) const
{
    const size_t nParentDimCount = m_parentRanges.size();
    for( size_t i = 0; i < nParentDimCount; i++ )
    {
        // For dimensions in parent that have no existence in sliced array
        m_parentStart[i] = m_parentRanges[i].m_nStartIdx;
    }

    for( size_t i = 0; i < m_dims.size(); i++ )
    {
        const auto iParent = m_mapDimIdxToParentDimIdx[i];
        if( iParent != static_cast<size_t>(-1) )
        {
            m_parentStart[iParent] =
                m_parentRanges[iParent].m_nIncr >= 0 ?
                    m_parentRanges[iParent].m_nStartIdx +
                        arrayStartIdx[i] * m_parentRanges[iParent].m_nIncr :
                    m_parentRanges[iParent].m_nStartIdx -
                        arrayStartIdx[i] *
                        static_cast<GUInt64>(-m_parentRanges[iParent].m_nIncr);
            m_parentCount[iParent] = count[i];
            m_parentStep[iParent] = count[i] == 1 ? 1 :
                        // other checks should have ensured this does not
                        // overflow
                        arrayStep[i] * m_parentRanges[iParent].m_nIncr;
            m_parentStride[iParent] = bufferStride[i];
        }
    }
}

/************************************************************************/
/*                             IRead()                                  */
/************************************************************************/

bool GDALSlicedMDArray::IRead(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64* arrayStep,
                              const GPtrDiff_t* bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              void* pDstBuffer) const
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Read(m_parentStart.data(),
                            m_parentCount.data(),
                            m_parentStep.data(),
                            m_parentStride.data(),
                            bufferDataType,
                            pDstBuffer);
}

/************************************************************************/
/*                             IWrite()                                  */
/************************************************************************/

bool GDALSlicedMDArray::IWrite(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64* arrayStep,
                              const GPtrDiff_t* bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              const void* pSrcBuffer)
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Write(m_parentStart.data(),
                             m_parentCount.data(),
                             m_parentStep.data(),
                             m_parentStride.data(),
                             bufferDataType,
                             pSrcBuffer);
}

/************************************************************************/
/*                        CreateSlicedArray()                           */
/************************************************************************/

static std::shared_ptr<GDALMDArray> CreateSlicedArray(
                                const std::shared_ptr<GDALMDArray>& self,
                                const std::string& activeSlice,
                                bool bRenameDimensions,
                                std::vector<GDALMDArray::ViewSpec>& viewSpecs)
{
    const auto& srcDims(self->GetDimensions());
    if( srcDims.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot slice a 0-d array");
        return nullptr;
    }

    CPLStringList aosTokens(CSLTokenizeString2(activeSlice.c_str(), ",", 0));
    const auto nTokens = static_cast<size_t>(aosTokens.size());

    std::vector<std::shared_ptr<GDALDimension>> newDims;
    std::vector<size_t> mapDimIdxToParentDimIdx;
    std::vector<GDALSlicedMDArray::Range> parentRanges;
    newDims.reserve(nTokens);
    mapDimIdxToParentDimIdx.reserve(nTokens);
    parentRanges.reserve(nTokens);

    bool bGotEllipsis = false;
    size_t nCurSrcDim = 0;
    for( size_t i = 0; i < nTokens; i++ )
    {
        const char* pszIdxSpec = aosTokens[i];
        if( EQUAL(pszIdxSpec, "...") )
        {
            if( bGotEllipsis )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Only one single ellipsis is supported");
                return nullptr;
            }
            bGotEllipsis = true;
            const auto nSubstitutionCount = srcDims.size() - (nTokens - 1);
            for( size_t j = 0; j < nSubstitutionCount; j++, nCurSrcDim++ )
            {
                parentRanges.emplace_back(0, 1);
                newDims.push_back(srcDims[nCurSrcDim]);
                mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
            }
            continue;
        }
        else if( EQUAL(pszIdxSpec, "newaxis") ||
                    EQUAL(pszIdxSpec, "np.newaxis") )
        {
            newDims.push_back(std::make_shared<GDALDimension>(
                std::string(),
                "newaxis",
                std::string(),
                std::string(),
                1));
            mapDimIdxToParentDimIdx.push_back(static_cast<size_t>(-1));
            continue;
        }
        else if( CPLGetValueType(pszIdxSpec) == CPL_VALUE_INTEGER )
        {
            if( nCurSrcDim >= srcDims.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many values in %s", activeSlice.c_str());
                return nullptr;
            }

            auto nVal = CPLAtoGIntBig(pszIdxSpec);
            GUInt64 nDimSize = srcDims[nCurSrcDim]->GetSize();
            if( (nVal >= 0 && static_cast<GUInt64>(nVal) >= nDimSize) ||
                (nVal < 0 && nDimSize < static_cast<GUInt64>(-nVal)) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Index " CPL_FRMT_GIB " is out of bounds", nVal);
                return nullptr;
            }
            if( nVal < 0 )
                nVal += nDimSize;
            parentRanges.emplace_back(nVal, 0);
        }
        else
        {
            if( nCurSrcDim >= srcDims.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many values in %s", activeSlice.c_str());
                return nullptr;
            }

            CPLStringList aosRangeTokens(CSLTokenizeString2(
                            pszIdxSpec, ":", CSLT_ALLOWEMPTYTOKENS));
            int nRangeTokens = aosRangeTokens.size();
            if( nRangeTokens > 3 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Too many : in %s", pszIdxSpec);
                return nullptr;
            }
            if( nRangeTokens <= 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Invalid value %s", pszIdxSpec);
                return nullptr;
            }
            const char* pszStart = aosRangeTokens[0];
            const char* pszEnd = aosRangeTokens[1];
            const char* pszInc = (nRangeTokens == 3) ? aosRangeTokens[2]: "";
            GDALSlicedMDArray::Range range;
            const GUInt64 nDimSize(srcDims[nCurSrcDim]->GetSize());
            range.m_nIncr = EQUAL(pszInc, "") ? 1 : CPLAtoGIntBig(pszInc);
            if( range.m_nIncr == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Invalid increment 0");
                return nullptr;
            }
            auto startIdx(CPLAtoGIntBig(pszStart));
            if( startIdx < 0 )
            {
                if( nDimSize < static_cast<GUInt64>(-startIdx) )
                    startIdx = 0;
                else
                    startIdx = nDimSize + startIdx;
            }
            range.m_nStartIdx = startIdx;
            range.m_nStartIdx = EQUAL(pszStart, "") ?
                (range.m_nIncr > 0 ? 0 : nDimSize-1) :
                range.m_nStartIdx;
            if( range.m_nStartIdx >= nDimSize - 1)
                range.m_nStartIdx = nDimSize - 1;
            auto endIdx(CPLAtoGIntBig(pszEnd));
            if( endIdx < 0 )
            {
                const auto positiveEndIdx = static_cast<GUInt64>(-endIdx);
                if( nDimSize < positiveEndIdx )
                    endIdx = 0;
                else
                    endIdx = nDimSize - positiveEndIdx;
            }
            GUInt64 nEndIdx = endIdx;
            nEndIdx = EQUAL(pszEnd, "") ?
                (range.m_nIncr < 0 ? 0 : nDimSize) :
                    nEndIdx;
            if( (range.m_nIncr > 0 && range.m_nStartIdx >= nEndIdx) ||
                (range.m_nIncr < 0 && range.m_nStartIdx <= nEndIdx) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Output dimension of size 0 is not allowed");
                return nullptr;
            }
            int inc = (EQUAL(pszEnd, "") && range.m_nIncr < 0) ? 1 : 0;
            const GUInt64 newSize = range.m_nIncr > 0 ?
                (nEndIdx - range.m_nStartIdx) / range.m_nIncr +
                    (((inc + nEndIdx - range.m_nStartIdx) % range.m_nIncr) ? 1 : 0):
                (inc + range.m_nStartIdx - nEndIdx) / -range.m_nIncr +
                    (((inc + range.m_nStartIdx - nEndIdx) % -range.m_nIncr) ? 1 : 0);
            if( range.m_nStartIdx == 0 &&
                range.m_nIncr == 1 &&
                newSize == srcDims[nCurSrcDim]->GetSize() )
            {
                newDims.push_back(srcDims[nCurSrcDim]);
            }
            else
            {
                std::string osNewDimName(srcDims[nCurSrcDim]->GetName());
                if( bRenameDimensions )
                {
                    osNewDimName =
                        "subset_" + srcDims[nCurSrcDim]->GetName() +
                            CPLSPrintf("_" CPL_FRMT_GUIB "_" CPL_FRMT_GIB "_" CPL_FRMT_GUIB,
                                    static_cast<GUIntBig>(range.m_nStartIdx),
                                    static_cast<GIntBig>(range.m_nIncr),
                                    static_cast<GUIntBig>(newSize));
                }
                newDims.push_back(std::make_shared<GDALDimension>(
                    std::string(),
                    osNewDimName,
                    srcDims[nCurSrcDim]->GetType(),
                    range.m_nIncr > 0 ?
                        srcDims[nCurSrcDim]->GetDirection():
                        std::string(),
                    newSize));
            }
            mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
            parentRanges.emplace_back(range);
        }

        nCurSrcDim++;
    }
    for( ; nCurSrcDim < srcDims.size(); nCurSrcDim++ )
    {
        parentRanges.emplace_back(0, 1);
        newDims.push_back(srcDims[nCurSrcDim]);
        mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
    }

    GDALMDArray::ViewSpec viewSpec;
    viewSpec.m_mapDimIdxToParentDimIdx = mapDimIdxToParentDimIdx;
    viewSpec.m_parentRanges = parentRanges;
    viewSpecs.emplace_back(std::move(viewSpec));

    return GDALSlicedMDArray::Create(self,
                                     std::move(newDims),
                                     std::move(mapDimIdxToParentDimIdx),
                                     std::move(parentRanges));
}

/************************************************************************/
/*                       GDALExtractFieldMDArray                        */
/************************************************************************/

class GDALExtractFieldMDArray final: public GDALMDArray
{
private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    GDALExtendedDataType m_dt;
    std::string m_srcCompName;
    mutable std::vector<GByte> m_pabyNoData{};

protected:
    GDALExtractFieldMDArray(
        const std::shared_ptr<GDALMDArray>& poParent,
        const std::unique_ptr<GDALEDTComponent>& srcComp)
    :
        GDALAbstractMDArray(
            std::string(), "Extract field view of " + poParent->GetName()),
        GDALMDArray(
            std::string(), "Extract field view of " + poParent->GetName()),
        m_poParent(poParent),
        m_dt(srcComp->GetType()),
        m_srcCompName(srcComp->GetName())
    {
        m_pabyNoData.resize(m_dt.GetSize());
    }

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    static std::shared_ptr<GDALExtractFieldMDArray> Create(
                    const std::shared_ptr<GDALMDArray>& poParent,
                    const std::unique_ptr<GDALEDTComponent>& srcComp)
    {
        auto newAr(std::shared_ptr<GDALExtractFieldMDArray>(
            new GDALExtractFieldMDArray(poParent, srcComp)));
        newAr->SetSelf(newAr);
        return newAr;
    }
    ~GDALExtractFieldMDArray()
    {
        m_dt.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    bool IsWritable() const override { return m_poParent->IsWritable(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override
    { return m_poParent->GetDimensions(); }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    const std::string& GetUnit() const override { return m_poParent->GetUnit(); }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override { return m_poParent->GetSpatialRef(); }

    const void* GetRawNoDataValue() const override
    {
        const void* parentNoData = m_poParent->GetRawNoDataValue();
        if( parentNoData == nullptr )
            return nullptr;

        m_dt.FreeDynamicMemory(&m_pabyNoData[0]);
        memset(&m_pabyNoData[0], 0, m_dt.GetSize());

        std::vector<std::unique_ptr<GDALEDTComponent>> comps;
        comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
            new GDALEDTComponent(m_srcCompName, 0, m_dt)));
        auto tmpDT(GDALExtendedDataType::Create(std::string(),
                                                m_dt.GetSize(),
                                                std::move(comps)));

        GDALExtendedDataType::CopyValue(
            parentNoData, m_poParent->GetDataType(),
            &m_pabyNoData[0], tmpDT);

        return &m_pabyNoData[0];
    }

    double GetOffset(bool* pbHasOffset) const override { return m_poParent->GetOffset(pbHasOffset); }

    double GetScale(bool* pbHasScale) const override { return m_poParent->GetScale(pbHasScale); }

    std::vector<GUInt64> GetBlockSize() const override { return m_poParent->GetBlockSize(); }
};

/************************************************************************/
/*                             IRead()                                  */
/************************************************************************/

bool GDALExtractFieldMDArray::IRead(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64* arrayStep,
                              const GPtrDiff_t* bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              void* pDstBuffer) const
{
    std::vector<std::unique_ptr<GDALEDTComponent>> comps;
    comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
        new GDALEDTComponent(m_srcCompName, 0, bufferDataType)));
    auto tmpDT(GDALExtendedDataType::Create(std::string(),
                                            bufferDataType.GetSize(),
                                            std::move(comps)));

    return m_poParent->Read(arrayStartIdx, count, arrayStep, bufferStride,
                            tmpDT, pDstBuffer);
}

/************************************************************************/
/*                      CreateFieldNameExtractArray()                   */
/************************************************************************/

static std::shared_ptr<GDALMDArray> CreateFieldNameExtractArray(
                                const std::shared_ptr<GDALMDArray>& self,
                                const std::string& fieldName)
{
    CPLAssert( self->GetDataType().GetClass() == GEDTC_COMPOUND );
    const std::unique_ptr<GDALEDTComponent>* srcComp = nullptr;
    for( const auto& comp: self->GetDataType().GetComponents() )
    {
        if( comp->GetName() == fieldName )
        {
            srcComp = &comp;
            break;
        }
    }
    if( srcComp == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find field %s",
                 fieldName.c_str());
        return nullptr;
    }
    return GDALExtractFieldMDArray::Create(self, *srcComp);
}

/************************************************************************/
/*                             GetView()                                */
/************************************************************************/

/** Return a view of the array using slicing or field access.
 *
 * The slice expression uses the same syntax as NumPy basic slicing and
 * indexing. See
 * https://www.numpy.org/devdocs/reference/arrays.indexing.html#basic-slicing-and-indexing
 * Or it can use field access by name. See
 * https://www.numpy.org/devdocs/reference/arrays.indexing.html#field-access
 * 
 * Multiple [] bracket elements can be concatenated, with a slice expression
 * or field name inside each.
 *
 * For basic slicing and indexing, inside each [] bracket element, a list of
 * indexes that apply to successive source dimensions, can be specified, using
 * integer indexing (e.g. 1), range indexing (start:stop:step), ellipsis (...)
 * or newaxis, using a comma separator.
 * 
 * Examples with a 2-dimensional array whose content is [[0,1,2,3],[4,5,6,7]].
 * <ul>
 * <li>GetView("[1][2]"): returns a 0-dimensional/scalar array with the value
 *     at index 1 in the first dimension, and index 2 in the second dimension
 *     from the source array. That is 5</li>
 * <li>GetView("[1]")->GetView("[2]"): same as above. Above is actually implemented
 *     internally doing this intermediate slicing approach.</li>
 * <li>GetView("[1,2]"): same as above, but a bit more performant.</li>
 * <li>GetView("[1]"): returns a 1-dimensional array, sliced at index 1 in the
 *     first dimension. That is [4,5,6,7].</li>
 * <li>GetView("[:,2]"): returns a 1-dimensional array, sliced at index 2 in the
 *     second dimension. That is [2,6].</li>
 * <li>GetView("[:,2:3:]"): returns a 2-dimensional array, sliced at index 2 in the
 *     second dimension. That is [[2],[6]].</li>
 * <li>GetView("[::,2]"): Same as above.</li>
 * <li>GetView("[...,2]"): same as above, in that case, since the ellipsis only
 *     expands to one dimension here.</li>
 * <li>GetView("[:,::2]"): returns a 2-dimensional array, with even-indexed elements
 *     of the second dimension. That is [[0,2],[4,6]].</li>
 * <li>GetView("[:,1::2]"): returns a 2-dimensional array, with odd-indexed elements
 *     of the second dimension. That is [[1,3],[5,7]].</li>
 * <li>GetView("[:,1:3:]"): returns a 2-dimensional array, with elements of the
 *     second dimension with index in the range [1,3[. That is [[1,2],[5,6]].</li>
 * <li>GetView("[::-1,:]"): returns a 2-dimensional array, with the values in
 *     first dimension reversed. That is [[4,5,6,7],[0,1,2,3]].</li>
 * <li>GetView("[newaxis,...]"): returns a 3-dimensional array, with an addditional
 *     dimension of size 1 put at the beginning. That is [[[0,1,2,3],[4,5,6,7]]].</li>
 * </ul>
 *
 * One difference with NumPy behaviour is that ranges that would result in
 * zero elements are not allowed (dimensions of size 0 not being allowed in the
 * GDAL multidimensional model).
 *
 * For field access, the syntax to use is ["field_name"] or ['field_name'].
 * Multipe field specification is not supported currently.
 *
 * Both type of access can be combined, e.g. GetView("[1]['field_name']")
 *
 * \note When using the GDAL Python bindings, natural Python syntax can be
 * used. That is ar[0,::,1]["foo"] will be internally translated to
 * ar.GetView("[0,::,1]['foo']")
 * \note When using the C++ API and integer indexing only, you may use the
 * at(idx0, idx1, ...) method.
 *
 * The returned array holds a reference to the original one, and thus is
 * a view of it (not a copy). If the content of the original array changes,
 * the content of the view array too. When using basic slicing and indexing,
 * the view can be written if the underlying array is writable.
 *
 * This is the same as the C function GDALMDArrayGetView()
 *
 * @param viewExpr Expression expressing basic slicing and indexing, or field access.
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 */
std::shared_ptr<GDALMDArray> GDALMDArray::GetView(const std::string& viewExpr) const
{
    std::vector<ViewSpec> viewSpecs;
    return GetView(viewExpr, true, viewSpecs);
}

//! @cond Doxygen_Suppress
std::shared_ptr<GDALMDArray> GDALMDArray::GetView(const std::string& viewExpr,
                                                  bool bRenameDimensions,
                                                  std::vector<ViewSpec>& viewSpecs) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if( !self )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    std::string curExpr(viewExpr);
    while( true )
    {
        if( curExpr.empty() || curExpr[0] != '[' )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Slice string should start with ['");
            return nullptr;
        }

        std::string fieldName;
        size_t endExpr;
        if( curExpr.size() > 2 &&
            (curExpr[1] == '"' || curExpr[1] == '\'') )
        {
            if( self->GetDataType().GetClass() != GEDTC_COMPOUND )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field access not allowed on non-compound data type");
                return nullptr;
            }
            size_t idx = 2;
            for(; idx < curExpr.size(); idx++ )
            {
                const char ch = curExpr[idx];
                if( ch == curExpr[1] )
                    break;
                if( ch == '\\' && idx + 1 < curExpr.size() )
                {
                    fieldName += curExpr[idx+1];
                    idx++;
                }
                else
                {
                    fieldName += ch;
                }
            }
            if( idx + 1 >= curExpr.size() ||
                curExpr[idx+1] != ']' )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid field access specification");
                return nullptr;
            }
            endExpr = idx + 1;
        }
        else
        {
            endExpr = curExpr.find(']');
        }
        if( endExpr == std::string::npos )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing ]'");
            return nullptr;
        }
        if( endExpr == 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "[] not allowed");
            return nullptr;
        }
        std::string activeSlice(curExpr.substr(1, endExpr-1));

        if( !fieldName.empty() )
        {
            ViewSpec viewSpec;
            viewSpec.m_osFieldName = fieldName;
            viewSpecs.emplace_back(std::move(viewSpec));
        }

        auto newArray = !fieldName.empty() ?
            CreateFieldNameExtractArray(self, fieldName):
            CreateSlicedArray(self, activeSlice, bRenameDimensions, viewSpecs);

        if( endExpr == curExpr.size() - 1 )
        {
            return newArray;
        }
        self = std::move(newArray);
        curExpr = curExpr.substr(endExpr+1);
    }
}
//! @endcond

std::shared_ptr<GDALMDArray> GDALMDArray::GetView(const std::vector<GUInt64>& indices) const
{
    std::string osExpr("[");
    bool bFirst = true;
    for(const auto& idx: indices )
    {
        if( !bFirst )
            osExpr += ',';
        bFirst = false;
        osExpr += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(idx));
    }
    return GetView(osExpr + ']');
}

/************************************************************************/
/*                            operator[]                                */
/************************************************************************/

/** Return a view of the array using field access
 *
 * Equivalent of GetView("['fieldName']")
 *
 * \note When operationg on a shared_ptr, use (*array)["fieldName"] syntax.
 */
std::shared_ptr<GDALMDArray> GDALMDArray::operator[](const std::string& fieldName) const
{
    return GetView(CPLSPrintf("['%s']",
        CPLString(fieldName).replaceAll('\\', "\\\\").
                             replaceAll('\'', "\\\'").c_str()));
}

/************************************************************************/
/*                      GDALMDArrayTransposed                           */
/************************************************************************/

class GDALMDArrayTransposed final: public GDALMDArray
{
private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<int> m_anMapNewAxisToOldAxis{};
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};

    mutable std::vector<GUInt64> m_parentStart;
    mutable std::vector<size_t> m_parentCount;
    mutable std::vector<GInt64> m_parentStep;
    mutable std::vector<GPtrDiff_t> m_parentStride;

    void PrepareParentArrays(const GUInt64* arrayStartIdx,
                             const size_t* count,
                             const GInt64* arrayStep,
                             const GPtrDiff_t* bufferStride) const;

protected:
    GDALMDArrayTransposed(
        const std::shared_ptr<GDALMDArray>& poParent,
        const std::vector<int>& anMapNewAxisToOldAxis,
        std::vector<std::shared_ptr<GDALDimension>>&& dims)
    :
        GDALAbstractMDArray(std::string(), "Transposed view of " + poParent->GetName()),
        GDALMDArray(std::string(), "Transposed view of " + poParent->GetName()),
        m_poParent(std::move(poParent)),
        m_anMapNewAxisToOldAxis(anMapNewAxisToOldAxis),
        m_dims(std::move(dims)),
        m_parentStart(m_poParent->GetDimensionCount()),
        m_parentCount(m_poParent->GetDimensionCount()),
        m_parentStep(m_poParent->GetDimensionCount()),
        m_parentStride(m_poParent->GetDimensionCount())
    {
    }

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;


public:
    static std::shared_ptr<GDALMDArrayTransposed> Create(
                    const std::shared_ptr<GDALMDArray>& poParent,
                    const std::vector<int>& anMapNewAxisToOldAxis)
    {
        const auto& parentDims(poParent->GetDimensions());
        std::vector<std::shared_ptr<GDALDimension>> dims;
        for( const auto iOldAxis: anMapNewAxisToOldAxis )
        {
            if( iOldAxis < 0 )
            {
                dims.push_back(std::make_shared<GDALDimension>(
                                    std::string(),
                                    "newaxis",
                                    std::string(),
                                    std::string(),
                                    1));
            }
            else
            {
                dims.emplace_back(parentDims[iOldAxis]);
            }
        }

        auto newAr(std::shared_ptr<GDALMDArrayTransposed>(new GDALMDArrayTransposed(
            poParent, anMapNewAxisToOldAxis, std::move(dims))));
        newAr->SetSelf(newAr);
        return newAr;
    }

    bool IsWritable() const override { return m_poParent->IsWritable(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_poParent->GetDataType(); }

    const std::string& GetUnit() const override { return m_poParent->GetUnit(); }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        auto poSrcSRS = m_poParent->GetSpatialRef();
        if( !poSrcSRS )
            return nullptr;
        auto srcMapping = poSrcSRS->GetDataAxisToSRSAxisMapping();
        std::vector<int> dstMapping;
        for( int srcAxis: srcMapping )
        {
            bool bFound = false;
            for( size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); i++ )
            {
                if( m_anMapNewAxisToOldAxis[i] == srcAxis - 1 )
                {
                    dstMapping.push_back(static_cast<int>(i) + 1);
                    bFound = true;
                    break;
                }
            }
            if( !bFound )
            {
                dstMapping.push_back(0);
            }
        }
        auto poClone(std::shared_ptr<OGRSpatialReference>(poSrcSRS->Clone()));
        poClone->SetDataAxisToSRSAxisMapping(dstMapping);
        return poClone;
    }

    const void* GetRawNoDataValue() const override { return m_poParent->GetRawNoDataValue(); }

    // bool SetRawNoDataValue(const void* pRawNoData) override { return m_poParent->SetRawNoDataValue(pRawNoData); }

    double GetOffset(bool* pbHasOffset) const override { return m_poParent->GetOffset(pbHasOffset); }

    double GetScale(bool* pbHasScale) const override { return m_poParent->GetScale(pbHasScale); }

    // bool SetOffset(double dfOffset) override { return m_poParent->SetOffset(dfOffset); }

    // bool SetScale(double dfScale) override { return m_poParent->SetScale(dfScale); }

    std::vector<GUInt64> GetBlockSize() const override
    {
        std::vector<GUInt64> ret(GetDimensionCount());
        const auto parentBlockSize(m_poParent->GetBlockSize());
        for( size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); ++i )
        {
            const auto iOldAxis = m_anMapNewAxisToOldAxis[i];
            if( iOldAxis >= 0 )
            {
                ret[i] = parentBlockSize[iOldAxis];
            }
        }
        return ret;
    }

};

/************************************************************************/
/*                         PrepareParentArrays()                        */
/************************************************************************/

void GDALMDArrayTransposed::PrepareParentArrays(const GUInt64* arrayStartIdx,
                                                const size_t* count,
                                                const GInt64* arrayStep,
                                                const GPtrDiff_t* bufferStride) const
{
    for( size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); ++i )
    {
        const auto iOldAxis = m_anMapNewAxisToOldAxis[i];
        if( iOldAxis >= 0 )
        {
            m_parentStart[iOldAxis] = arrayStartIdx[i];
            m_parentCount[iOldAxis] = count[i];
            m_parentStep[iOldAxis] = arrayStep[i];
            m_parentStride[iOldAxis] = bufferStride[i];
        }
    }
}

/************************************************************************/
/*                             IRead()                                  */
/************************************************************************/

bool GDALMDArrayTransposed::IRead(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64* arrayStep,
                              const GPtrDiff_t* bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              void* pDstBuffer) const
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Read(m_parentStart.data(),
                            m_parentCount.data(),
                            m_parentStep.data(),
                            m_parentStride.data(),
                            bufferDataType,
                            pDstBuffer);
}

/************************************************************************/
/*                            IWrite()                                  */
/************************************************************************/

bool GDALMDArrayTransposed::IWrite(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64* arrayStep,
                              const GPtrDiff_t* bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              const void* pSrcBuffer)
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Write(m_parentStart.data(),
                             m_parentCount.data(),
                             m_parentStep.data(),
                             m_parentStride.data(),
                             bufferDataType,
                             pSrcBuffer);
}

/************************************************************************/
/*                           Transpose()                                */
/************************************************************************/

/** Return a view of the array whose axis have been reordered.
 *
 * The anMapNewAxisToOldAxis parameter should contain all the values between 0
 * and GetDimensionCount() - 1, and each only once.
 * -1 can be used as a special index value to ask for the insertion of a new
 * axis of size 1.
 * The new array will have anMapNewAxisToOldAxis.size() axis, and if i is the
 * index of one of its dimension, it corresponds to the axis of index
 * anMapNewAxisToOldAxis[i] from the current array.
 *
 * This is similar to the numpy.transpose() method
 *
 * The returned array holds a reference to the original one, and thus is
 * a view of it (not a copy). If the content of the original array changes,
 * the content of the view array too. The view can be written if the underlying
 * array is writable.
 *
 * Note that I/O performance in such a transposed view might be poor.
 *
 * This is the same as the C function GDALMDArrayTranspose().
 *
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 */
std::shared_ptr<GDALMDArray> GDALMDArray::Transpose(
                        const std::vector<int>& anMapNewAxisToOldAxis) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if( !self )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    const int nDims = static_cast<int>(GetDimensionCount());
    std::vector<bool> alreadyUsedOldAxis(nDims, false);
    int nCountOldAxis = 0;
    for( const auto iOldAxis: anMapNewAxisToOldAxis )
    {
        if( iOldAxis < -1 || iOldAxis >= nDims )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid axis number");
            return nullptr;
        }
        if( iOldAxis >= 0 )
        {
            if( alreadyUsedOldAxis[iOldAxis] )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Axis %d is repeated", iOldAxis);
                return nullptr;
            }
            alreadyUsedOldAxis[iOldAxis] = true;
            nCountOldAxis ++;
        }
    }
    if( nCountOldAxis != nDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "One or several original axis missing");
        return nullptr;
    }
    return GDALMDArrayTransposed::Create(self, anMapNewAxisToOldAxis);
}

/************************************************************************/
/*                         GDALDatasetFromArray()                       */
/************************************************************************/

class GDALDatasetFromArray;

class GDALRasterBandFromArray final: public GDALRasterBand
{
    std::vector<GUInt64>     m_anOffset{};
    std::vector<size_t>      m_anCount{};
    std::vector<GPtrDiff_t>  m_anStride{};

protected:
    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;
    CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg ) override;
public:
    explicit GDALRasterBandFromArray(GDALDatasetFromArray* poDSIn,
                                     const std::vector<GUInt64>& anOtherDimCoord);

    double GetNoDataValue(int* pbHasNoData) override;
    double GetOffset(int* pbHasOffset) override;
    double GetScale(int* pbHasScale) override;
    const char* GetUnitType() override;
};


class GDALDatasetFromArray final: public GDALDataset
{
    friend class GDALRasterBandFromArray;

    std::shared_ptr<GDALMDArray> m_poArray;
    size_t m_iXDim;
    size_t m_iYDim;
    double m_adfGeoTransform[6]{0,1,0,0,0,1};
    bool m_bHasGT = false;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};

public:
    void GuessGeoTransform()
    {
        const auto& dims(m_poArray->GetDimensions());
        if( dims.size() < 2 )
            return;
        auto poVarX = dims[m_iXDim]->GetIndexingVariable();
        auto poVarY = dims[m_iYDim]->GetIndexingVariable();
        if( poVarX && poVarX->GetDimensionCount() == 1 &&
            poVarX->GetDimensions()[0]->GetSize() == dims[m_iXDim]->GetSize() &&
            poVarY && poVarY->GetDimensionCount() == 1 &&
            poVarY->GetDimensions()[0]->GetSize() == dims[m_iYDim]->GetSize() &&
            dims[m_iXDim]->GetSize() > 1 &&
            dims[m_iXDim]->GetSize() < 10 * 1000 * 1000 &&
            dims[m_iYDim]->GetSize() > 1 &&
            dims[m_iYDim]->GetSize() < 10 * 1000 * 1000 )
        {
            std::vector<double> adfTmp(static_cast<size_t>(std::max(
                 dims[m_iXDim]->GetSize(), dims[m_iYDim]->GetSize())));
            const GUInt64 anStart[1] = { 0 };
            size_t nCount = static_cast<size_t>(dims[m_iXDim]->GetSize());
            size_t anCount[1] = { nCount };
            if( !poVarX->Read(anStart, anCount, nullptr, nullptr,
                         GDALExtendedDataType::Create(GDT_Float64),
                         &adfTmp[0]) )
            {
                return;
            }

            double dfSpacing = (adfTmp[nCount-1] - adfTmp[0]) / (nCount - 1);
            for(size_t i = 1; i < nCount; i++ )
            {
                if( fabs((adfTmp[i] - adfTmp[i-1]) - dfSpacing) > 1e-3 * fabs(dfSpacing) )
                {
                    return;
                }
            }
            const double dfXStart = adfTmp[0];
            const double dfXSpacing = dfSpacing;

            nCount = static_cast<size_t>(dims[m_iYDim]->GetSize());
            anCount[0] = nCount;
            if( !poVarY->Read(anStart, anCount, nullptr, nullptr,
                         GDALExtendedDataType::Create(GDT_Float64),
                         &adfTmp[0]) )
            {
                return;
            }
            dfSpacing = (adfTmp[nCount-1] - adfTmp[0]) / (nCount - 1);
            for(size_t i = 1; i < nCount; i++ )
            {
                if( fabs((adfTmp[i] - adfTmp[i-1]) - dfSpacing) > 1e-3 * fabs(dfSpacing) )
                {
                    return;
                }
            }
            const double dfYStart = adfTmp[0];
            const double dfYSpacing = dfSpacing;
            m_bHasGT = true;
            m_adfGeoTransform[0] = dfXStart - dfXSpacing / 2;
            m_adfGeoTransform[1] = dfXSpacing;
            m_adfGeoTransform[2] = 0;
            m_adfGeoTransform[3] = dfYStart - dfYSpacing / 2;
            m_adfGeoTransform[4] = 0;
            m_adfGeoTransform[5] = dfYSpacing;
        }

    }

    GDALDatasetFromArray(const std::shared_ptr<GDALMDArray>& array,
                         size_t iXDim, size_t iYDim):
        m_poArray(array),
        m_iXDim(iXDim),
        m_iYDim(iYDim)
    {
        const auto& dims(m_poArray->GetDimensions());
        const auto nDimCount = dims.size();
        nRasterYSize = nDimCount < 2 ? 1 : static_cast<int>(
            std::min(static_cast<GUInt64>(INT_MAX), dims[iYDim]->GetSize()));
        nRasterXSize = static_cast<int>(
            std::min(static_cast<GUInt64>(INT_MAX), dims[iXDim]->GetSize()));
        eAccess = array->IsWritable() ? GA_Update: GA_ReadOnly;

        const size_t nNewDimCount = nDimCount >= 2 ? nDimCount - 2 : 0;
        std::vector<GUInt64> anOtherDimCoord(nNewDimCount);
        std::vector<GUInt64> anStackIters(nDimCount);
        std::vector<size_t> anMapNewToOld(nNewDimCount);
        for( size_t i = 0, j = 0; i < nDimCount; ++i )
        {
            if( i != iXDim && !(nDimCount >= 2 && i == iYDim) )
            {
                anMapNewToOld[j] = i;
                j++;
            }
        }

        GuessGeoTransform();

        const auto attrs(array->GetAttributes());
        for( const auto& attr: attrs )
        {
            auto stringArray = attr->ReadAsStringArray();
            std::string val;
            if( stringArray.size() > 1 )
            {
                val += '{';
            }
            for( int i = 0; i < stringArray.size(); ++i )
            {
                if( i > 0 )
                    val += ',';
                val += stringArray[i];
            }
            if( stringArray.size() > 1 )
            {
                val += '}';
            }
            SetMetadataItem(attr->GetName().c_str(), val.c_str());
        }

        // Instanciate bands by iterating over non-XY variables
        size_t iDim = 0;
lbl_next_depth:
        if( iDim < nNewDimCount )
        {
            anStackIters[iDim] = dims[anMapNewToOld[iDim]]->GetSize();
            anOtherDimCoord[iDim] = 0;
            while( true )
            {
                ++iDim;
                goto lbl_next_depth;
lbl_return_to_caller:
                --iDim;
                --anStackIters[iDim];
                if( anStackIters[iDim] == 0 )
                    break;
                ++anOtherDimCoord[iDim];
            }
        }
        else
        {
            SetBand(nBands + 1, new GDALRasterBandFromArray(this, anOtherDimCoord));
        }
        if( iDim > 0 )
            goto lbl_return_to_caller;
    }

    CPLErr GetGeoTransform(double* padfGeoTransform) override
    {
        memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
        return m_bHasGT ? CE_None : CE_Failure;
    }

    const OGRSpatialReference* GetSpatialRef() const override
    {
        if( m_poArray->GetDimensionCount() < 2 )
            return nullptr;
        m_poSRS = m_poArray->GetSpatialRef();
        if( m_poSRS )
        {
            m_poSRS.reset(m_poSRS->Clone());
            auto axisMapping = m_poSRS->GetDataAxisToSRSAxisMapping();
            for( auto& m: axisMapping )
            {
                if( m == static_cast<int>(m_iXDim) + 1 )
                    m = 1;
                else if( m == static_cast<int>(m_iYDim) + 1 )
                    m = 2;
                else
                    m = 0;
            }
            m_poSRS->SetDataAxisToSRSAxisMapping(axisMapping);
        }
        return m_poSRS.get();
    }
};

/************************************************************************/
/*                      GDALRasterBandFromArray()                       */
/************************************************************************/

GDALRasterBandFromArray::GDALRasterBandFromArray(
                                    GDALDatasetFromArray* poDSIn,
                                    const std::vector<GUInt64>& anOtherDimCoord)
{
    const auto& poArray(poDSIn->m_poArray);
    const auto& dims(poArray->GetDimensions());
    const auto nDimCount(dims.size());
    const auto blockSize(poArray->GetBlockSize());
    nBlockYSize = (nDimCount >= 2 && blockSize[poDSIn->m_iYDim]) ? static_cast<int>(
        std::min(static_cast<GUInt64>(INT_MAX), blockSize[poDSIn->m_iYDim])) : 1;
    nBlockXSize = blockSize[poDSIn->m_iXDim] ? static_cast<int>(
        std::min(static_cast<GUInt64>(INT_MAX), blockSize[poDSIn->m_iXDim])) :
        poDSIn->GetRasterXSize();
    eDataType = poArray->GetDataType().GetNumericDataType();
    eAccess = poDSIn->eAccess;
    m_anOffset.resize(nDimCount);
    m_anCount.resize(nDimCount, 1);
    m_anStride.resize(nDimCount);
    for( size_t i = 0, j = 0; i < nDimCount; ++i )
    {
        if( i != poDSIn->m_iXDim && !(nDimCount >= 2 && i == poDSIn->m_iYDim) )
        {
            std::string dimName(dims[i]->GetName());
            GUInt64 nIndex = anOtherDimCoord[j];
            // Detect subset_{orig_dim_name}_{start}_{incr}_{size} names of
            // subsetted dimensions as generated by GetView()
            if( STARTS_WITH(dimName.c_str(), "subset_") )
            {
                CPLStringList aosTokens(CSLTokenizeString2(dimName.c_str(), "_", 0));
                if( aosTokens.size() == 5 )
                {
                    dimName = aosTokens[1];
                    const auto nStartDim = static_cast<GUInt64>(
                        CPLScanUIntBig(aosTokens[2], static_cast<int>(strlen(aosTokens[2]))));
                    const auto nIncrDim = CPLAtoGIntBig(aosTokens[3]);
                    nIndex = nIncrDim > 0 ?
                        nStartDim + nIndex * nIncrDim :
                        nStartDim - (nIndex * -nIncrDim);
                }
            }
            SetMetadataItem(
                CPLSPrintf("DIM_%s_INDEX", dimName.c_str()),
                CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nIndex)) );
            auto indexingVar = dims[i]->GetIndexingVariable();
            if( indexingVar && indexingVar->GetDimensionCount() == 1 &&
                indexingVar->GetDimensions()[0]->GetSize() == dims[i]->GetSize() )
            {
                size_t nCount = 1;
                const auto& dt(indexingVar->GetDataType());
                std::vector<GByte> abyTmp(dt.GetSize());
                if( indexingVar->Read(&(anOtherDimCoord[j]), &nCount, nullptr, nullptr,
                                      dt, &abyTmp[0]) )
                {
                    char* pszTmp = nullptr;
                    GDALExtendedDataType::CopyValue(
                        &abyTmp[0], dt,
                        &pszTmp, GDALExtendedDataType::CreateString());
                    if( pszTmp )
                    {
                         SetMetadataItem(
                            CPLSPrintf("DIM_%s_VALUE", dimName.c_str()),
                            pszTmp );
                        CPLFree(pszTmp);
                    }

                    const auto unit(indexingVar->GetUnit());
                    if( !unit.empty() )
                    {
                         SetMetadataItem(
                            CPLSPrintf("DIM_%s_UNIT", dimName.c_str()),
                            unit.c_str() );
                    }
                }
            }
            m_anOffset[i] = anOtherDimCoord[j];
            j++;
        }
    }
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALRasterBandFromArray::GetNoDataValue(int* pbHasNoData)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray*>(poDS));
    const auto& poArray(l_poDS->m_poArray);
    bool bHasNodata = false;
    double dfRes = poArray->GetNoDataValueAsDouble(&bHasNodata);
    if( pbHasNoData )
        *pbHasNoData = bHasNodata;
    return dfRes;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALRasterBandFromArray::GetOffset(int* pbHasOffset)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray*>(poDS));
    const auto& poArray(l_poDS->m_poArray);
    bool bHasValue = false;
    double dfRes = poArray->GetOffset(&bHasValue);
    if( pbHasOffset )
        *pbHasOffset = bHasValue;
    return dfRes;
}

/************************************************************************/
/*                           GetUnitType()                              */
/************************************************************************/

const char* GDALRasterBandFromArray::GetUnitType()
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray*>(poDS));
    const auto& poArray(l_poDS->m_poArray);
    return poArray->GetUnit().c_str();
}

/************************************************************************/
/*                             GetScale()                              */
/************************************************************************/

double GDALRasterBandFromArray::GetScale(int* pbHasScale)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray*>(poDS));
    const auto& poArray(l_poDS->m_poArray);
    bool bHasValue = false;
    double dfRes = poArray->GetScale(&bHasValue);
    if( pbHasScale )
        *pbHasScale = bHasValue;
    return dfRes;
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IReadBlock( int nBlockXOff,
                                            int nBlockYOff,
                                            void * pImage )
{
    const int nDTSize(GDALGetDataTypeSizeBytes(eDataType));
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Read, nXOff, nYOff, nReqXSize, nReqYSize,
                     pImage, nReqXSize, nReqYSize, eDataType,
                     nDTSize, nDTSize * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IWriteBlock( int nBlockXOff,
                                             int nBlockYOff,
                                             void * pImage )
{
    const int nDTSize(GDALGetDataTypeSizeBytes(eDataType));
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Write, nXOff, nYOff, nReqXSize, nReqYSize,
                     pImage, nReqXSize, nReqYSize, eDataType,
                     nDTSize, nDTSize * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg )
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray*>(poDS));
    const auto& poArray(l_poDS->m_poArray);
    const int nDTSize(GDALGetDataTypeSizeBytes(eDataType));
    if( nXSize == nBufXSize && nYSize == nBufYSize && nDTSize > 0 &&
        (nPixelSpaceBuf % nDTSize) == 0 && (nLineSpaceBuf % nDTSize) == 0 )
    {
        m_anOffset[l_poDS->m_iXDim] = static_cast<GUInt64>(nXOff);
        m_anCount[l_poDS->m_iXDim] = static_cast<size_t>(nXSize);
        m_anStride[l_poDS->m_iXDim] =
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nDTSize);
        if( poArray->GetDimensionCount() >= 2 )
        {
            m_anOffset[l_poDS->m_iYDim] = static_cast<GUInt64>(nYOff);
            m_anCount[l_poDS->m_iYDim] = static_cast<size_t>(nYSize);
            m_anStride[l_poDS->m_iYDim] =
                static_cast<GPtrDiff_t>(nLineSpaceBuf / nDTSize);
        }
        if( eRWFlag == GF_Read )
        {
            return poArray->Read(m_anOffset.data(),
                                 m_anCount.data(),
                                 nullptr, m_anStride.data(),
                                 poArray->GetDataType(), pData) ?
                                 CE_None : CE_Failure;
        }
        else
        {
            return poArray->Write(m_anOffset.data(),
                                  m_anCount.data(),
                                  nullptr, m_anStride.data(),
                                  poArray->GetDataType(), pData) ?
                                  CE_None : CE_Failure;
        }
    }
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpaceBuf, nLineSpaceBuf,
                                     psExtraArg);
}

/************************************************************************/
/*                          AsClassicDataset()                         */
/************************************************************************/

/** Return a view of this array as a "classic" GDALDataset (ie 2D)
 *
 * In the case of > 2D arrays, additional dimensions will be represented as
 * raster bands.
 *
 * The "reverse" method is GDALRasterBand::AsMDArray().
 *
 * This is the same as the C function GDALMDArrayAsClassicDataset().
 *
 * @param iXDim Index of the dimension that will be used as the X/width axis.
 * @param iYDim Index of the dimension that will be used as the Y/height axis.
 *              Ignored if the dimension count is 1.
 * @return a new GDALDataset that must be freed with GDALClose(), or nullptr
 */
GDALDataset* GDALMDArray::AsClassicDataset(size_t iXDim, size_t iYDim) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if( !self )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    const auto nDimCount(GetDimensionCount());
    if( nDimCount == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported number of dimensions");
        return nullptr;
    }
    if( GetDataType().GetClass() != GEDTC_NUMERIC ||
        GetDataType().GetNumericDataType() == GDT_Unknown )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only arrays with numeric data types "
                 "can be exposed as classic GDALDataset");
        return nullptr;
    }
    if( iXDim >= nDimCount ||
        (nDimCount >=2 && (iYDim >= nDimCount || iXDim == iYDim)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid iXDim and/or iYDim");
        return nullptr;
    }
    GUInt64 nBands = 1;
    const auto& dims(GetDimensions());
    for( size_t i = 0; i < nDimCount; ++i )
    {
        if( i != iXDim && !(nDimCount >= 2 && i == iYDim) )
        {
            if( dims[i]->GetSize() > 65536 / nBands )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many bands. Operate on a sliced view");
                return nullptr;
            }
            nBands *= dims[i]->GetSize();
        }
    }
    return new GDALDatasetFromArray(self, iXDim, iYDim);
}

/************************************************************************/
/*                       ~GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::~GDALExtendedDataType() = default;

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(size_t nMaxStringLength):
    m_eClass(GEDTC_STRING),
    m_nSize(sizeof(char*)),
    m_nMaxStringLength(nMaxStringLength)
{}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(GDALDataType eType):
    m_eClass(GEDTC_NUMERIC),
    m_eNumericDT(eType),
    m_nSize(GDALGetDataTypeSizeBytes(eType))
{}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

GDALExtendedDataType::GDALExtendedDataType(
    const std::string& osName,
    size_t nTotalSize,
    std::vector<std::unique_ptr<GDALEDTComponent>>&& components):
    m_osName(osName),
    m_eClass(GEDTC_COMPOUND),
    m_aoComponents(std::move(components)),
    m_nSize(nTotalSize)
{
}

/************************************************************************/
/*                        GDALExtendedDataType()                        */
/************************************************************************/

/** Copy constructor. */
GDALExtendedDataType::GDALExtendedDataType(const GDALExtendedDataType& other):
    m_osName(other.m_osName),
    m_eClass(other.m_eClass),
    m_eNumericDT(other.m_eNumericDT),
    m_nSize(other.m_nSize),
    m_nMaxStringLength(other.m_nMaxStringLength)
{
    if( m_eClass == GEDTC_COMPOUND )
    {
        for( const auto& elt: other.m_aoComponents )
        {
            m_aoComponents.emplace_back(new GDALEDTComponent(*elt));
        }
    }
}

/************************************************************************/
/*                            operator= ()                              */
/************************************************************************/

/** Move assignment. */
GDALExtendedDataType& GDALExtendedDataType::operator= (GDALExtendedDataType&& other)
{
    m_osName = std::move(other.m_osName);
    m_eClass = other.m_eClass;
    m_eNumericDT = other.m_eNumericDT;
    m_nSize = other.m_nSize;
    m_nMaxStringLength = other.m_nMaxStringLength;
    m_aoComponents = std::move(other.m_aoComponents);
    other.m_eClass = GEDTC_NUMERIC;
    other.m_eNumericDT = GDT_Unknown;
    other.m_nSize = 0;
    other.m_nMaxStringLength = 0;
    return *this;
}

/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_NUMERIC.
 *
 * This is the same as the C function GDALExtendedDataTypeCreate()
 *
 * @param eType Numeric data type.
 */
GDALExtendedDataType GDALExtendedDataType::Create(GDALDataType eType)
{
    return GDALExtendedDataType(eType);
}

/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_COMPOUND.
 *
 * This is the same as the C function GDALExtendedDataTypeCreateCompound()
 *
 * @param osName Type name.
 * @param nTotalSize Total size of the type in bytes.
 *                   Should be large enough to store all components.
 * @param components Components of the compound type.
 */
GDALExtendedDataType GDALExtendedDataType::Create(
    const std::string& osName,
    size_t nTotalSize,
    std::vector<std::unique_ptr<GDALEDTComponent>>&& components)
{
    size_t nLastOffset = 0;
    // Some arbitrary threshold to avoid potential integer overflows
    if( nTotalSize > static_cast<size_t>(std::numeric_limits<int>::max() / 2) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
        return GDALExtendedDataType(GDT_Unknown);
    }
    for( const auto& comp: components )
    {
        // Check alignment too ?
        if( comp->GetOffset() < nLastOffset )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
            return GDALExtendedDataType(GDT_Unknown);
        }
        nLastOffset = comp->GetOffset() + comp->GetType().GetSize();
    }
    if( nTotalSize < nLastOffset )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid offset/size");
        return GDALExtendedDataType(GDT_Unknown);
    }
    if( nTotalSize == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty compound not allowed");
        return GDALExtendedDataType(GDT_Unknown);
    }
    return GDALExtendedDataType(osName, nTotalSize, std::move(components));
}

/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_STRING.
 *
 * This is the same as the C function GDALExtendedDataTypeCreateString().
 * 
 * @param nMaxStringLength maximum length of a string in bytes. 0 if unknown/unlimited
 */
GDALExtendedDataType GDALExtendedDataType::CreateString(size_t nMaxStringLength)
{
    return GDALExtendedDataType(nMaxStringLength);
}

/************************************************************************/
/*                           operator==()                               */
/************************************************************************/

/** Equality operator.
 *
 * This is the same as the C function GDALExtendedDataTypeEquals().
 */
bool GDALExtendedDataType::operator==(const GDALExtendedDataType& other) const
{
    if( m_eClass != other.m_eClass ||
        m_nSize != other.m_nSize ||
        m_osName != other.m_osName )
    {
        return false;
    }
    if( m_eClass == GEDTC_NUMERIC )
    {
        return m_eNumericDT == other.m_eNumericDT;
    }
    if( m_eClass == GEDTC_STRING )
    {
        return true;
    }
    CPLAssert( m_eClass == GEDTC_COMPOUND );
    if( m_aoComponents.size() != other.m_aoComponents.size() )
    {
        return false;
    }
    for( size_t i = 0; i < m_aoComponents.size(); i++ )
    {
        if( !(*m_aoComponents[i] == *other.m_aoComponents[i]) )
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                        CanConvertTo()                                */
/************************************************************************/

/** Return whether this data type can be converted to the other one.
 *
 * This is the same as the C function GDALExtendedDataTypeCanConvertTo().
 *
 * @param other Target data type for the conversion being considered.
 */
bool GDALExtendedDataType::CanConvertTo(const GDALExtendedDataType& other) const
{
    if( m_eClass == GEDTC_NUMERIC )
    {
        if( m_eNumericDT == GDT_Unknown )
            return false;
        if( other.m_eClass == GEDTC_NUMERIC && other.m_eNumericDT == GDT_Unknown )
            return false;
        return other.m_eClass == GEDTC_NUMERIC || other.m_eClass == GEDTC_STRING;
    }
    if( m_eClass == GEDTC_STRING )
    {
        return other.m_eClass == m_eClass;
    }
    CPLAssert( m_eClass == GEDTC_COMPOUND );
    if( other.m_eClass != GEDTC_COMPOUND )
        return false;
    std::map<std::string, const std::unique_ptr<GDALEDTComponent>*> srcComponents;
    for( const auto& srcComp: m_aoComponents )
    {
        srcComponents[srcComp->GetName()] = &srcComp;
    }
    for( const auto& dstComp: other.m_aoComponents )
    {
        auto oIter = srcComponents.find(dstComp->GetName());
        if( oIter == srcComponents.end() )
            return false;
        if( !(*(oIter->second))->GetType().CanConvertTo(dstComp->GetType()) )
            return false;
    }
    return true;
}

/************************************************************************/
/*                     NeedsFreeDynamicMemory()                         */
/************************************************************************/

/** Return whether the data type holds dynamically allocated memory, that
 * needs to be freed with FreeDynamicMemory().
 *
 */
bool GDALExtendedDataType::NeedsFreeDynamicMemory() const
{
    switch( m_eClass )
    {
        case GEDTC_STRING:
            return true;

        case GEDTC_NUMERIC:
            return false;

        case GEDTC_COMPOUND:
        {
            for( const auto& comp: m_aoComponents )
            {
                if( comp->GetType().NeedsFreeDynamicMemory() )
                    return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*                        FreeDynamicMemory()                           */
/************************************************************************/

/** Release the dynamic memory (strings typically) from a raw value.
 *
 * This is the same as the C function GDALExtendedDataTypeFreeDynamicMemory().
 *
 * @param pBuffer Raw buffer of a single element of an attribute or array value.
 */
void GDALExtendedDataType::FreeDynamicMemory(void* pBuffer) const
{
    switch( m_eClass )
    {
        case GEDTC_STRING:
        {
            char* pszStr;
            memcpy(&pszStr, pBuffer, sizeof(char*));
            if( pszStr )
            {
                VSIFree(pszStr);
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            break;
        }

        case GEDTC_COMPOUND:
        {
            GByte* pabyBuffer = static_cast<GByte*>(pBuffer);
            for( const auto& comp: m_aoComponents )
            {
                comp->GetType().FreeDynamicMemory(pabyBuffer + comp->GetOffset());
            }
            break;
        }
    }
}


/************************************************************************/
/*                      ~GDALEDTComponent()                             */
/************************************************************************/

GDALEDTComponent::~GDALEDTComponent() = default;

/************************************************************************/
/*                      GDALEDTComponent()                              */
/************************************************************************/

/** constructor of a GDALEDTComponent
 *
 * This is the same as the C function GDALEDTComponendCreate()
 *
 * @param name Component name
 * @param offset Offset in byte of the component in the compound data type.
 *               In case of nesting of compound data type, this should be
 *               the offset to the immediate belonging data type, not to the
 *               higher level one.
 * @param type   Component data type.
 */
GDALEDTComponent::GDALEDTComponent(const std::string& name,
                                   size_t offset,
                                   const GDALExtendedDataType& type):
    m_osName(name),
    m_nOffset(offset),
    m_oType(type)
{}

/************************************************************************/
/*                      GDALEDTComponent()                              */
/************************************************************************/

/** Copy constructor. */
GDALEDTComponent::GDALEDTComponent(const GDALEDTComponent&) = default;

/************************************************************************/
/*                           operator==()                               */
/************************************************************************/

/** Equality operator.
 */
bool GDALEDTComponent::operator==(const GDALEDTComponent& other) const
{
    return m_osName == other.m_osName &&
           m_nOffset == other.m_nOffset &&
           m_oType == other.m_oType;
}

/************************************************************************/
/*                        ~GDALDimension()                              */
/************************************************************************/

GDALDimension::~GDALDimension() = default;

/************************************************************************/
/*                         GDALDimension()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
/** Constructor.
 *
 * @param osParentName Parent name
 * @param osName name
 * @param osType type. See GetType().
 * @param osDirection direction. See GetDirection().
 * @param nSize size.
 */
GDALDimension::GDALDimension(const std::string& osParentName,
                             const std::string& osName,
                             const std::string& osType,
                             const std::string& osDirection,
                             GUInt64 nSize):
    m_osName(osName),
    m_osFullName(!osParentName.empty() ? ((osParentName == "/" ? "/" : osParentName + "/") + osName) : osName),
    m_osType(osType),
    m_osDirection(osDirection),
    m_nSize(nSize)
{
}
//! @endcond

/************************************************************************/
/*                         GetIndexingVariable()                        */
/************************************************************************/

/** Return the variable that is used to index the dimension (if there is one).
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 */
std::shared_ptr<GDALMDArray> GDALDimension::GetIndexingVariable() const
{
    return nullptr;
}

/************************************************************************/
/*                         SetIndexingVariable()                        */
/************************************************************************/

/** Set the variable that is used to index the dimension.
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * Optionaly implemented by drivers.
 *
 * Drivers known to implement it: MEM.
 *
 * @param poArray Variable to use to index the dimension.
 * @return true in case of success.
 */
bool GDALDimension::SetIndexingVariable(CPL_UNUSED std::shared_ptr<GDALMDArray> poArray)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetIndexingVariable() not implemented");
    return false;
}

/************************************************************************/
/************************************************************************/
/************************************************************************/
/*                              C API                                   */
/************************************************************************/
/************************************************************************/
/************************************************************************/


struct GDALExtendedDataTypeHS
{
    std::unique_ptr<GDALExtendedDataType> m_poImpl;

    explicit GDALExtendedDataTypeHS(GDALExtendedDataType* dt): m_poImpl(dt) {}
};

struct GDALEDTComponentHS
{
    std::unique_ptr<GDALEDTComponent> m_poImpl;

    explicit GDALEDTComponentHS(const GDALEDTComponent& component):
        m_poImpl(new GDALEDTComponent(component)) {}
};

struct GDALGroupHS
{
    std::shared_ptr<GDALGroup> m_poImpl;

    explicit GDALGroupHS(std::shared_ptr<GDALGroup> poGroup): m_poImpl(poGroup) {}
};

struct GDALMDArrayHS
{
    std::shared_ptr<GDALMDArray> m_poImpl;

    explicit GDALMDArrayHS(std::shared_ptr<GDALMDArray> poArray): m_poImpl(poArray) {}
};

struct GDALAttributeHS
{
    std::shared_ptr<GDALAttribute> m_poImpl;

    explicit GDALAttributeHS(std::shared_ptr<GDALAttribute> poAttr): m_poImpl(poAttr) {}
};

struct GDALDimensionHS
{
    std::shared_ptr<GDALDimension> m_poImpl;

    explicit GDALDimensionHS(std::shared_ptr<GDALDimension> poDim): m_poImpl(poDim) {}
};

/************************************************************************/
/*                      GDALExtendedDataTypeCreate()                    */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_NUMERIC.
 *
 * This is the same as the C++ method GDALExtendedDataType::Create()
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @param eType Numeric data type.
 *
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH GDALExtendedDataTypeCreate(GDALDataType eType)
{
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(
            GDALExtendedDataType::Create(eType)));
}

/************************************************************************/
/*                    GDALExtendedDataTypeCreateString()                */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_STRING.
 *
 * This is the same as the C++ method GDALExtendedDataType::CreateString()
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH GDALExtendedDataTypeCreateString(size_t nMaxStringLength)
{
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(
            GDALExtendedDataType::CreateString(nMaxStringLength)));
}

/************************************************************************/
/*                   GDALExtendedDataTypeCreateCompound()               */
/************************************************************************/

/** Return a new GDALExtendedDataType of class GEDTC_COMPOUND.
 *
 * This is the same as the C++ method GDALExtendedDataType::Create(const std::string&, size_t, std::vector<std::unique_ptr<GDALEDTComponent>>&&)
 *
 * The returned handle should be freed with GDALExtendedDataTypeRelease().
 *
 * @param pszName Type name.
 * @param nTotalSize Total size of the type in bytes.
 *                   Should be large enough to store all components.
 * @param nComponents Number of components in comps array.
 * @param comps Components.
 * @return a new GDALExtendedDataTypeH handle, or nullptr.
 */
GDALExtendedDataTypeH GDALExtendedDataTypeCreateCompound(
    const char* pszName, size_t nTotalSize, size_t nComponents,
    const GDALEDTComponentH* comps)
{
    std::vector<std::unique_ptr<GDALEDTComponent>> compsCpp;
    for( size_t i = 0; i < nComponents; i++ )
    {
        compsCpp.emplace_back(std::unique_ptr<GDALEDTComponent>(
            new GDALEDTComponent(*(comps[i]->m_poImpl.get()))));
    }
    auto dt = GDALExtendedDataType::Create(pszName ? pszName : "",
                                           nTotalSize, std::move(compsCpp));
    if( dt.GetClass() != GEDTC_COMPOUND )
        return nullptr;
    return new GDALExtendedDataTypeHS(new GDALExtendedDataType(dt));
}

/************************************************************************/
/*                     GDALExtendedDataTypeRelease()                    */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALExtendedDataTypeH.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALExtendedDataTypeRelease(GDALExtendedDataTypeH hEDT)
{
    delete hEDT;
}

/************************************************************************/
/*                     GDALExtendedDataTypeGetName()                    */
/************************************************************************/

/** Return type name.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetName()
 */
const char* GDALExtendedDataTypeGetName(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1( hEDT, __func__, "" );
    return hEDT->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                     GDALExtendedDataTypeGetClass()                    */
/************************************************************************/

/** Return type class.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetClass()
 */
GDALExtendedDataTypeClass GDALExtendedDataTypeGetClass(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1( hEDT, __func__, GEDTC_NUMERIC );
    return hEDT->m_poImpl->GetClass();
}

/************************************************************************/
/*               GDALExtendedDataTypeGetNumericDataType()               */
/************************************************************************/

/** Return numeric data type (only valid when GetClass() == GEDTC_NUMERIC)
 *
 * This is the same as the C++ method GDALExtendedDataType::GetNumericDataType()
 */
GDALDataType GDALExtendedDataTypeGetNumericDataType(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1( hEDT, __func__, GDT_Unknown );
    return hEDT->m_poImpl->GetNumericDataType();
}

/************************************************************************/
/*                   GDALExtendedDataTypeGetSize()                      */
/************************************************************************/

/** Return data type size in bytes.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetSize()
 */
size_t  GDALExtendedDataTypeGetSize(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1( hEDT, __func__, 0 );
    return hEDT->m_poImpl->GetSize();
}

/************************************************************************/
/*              GDALExtendedDataTypeGetMaxStringLength()                */
/************************************************************************/

/** Return the maximum length of a string in bytes.
 *
 * 0 indicates unknown/unlimited string.
 *
 * This is the same as the C++ method GDALExtendedDataType::GetMaxStringLength()
 */
size_t  GDALExtendedDataTypeGetMaxStringLength(GDALExtendedDataTypeH hEDT)
{
    VALIDATE_POINTER1( hEDT, __func__, 0 );
    return hEDT->m_poImpl->GetMaxStringLength();
}

/************************************************************************/
/*                    GDALExtendedDataTypeCanConvertTo()                */
/************************************************************************/

/** Return whether this data type can be converted to the other one.
 *
 * This is the same as the C function GDALExtendedDataType::CanConvertTo()
 *
 * @param hSourceEDT Source data type for the conversion being considered.
 * @param hTargetEDT Target data type for the conversion being considered.
 * @return TRUE if hSourceEDT can be convert to hTargetEDT. FALSE otherwise.
 */
int GDALExtendedDataTypeCanConvertTo(GDALExtendedDataTypeH hSourceEDT,
                                     GDALExtendedDataTypeH hTargetEDT)
{
    VALIDATE_POINTER1( hSourceEDT, __func__, FALSE );
    VALIDATE_POINTER1( hTargetEDT, __func__, FALSE );
    return hSourceEDT->m_poImpl->CanConvertTo(*(hTargetEDT->m_poImpl));
}

/************************************************************************/
/*                        GDALExtendedDataTypeEquals()                  */
/************************************************************************/

/** Return whether this data type is equal to another one.
 *
 * This is the same as the C++ method GDALExtendedDataType::operator==()
 *
 * @param hFirstEDT First data type.
 * @param hSecondEDT Second data type.
 * @return TRUE if they are equal. FALSE otherwise.
 */
int GDALExtendedDataTypeEquals(GDALExtendedDataTypeH hFirstEDT,
                               GDALExtendedDataTypeH hSecondEDT)
{
    VALIDATE_POINTER1( hFirstEDT, __func__, FALSE );
    VALIDATE_POINTER1( hSecondEDT, __func__, FALSE );
    return *(hFirstEDT->m_poImpl) == *(hSecondEDT->m_poImpl);
}

/************************************************************************/
/*                     GDALExtendedDataTypeGetComponents()              */
/************************************************************************/

/** Return the components of the data type (only valid when GetClass() == GEDTC_COMPOUND)
 *
 * The returned array and its content must be freed with GDALExtendedDataTypeFreeComponents(). If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALExtendedDataTypeRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALExtendedDataType::GetComponents()
 *
 * @param hEDT Data type
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return an array of *pnCount components.
 */
GDALEDTComponentH *GDALExtendedDataTypeGetComponents(
                            GDALExtendedDataTypeH hEDT, size_t* pnCount)
{
    VALIDATE_POINTER1( hEDT, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    const auto& components = hEDT->m_poImpl->GetComponents();
    auto ret = static_cast<GDALEDTComponentH*>(
        CPLMalloc(sizeof(GDALEDTComponentH) * components.size()));
    for( size_t i = 0; i < components.size(); i++ )
    {
        ret[i] = new GDALEDTComponentHS(*components[i].get());
    }
    *pnCount = components.size();
    return ret;
}

/************************************************************************/
/*                     GDALExtendedDataTypeFreeComponents()             */
/************************************************************************/

/** Free the return of GDALExtendedDataTypeGetComponents().
 *
 * @param components return value of GDALExtendedDataTypeGetComponents()
 * @param nCount *pnCount value returned by GDALExtendedDataTypeGetComponents()
 */
void GDALExtendedDataTypeFreeComponents(GDALEDTComponentH* components, size_t nCount)
{
    for( size_t i = 0; i < nCount; i++ )
    {
        delete components[i];
    }
    CPLFree(components);
}

/************************************************************************/
/*                         GDALEDTComponentCreate()                     */
/************************************************************************/

/** Create a new GDALEDTComponent.
 *
 * The returned value must be freed with GDALEDTComponentRelease().
 *
 * This is the same as the C++ constructor GDALEDTComponent::GDALEDTComponent().
 */
GDALEDTComponentH GDALEDTComponentCreate(const char* pszName, size_t nOffset, GDALExtendedDataTypeH hType)
{
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    VALIDATE_POINTER1( hType, __func__, nullptr );
    return new GDALEDTComponentHS(GDALEDTComponent(pszName, nOffset, *(hType->m_poImpl.get())));
}

/************************************************************************/
/*                         GDALEDTComponentRelease()                    */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALEDTComponentH.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALEDTComponentRelease(GDALEDTComponentH hComp)
{
    delete hComp;
}

/************************************************************************/
/*                         GDALEDTComponentGetName()                    */
/************************************************************************/

/** Return the name.
 *
 * The returned pointer is valid until hComp is released.
 *
 * This is the same as the C++ method GDALEDTComponent::GetName().
 */
const char * GDALEDTComponentGetName(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1( hComp, __func__, nullptr );
    return hComp->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                       GDALEDTComponentGetOffset()                    */
/************************************************************************/

/** Return the offset (in bytes) of the component in the compound data type.
 *
 * This is the same as the C++ method GDALEDTComponent::GetOffset().
 */
size_t GDALEDTComponentGetOffset(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1( hComp, __func__, 0 );
    return hComp->m_poImpl->GetOffset();
}

/************************************************************************/
/*                       GDALEDTComponentGetType()                      */
/************************************************************************/

/** Return the data type of the component.
 *
 * This is the same as the C++ method GDALEDTComponent::GetType().
 */
GDALExtendedDataTypeH GDALEDTComponentGetType(GDALEDTComponentH hComp)
{
    VALIDATE_POINTER1( hComp, __func__, nullptr );
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hComp->m_poImpl->GetType()));
}

/************************************************************************/
/*                           GDALGroupRelease()                         */
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
/*                           GDALGroupGetName()                         */
/************************************************************************/

/** Return the name of the group.
 *
 * The returned pointer is valid until hGroup is released.
 *
 * This is the same as the C++ method GDALGroup::GetName().
 */
const char *GDALGroupGetName(GDALGroupH hGroup)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    return hGroup->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                         GDALGroupGetFullName()                       */
/************************************************************************/

/** Return the full name of the group.
 *
 * The returned pointer is valid until hGroup is released.
 *
 * This is the same as the C++ method GDALGroup::GetFullName().
 */
const char *GDALGroupGetFullName(GDALGroupH hGroup)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    return hGroup->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                          GDALGroupGetMDArrayNames()                  */
/************************************************************************/

/** Return the list of multidimensional array names contained in this group.
 *
 * This is the same as the C++ method GDALGroup::GetGroupNames().
 *
 * @return the array names, to be freed with CSLDestroy()
 */
char **GDALGroupGetMDArrayNames(GDALGroupH hGroup, CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    auto names = hGroup->m_poImpl->GetMDArrayNames(papszOptions);
    CPLStringList res;
    for( const auto& name: names )
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*                          GDALGroupOpenMDArray()                      */
/************************************************************************/

/** Open and return a multidimensional array.
 *
 * This is the same as the C++ method GDALGroup::OpenMDArray().
 *
 * @return the array, to be freed with GDALMDArrayRelease(), or nullptr.
 */
GDALMDArrayH GDALGroupOpenMDArray(GDALGroupH hGroup, const char* pszMDArrayName,
                                  CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszMDArrayName, __func__, nullptr );
    auto array = hGroup->m_poImpl->OpenMDArray(std::string(pszMDArrayName), papszOptions);
    if( !array )
        return nullptr;
    return new GDALMDArrayHS(array);
}

/************************************************************************/
/*                        GDALGroupGetGroupNames()                      */
/************************************************************************/

/** Return the list of sub-groups contained in this group.
 *
 * This is the same as the C++ method GDALGroup::GetGroupNames().
 *
 * @return the group names, to be freed with CSLDestroy()
 */
char **GDALGroupGetGroupNames(GDALGroupH hGroup, CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    auto names = hGroup->m_poImpl->GetGroupNames(papszOptions);
    CPLStringList res;
    for( const auto& name: names )
    {
        res.AddString(name.c_str());
    }
    return res.StealList();
}

/************************************************************************/
/*                           GDALGroupOpenGroup()                       */
/************************************************************************/

/** Open and return a sub-group.
 *
 * This is the same as the C++ method GDALGroup::OpenGroup().
 *
 * @return the sub-group, to be freed with GDALGroupRelease(), or nullptr.
 */
GDALGroupH GDALGroupOpenGroup(GDALGroupH hGroup, const char* pszSubGroupName,
                              CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszSubGroupName, __func__, nullptr );
    auto subGroup = hGroup->m_poImpl->OpenGroup(std::string(pszSubGroupName), papszOptions);
    if( !subGroup )
        return nullptr;
    return new GDALGroupHS(subGroup);
}

/************************************************************************/
/*                         GDALGroupGetDimensions()                     */
/************************************************************************/

/** Return the list of dimensions contained in this group and used by its
 * arrays.
 *
 * The returned array must be freed with GDALReleaseDimensions().  If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALDimensionRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALGroup::GetDimensions().
 *
 * @param hGroup Group.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how dimensions
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return an array of *pnCount dimensions.
 */
GDALDimensionH *GDALGroupGetDimensions(GDALGroupH hGroup, size_t* pnCount,
                                       CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    auto dims = hGroup->m_poImpl->GetDimensions(papszOptions);
    auto ret = static_cast<GDALDimensionH*>(
        CPLMalloc(sizeof(GDALDimensionH) * dims.size()));
    for( size_t i = 0; i < dims.size(); i++ )
    {
        ret[i] = new GDALDimensionHS(dims[i]);
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                          GDALGroupGetAttribute()                     */
/************************************************************************/

/** Return an attribute by its name.
 *
 * This is the same as the C++ method GDALIHasAttribute::GetAttribute()
 *
 * The returned attribute must be freed with GDALAttributeRelease().
 */
GDALAttributeH GDALGroupGetAttribute(GDALGroupH hGroup, const char* pszName)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    auto attr = hGroup->m_poImpl->GetAttribute(std::string(pszName));
    if( attr )
        return new GDALAttributeHS(attr);
    return nullptr;
}

/************************************************************************/
/*                         GDALGroupGetAttributes()                     */
/************************************************************************/

/** Return the list of attributes contained in this group.
 *
 * The returned array must be freed with GDALReleaseAttributes(). If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALAttributeRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALGroup::GetAttributes().
 *
 * @param hGroup Group.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return an array of *pnCount attributes.
 */
GDALAttributeH *GDALGroupGetAttributes(GDALGroupH hGroup, size_t* pnCount,
                                       CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    auto attrs = hGroup->m_poImpl->GetAttributes(papszOptions);
    auto ret = static_cast<GDALAttributeH*>(
        CPLMalloc(sizeof(GDALAttributeH) * attrs.size()));
    for( size_t i = 0; i < attrs.size(); i++ )
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
 * This is the same as the C++ method GDALGroup::GetStruturalInfo().
 */
CSLConstList GDALGroupGetStructuralInfo(GDALGroupH hGroup)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    return hGroup->m_poImpl->GetStructuralInfo();
}

/************************************************************************/
/*                         GDALReleaseAttributes()                      */
/************************************************************************/

/** Free the return of GDALGroupGetAttributes() or GDALMDArrayGetAttributes()
 *
 * @param attributes return pointer of above methods
 * @param nCount *pnCount value returned by above methods
 */
void GDALReleaseAttributes(GDALAttributeH* attributes, size_t nCount)
{
    for( size_t i = 0; i < nCount; i++ )
    {
        delete attributes[i];
    }
    CPLFree(attributes);
}

/************************************************************************/
/*                         GDALGroupCreateGroup()                       */
/************************************************************************/

/** Create a sub-group within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateGroup().
 *
 * @return the sub-group, to be freed with GDALGroupRelease(), or nullptr.
 */
GDALGroupH GDALGroupCreateGroup(GDALGroupH hGroup,
                                        const char* pszSubGroupName,
                                        CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszSubGroupName, __func__, nullptr );
    auto ret = hGroup->m_poImpl->CreateGroup(std::string(pszSubGroupName),
                                             papszOptions);
    if( !ret )
        return nullptr;
    return new GDALGroupHS(ret);
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
GDALDimensionH GDALGroupCreateDimension(GDALGroupH hGroup,
                                                const char* pszName,
                                                const char* pszType,
                                                const char* pszDirection,
                                                GUInt64 nSize,
                                                CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    auto ret = hGroup->m_poImpl->CreateDimension(std::string(pszName),
                                                 std::string(pszType ? pszType : ""),
                                                 std::string(pszDirection ? pszDirection: ""),
                                                 nSize,
                                                 papszOptions);
    if( !ret )
        return nullptr;
    return new GDALDimensionHS(ret);
}

/************************************************************************/
/*                      GDALGroupCreateMDArray()                        */
/************************************************************************/

/** Create a multidimensional array within a group.
 *
 * This is the same as the C++ method GDALGroup::CreateMDArray().
 *
 * @return the array, to be freed with GDALMDArrayRelease(), or nullptr.
 */
GDALMDArrayH GDALGroupCreateMDArray(GDALGroupH hGroup,
                                    const char* pszName,
                                    size_t nDimensions,
                                    GDALDimensionH* pahDimensions,
                                    GDALExtendedDataTypeH hEDT,
                                    CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    VALIDATE_POINTER1( hEDT, __func__, nullptr );
    std::vector<std::shared_ptr<GDALDimension>> dims;
    dims.reserve(nDimensions);
    for(size_t i = 0; i < nDimensions; i++)
        dims.push_back(pahDimensions[i]->m_poImpl);
    auto ret = hGroup->m_poImpl->CreateMDArray(std::string(pszName),
                                               dims,
                                               *(hEDT->m_poImpl),
                                               papszOptions);
    if( !ret )
        return nullptr;
    return new GDALMDArrayHS(ret);
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
GDALAttributeH GDALGroupCreateAttribute(GDALGroupH hGroup,
                                                const char* pszName,
                                                size_t nDimensions,
                                                const GUInt64* panDimensions,
                                                GDALExtendedDataTypeH hEDT,
                                                CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hGroup, __func__, nullptr );
    VALIDATE_POINTER1( hEDT, __func__, nullptr );
    std::vector<GUInt64> dims;
    dims.reserve(nDimensions);
    for(size_t i = 0; i < nDimensions; i++)
        dims.push_back(panDimensions[i]);
    auto ret = hGroup->m_poImpl->CreateAttribute(std::string(pszName),
                                               dims,
                                               *(hEDT->m_poImpl),
                                               papszOptions);
    if( !ret )
        return nullptr;
    return new GDALAttributeHS(ret);
}

/************************************************************************/
/*                        GDALMDArrayRelease()                          */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALMDArray.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALMDArrayRelease(GDALMDArrayH hMDArray)
{
    delete hMDArray;
}

/************************************************************************/
/*                        GDALMDArrayGetName()                          */
/************************************************************************/

/** Return array name.
 *
 * This is the same as the C++ method GDALMDArray::GetName()
 */
const char* GDALMDArrayGetName(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return hArray->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                    GDALMDArrayGetFullName()                          */
/************************************************************************/

/** Return array full name.
 *
 * This is the same as the C++ method GDALMDArray::GetFullName()
 */
const char* GDALMDArrayGetFullName(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return hArray->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                        GDALMDArrayGetName()                          */
/************************************************************************/

/** Return the total number of values in the array.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetTotalElementsCount()
 */
GUInt64 GDALMDArrayGetTotalElementsCount(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, 0 );
    return hArray->m_poImpl->GetTotalElementsCount();
}

/************************************************************************/
/*                        GDALMDArrayGetDimensionCount()                */
/************************************************************************/

/** Return the number of dimensions.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensionCount()
 */
size_t GDALMDArrayGetDimensionCount(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, 0 );
    return hArray->m_poImpl->GetDimensionCount();
}

/************************************************************************/
/*                        GDALMDArrayGetDimensions()                    */
/************************************************************************/

/** Return the dimensions of the array
 *
 * The returned array must be freed with GDALReleaseDimensions(). If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALDimensionRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensions()
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 *
 * @return an array of *pnCount dimensions.
 */
GDALDimensionH* GDALMDArrayGetDimensions(GDALMDArrayH hArray, size_t *pnCount)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    const auto& dims(hArray->m_poImpl->GetDimensions());
    auto ret = static_cast<GDALDimensionH*>(
        CPLMalloc(sizeof(GDALDimensionH) * dims.size()));
    for( size_t i = 0; i < dims.size(); i++ )
    {
        ret[i] = new GDALDimensionHS(dims[i]);
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                        GDALReleaseDimensions()                       */
/************************************************************************/

/** Free the return of GDALGroupGetDimensions() or GDALMDArrayGetDimensions()
 *
 * @param dims return pointer of above methods
 * @param nCount *pnCount value returned by above methods
 */
void GDALReleaseDimensions(GDALDimensionH* dims, size_t nCount)
{
    for( size_t i = 0; i < nCount; i++ )
    {
        delete dims[i];
    }
    CPLFree(dims);
}

/************************************************************************/
/*                        GDALMDArrayGetDataType()                     */
/************************************************************************/

/** Return the data type
 *
 * The return must be freed with GDALExtendedDataTypeRelease().
 */
GDALExtendedDataTypeH GDALMDArrayGetDataType(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hArray->m_poImpl->GetDataType()));
}

/************************************************************************/
/*                          GDALMDArrayRead()                           */
/************************************************************************/

/** Read part or totality of a multidimensional array.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Read()
 *
 * @return TRUE in case of success.
 */
int GDALMDArrayRead(GDALMDArrayH hArray,
                            const GUInt64* arrayStartIdx,
                            const size_t* count,
                            const GInt64* arrayStep,
                            const GPtrDiff_t* bufferStride,
                            GDALExtendedDataTypeH bufferDataType,
                            void* pDstBuffer,
                            const void* pDstBufferAllocStart,
                            size_t nDstBufferAllocSize)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    if( (arrayStartIdx == nullptr || count == nullptr) &&
         hArray->m_poImpl->GetDimensionCount() > 0 )
    {
        VALIDATE_POINTER1( arrayStartIdx, __func__, FALSE );
        VALIDATE_POINTER1( count, __func__, FALSE );
    }
    VALIDATE_POINTER1( bufferDataType, __func__, FALSE );
    VALIDATE_POINTER1( pDstBuffer, __func__, FALSE );
    // coverity[var_deref_model]
    return hArray->m_poImpl->Read(arrayStartIdx,
                                  count,
                                  arrayStep,
                                  bufferStride,
                                  *(bufferDataType->m_poImpl),
                                  pDstBuffer,
                                  pDstBufferAllocStart,
                                  nDstBufferAllocSize);
}

/************************************************************************/
/*                          GDALMDArrayWrite()                           */
/************************************************************************/

/** Write part or totality of a multidimensional array.
 *
 * This is the same as the C++ method GDALAbstractMDArray::Write()
 *
 * @return TRUE in case of success.
 */
int GDALMDArrayWrite(GDALMDArrayH hArray,
                            const GUInt64* arrayStartIdx,
                            const size_t* count,
                            const GInt64* arrayStep,
                            const GPtrDiff_t* bufferStride,
                            GDALExtendedDataTypeH bufferDataType,
                            const void* pSrcBuffer,
                            const void* pSrcBufferAllocStart,
                            size_t nSrcBufferAllocSize)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    if( (arrayStartIdx == nullptr || count == nullptr) &&
         hArray->m_poImpl->GetDimensionCount() > 0 )
    {
        VALIDATE_POINTER1( arrayStartIdx, __func__, FALSE );
        VALIDATE_POINTER1( count, __func__, FALSE );
    }
    VALIDATE_POINTER1( bufferDataType, __func__, FALSE );
    VALIDATE_POINTER1( pSrcBuffer, __func__, FALSE );
    // coverity[var_deref_model]
    return hArray->m_poImpl->Write(arrayStartIdx,
                                  count,
                                  arrayStep,
                                  bufferStride,
                                  *(bufferDataType->m_poImpl),
                                  pSrcBuffer,
                                  pSrcBufferAllocStart,
                                  nSrcBufferAllocSize);
}

/************************************************************************/
/*                         GDALMDArrayGetAttribute()                    */
/************************************************************************/

/** Return an attribute by its name.
 *
 * This is the same as the C++ method GDALIHasAttribute::GetAttribute()
 *
 * The returned attribute must be freed with GDALAttributeRelease().
 */
GDALAttributeH GDALMDArrayGetAttribute(GDALMDArrayH hArray, const char* pszName)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    auto attr = hArray->m_poImpl->GetAttribute(std::string(pszName));
    if( attr )
        return new GDALAttributeHS(attr);
    return nullptr;
}

/************************************************************************/
/*                        GDALMDArrayGetAttributes()                    */
/************************************************************************/

/** Return the list of attributes contained in this array.
 *
 * The returned array must be freed with GDALReleaseAttributes(). If only the array itself needs to be
 * freed, CPLFree() should be called (and GDALAttributeRelease() on
 * individual array members).
 *
 * This is the same as the C++ method GDALMDArray::GetAttributes().
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param papszOptions Driver specific options determining how attributes
 * should be retrieved. Pass nullptr for default behaviour.
 *
 * @return an array of *pnCount attributes.
 */
GDALAttributeH *GDALMDArrayGetAttributes(GDALMDArrayH hArray, size_t* pnCount,
                                         CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    auto attrs = hArray->m_poImpl->GetAttributes(papszOptions);
    auto ret = static_cast<GDALAttributeH*>(
        CPLMalloc(sizeof(GDALAttributeH) * attrs.size()));
    for( size_t i = 0; i < attrs.size(); i++ )
    {
        ret[i] = new GDALAttributeHS(attrs[i]);
    }
    *pnCount = attrs.size();
    return ret;
}

/************************************************************************/
/*                       GDALMDArrayCreateAttribute()                   */
/************************************************************************/

/** Create a attribute within an array.
 *
 * This is the same as the C++ method GDALMDArray::CreateAttribute().
 *
 * @return the attribute, to be freed with GDALAttributeRelease(), or nullptr.
 */
GDALAttributeH GDALMDArrayCreateAttribute(GDALMDArrayH hArray,
                                                const char* pszName,
                                                size_t nDimensions,
                                                const GUInt64* panDimensions,
                                                GDALExtendedDataTypeH hEDT,
                                                CSLConstList papszOptions)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    VALIDATE_POINTER1( hEDT, __func__, nullptr );
    std::vector<GUInt64> dims;
    dims.reserve(nDimensions);
    for(size_t i = 0; i < nDimensions; i++)
        dims.push_back(panDimensions[i]);
    auto ret = hArray->m_poImpl->CreateAttribute(std::string(pszName),
                                               dims,
                                               *(hEDT->m_poImpl),
                                               papszOptions);
    if( !ret )
        return nullptr;
    return new GDALAttributeHS(ret);
}

/************************************************************************/
/*                       GDALMDArrayGetRawNoDataValue()                 */
/************************************************************************/

/** Return the nodata value as a "raw" value.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * The returned value should not be modified or freed.
 *
 * This is the same as the ++ method GDALMDArray::GetRawNoDataValue().
 *
 * @return nullptr or a pointer to GetDataType().GetSize() bytes.
 */
const void *GDALMDArrayGetRawNoDataValue(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return hArray->m_poImpl->GetRawNoDataValue();
}

/************************************************************************/
/*                      GDALMDArrayGetNoDataValueAsDouble()             */
/************************************************************************/

/** Return the nodata value as a double.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * This is the same as the C++ method GDALMDArray::GetNoDataValueAsDouble().
 *
 * @param hArray Array handle.
 * @param pbHasNoDataValue Pointer to a output boolean that will be set to true if
 * a nodata value exists and can be converted to double. Might be nullptr.
 *
 * @return the nodata value as a double. A 0.0 value might also indicate the
 * absence of a nodata value or an error in the conversion (*pbHasNoDataValue will be
 * set to false then).
 */
double GDALMDArrayGetNoDataValueAsDouble(GDALMDArrayH hArray,
                                         int* pbHasNoDataValue)
{
    VALIDATE_POINTER1( hArray, __func__, 0 );
    bool bHasNodataValue = false;
    double ret = hArray->m_poImpl->GetNoDataValueAsDouble(&bHasNodataValue);
    if( pbHasNoDataValue )
        *pbHasNoDataValue = bHasNodataValue;
    return ret;
}

/************************************************************************/
/*                     GDALMDArraySetRawNoDataValue()                   */
/************************************************************************/

/** Set the nodata value as a "raw" value.
 *
 * The value passed might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr whose size in
 * bytes is GetDataType().GetSize() must be passed.
 *
 * This is the same as the C++ method GDALMDArray::SetRawNoDataValue(const void*).
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetRawNoDataValue(GDALMDArrayH hArray, const void* pNoData)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetRawNoDataValue(pNoData);
}

/************************************************************************/
/*                   GDALMDArraySetNoDataValueAsDouble()                */
/************************************************************************/

/** Set the nodata value as a double.
 *
 * If the natural data type of the attribute/array is not double, type conversion
 * will occur to the type returned by GetDataType().
 *
 * This is the same as the C++ method GDALMDArray::SetNoDataValue(double).
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetNoDataValueAsDouble(GDALMDArrayH hArray,
                                      double dfNoDataValue)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetNoDataValue(dfNoDataValue);
}

/************************************************************************/
/*                          GDALMDArraySetScale()                       */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetScale().
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetScale(GDALMDArrayH hArray, double dfScale)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetScale(dfScale);
}

/************************************************************************/
/*                          GDALMDArraySetOffset()                       */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::SetOffset().
 *
 * @return TRUE in case of success.
 */
int GDALMDArraySetOffset(GDALMDArrayH hArray, double dfOffset)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetOffset(dfOffset);
}

/************************************************************************/
/*                          GDALMDArrayGetScale()                       */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::GetScale().
 *
 * @return the scale value
 */
double GDALMDArrayGetScale(GDALMDArrayH hArray, int* pbHasValue)
{
    VALIDATE_POINTER1( hArray, __func__, 0.0 );
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetScale(&bHasValue);
    if( pbHasValue )
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                          GDALMDArrayGetOffset()                      */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C++ method GDALMDArray::GetOffset().
 *
 * @return the scale value
 */
double GDALMDArrayGetOffset(GDALMDArrayH hArray, int* pbHasValue)
{
    VALIDATE_POINTER1( hArray, __func__, 0.0 );
    bool bHasValue = false;
    double dfRet = hArray->m_poImpl->GetOffset(&bHasValue);
    if( pbHasValue )
        *pbHasValue = bHasValue;
    return dfRet;
}

/************************************************************************/
/*                      GDALMDArrayGetBlockSize()                       */
/************************************************************************/

/** Return the "natural" block size of the array along all dimensions.
 *
 * Some drivers might organize the array in tiles/blocks and reading/writing
 * aligned on those tile/block boundaries will be more efficient.
 *
 * The returned number of elements in the vector is the same as
 * GetDimensionCount(). A value of 0 should be interpretated as no hint regarding
 * the natural block size along the considered dimension.
 * "Flat" arrays will typically return a vector of values set to 0.
 *
 * The default implementation will return a vector of values set to 0.
 *
 * This method is used by GetProcessingChunkSize().
 *
 * Pedantic note: the returned type is GUInt64, so in the highly unlikeley theoretical
 * case of a 32-bit platform, this might exceed its size_t allocation capabilities.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetBlockSize().
 *
 * @return the block size, in number of elemnts along each dimension.
 */
GUInt64 *GDALMDArrayGetBlockSize(GDALMDArrayH hArray, size_t *pnCount)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    auto res = hArray->m_poImpl->GetBlockSize();
    auto ret = static_cast<GUInt64*>(
        CPLMalloc(sizeof(GUInt64) * res.size()));
    for( size_t i = 0; i < res.size(); i++ )
    {
        ret[i] = res[i];
    }
    *pnCount = res.size();
    return ret;
}

/***********************************************************************/
/*                   GDALMDArrayGetProcessingChunkSize()               */
/************************************************************************/

/** \brief Return an optimal chunk size for read/write oerations, given the natural
 * block size and memory constraints specified.
 *
 * This method will use GetBlockSize() to define a chunk whose dimensions are
 * multiple of those returned by GetBlockSize() (unless the block define by
 * GetBlockSize() is larger than nMaxChunkMemory, in which case it will be
 * returned by this method).
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetProcessingChunkSize().
 *
 * @param hArray Array.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @param nMaxChunkMemory Maximum amount of memory, in bytes, to use for the chunk.
 *
 * @return the chunk size, in number of elemnts along each dimension.
 */

size_t *GDALMDArrayGetProcessingChunkSize(GDALMDArrayH hArray, size_t *pnCount,
                                size_t nMaxChunkMemory)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    auto res = hArray->m_poImpl->GetProcessingChunkSize(nMaxChunkMemory);
    auto ret = static_cast<size_t*>(
        CPLMalloc(sizeof(size_t) * res.size()));
    for( size_t i = 0; i < res.size(); i++ )
    {
        ret[i] = res[i];
    }
    *pnCount = res.size();
    return ret;
}

/************************************************************************/
/*                     GDALMDArrayGetStructuralInfo()                   */
/************************************************************************/

/** Return structural information on the array.
 *
 * This may be the compression, etc..
 * 
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 * 
 * This is the same as the C++ method GDALMDArray::GetStruturalInfo().
 */
CSLConstList GDALMDArrayGetStructuralInfo(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return hArray->m_poImpl->GetStructuralInfo();
}

/************************************************************************/
/*                        GDALMDArrayGetView()                          */
/************************************************************************/

/** Return a view of the array using slicing or field access.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::GetView().
 */
GDALMDArrayH GDALMDArrayGetView(GDALMDArrayH hArray, const char* pszViewExpr)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    VALIDATE_POINTER1( pszViewExpr, __func__, nullptr );
    auto sliced = hArray->m_poImpl->GetView(std::string(pszViewExpr));
    if( !sliced )
        return nullptr;
    return new GDALMDArrayHS(sliced);
}

/************************************************************************/
/*                       GDALMDArrayTranspose()                         */
/************************************************************************/

/** Return a view of the array whose axis have been reordered.
 *
 * The returned object should be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALMDArray::Transpose().
 */
GDALMDArrayH GDALMDArrayTranspose(GDALMDArrayH hArray,
                                            size_t nNewAxisCount,
                                            const int *panMapNewAxisToOldAxis)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    std::vector<int> anMapNewAxisToOldAxis(nNewAxisCount);
    if( nNewAxisCount )
    {
        memcpy(&anMapNewAxisToOldAxis[0], panMapNewAxisToOldAxis,
               nNewAxisCount * sizeof(int));
    }
    auto reordered = hArray->m_poImpl->Transpose(anMapNewAxisToOldAxis);
    if( !reordered )
        return nullptr;
    return new GDALMDArrayHS(reordered);
}

/************************************************************************/
/*                      GDALMDArraySetUnit()                            */
/************************************************************************/

/** Set the variable unit.
 * 
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 * 
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArraySetUnit()
 *
 * @param hArray array.
 * @param pszUnit unit name.
 * @return TRUE in case of success.
 */
int GDALMDArraySetUnit(GDALMDArrayH hArray, const char* pszUnit)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetUnit(pszUnit ? pszUnit : "");
}

/************************************************************************/
/*                      GDALMDArrayGetUnit()                            */
/************************************************************************/

/** Return the array unit.
 * 
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 * 
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 * 
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 * 
 * This is the same as the C++ method GDALMDArray::GetUnit().
 */
const char* GDALMDArrayGetUnit(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return hArray->m_poImpl->GetUnit().c_str();
}


/************************************************************************/
/*                      GDALMDArrayGetSpatialRef()                      */
/************************************************************************/

/** Assign a spatial reference system object to the the array.
 * 
 * This is the same as the C++ method GDALMDArray::SetSpatialRef().
 * @return TRUE in case of success.
 */
int GDALMDArraySetSpatialRef(GDALMDArrayH hArray,
                             OGRSpatialReferenceH hSRS)
{
    VALIDATE_POINTER1( hArray, __func__, FALSE );
    return hArray->m_poImpl->SetSpatialRef(OGRSpatialReference::FromHandle(hSRS));
}

/************************************************************************/
/*                      GDALMDArrayGetSpatialRef()                      */
/************************************************************************/

/** Return the spatial reference system object associated with the array.
 * 
 * This is the same as the C++ method GDALMDArray::GetSpatialRef().
 *
 * The returned object must be freed with OSRDestroySpatialReference().
 */
OGRSpatialReferenceH GDALMDArrayGetSpatialRef(GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    auto poSRS = hArray->m_poImpl->GetSpatialRef();
    return poSRS ? OGRSpatialReference::ToHandle(poSRS->Clone()) : nullptr;
}

/************************************************************************/
/*                        GDALAttributeRelease()                        */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALAttribute.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALAttributeRelease(GDALAttributeH hAttr)
{
    delete hAttr;
}

/************************************************************************/
/*                        GDALAttributeGetName()                        */
/************************************************************************/

/** Return the name of the attribute.
 *
 * The returned pointer is valid until hAttr is released.
 *
 * This is the same as the C++ method GDALAttribute::GetName().
 */
const char* GDALAttributeGetName(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    return hAttr->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                      GDALAttributeGetFullName()                      */
/************************************************************************/

/** Return the full name of the attribute.
 *
 * The returned pointer is valid until hAttr is released.
 *
 * This is the same as the C++ method GDALAttribute::GetFullName().
 */
const char* GDALAttributeGetFullName(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    return hAttr->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                   GDALAttributeGetTotalElementsCount()               */
/************************************************************************/

/** Return the total number of values in the attribute.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetTotalElementsCount()
 */
GUInt64 GDALAttributeGetTotalElementsCount(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, 0 );
    return hAttr->m_poImpl->GetTotalElementsCount();
}

/************************************************************************/
/*                    GDALAttributeGetDimensionCount()                */
/************************************************************************/

/** Return the number of dimensions.
 *
 * This is the same as the C++ method GDALAbstractMDArray::GetDimensionCount()
 */
size_t GDALAttributeGetDimensionCount(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, 0 );
    return hAttr->m_poImpl->GetDimensionCount();
}

/************************************************************************/
/*                       GDALAttributeGetDimensionsSize()                */
/************************************************************************/

/** Return the dimension sizes of the attribute.
 *
 * The returned array must be freed with CPLFree()
 *
 * @param hAttr Attribute.
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 *
 * @return an array of *pnCount values.
 */
GUInt64* GDALAttributeGetDimensionsSize(GDALAttributeH hAttr, size_t *pnCount)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    const auto& dims = hAttr->m_poImpl->GetDimensions();
    auto ret = static_cast<GUInt64*>(
        CPLMalloc(sizeof(GUInt64) * dims.size()));
    for( size_t i = 0; i < dims.size(); i++ )
    {
        ret[i] = dims[i]->GetSize();
    }
    *pnCount = dims.size();
    return ret;
}

/************************************************************************/
/*                       GDALAttributeGetDataType()                     */
/************************************************************************/

/** Return the data type
 *
 * The return must be freed with GDALExtendedDataTypeRelease().
 */
GDALExtendedDataTypeH GDALAttributeGetDataType(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    return new GDALExtendedDataTypeHS(
        new GDALExtendedDataType(hAttr->m_poImpl->GetDataType()));
}

/************************************************************************/
/*                       GDALAttributeReadAsRaw()                       */
/************************************************************************/

/** Return the raw value of an attribute.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsRaw().
 *
 * The returned buffer must be freed with GDALAttributeFreeRawResult()
 *
 * @param hAttr Attribute.
 * @param pnSize Pointer to the number of bytes returned. Must NOT be NULL.
 *
 * @return a buffer of *pnSize bytes.
 */
GByte *GDALAttributeReadAsRaw(GDALAttributeH hAttr, size_t *pnSize)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    VALIDATE_POINTER1( pnSize, __func__, nullptr );
    auto res(hAttr->m_poImpl->ReadAsRaw());
    *pnSize = res.size();
    auto ret = res.StealData();
    if( !ret )
    {
        *pnSize = 0;
        return nullptr;
    }
    return ret;
}

/************************************************************************/
/*                       GDALAttributeFreeRawResult()                   */
/************************************************************************/

/** Free the return of GDALAttributeAsRaw()
 */
void GDALAttributeFreeRawResult(GDALAttributeH hAttr, GByte* raw,
                                CPL_UNUSED size_t nSize)
{
    VALIDATE_POINTER0( hAttr, __func__ );
    if( raw )
    {
        const auto dt(hAttr->m_poImpl->GetDataType());
        const auto nDTSize(dt.GetSize());
        GByte* pabyPtr = raw;
        const auto nEltCount(hAttr->m_poImpl->GetTotalElementsCount());
        CPLAssert( nSize == nDTSize * nEltCount );
        for( size_t i = 0; i < nEltCount; ++i )
        {
            dt.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
        CPLFree(raw);
    }
}

/************************************************************************/
/*                       GDALAttributeReadAsString()                    */
/************************************************************************/

/** Return the value of an attribute as a string.
 *
 * The returned string should not be freed, and its lifetime does not
 * excess a next call to ReadAsString() on the same object, or the deletion
 * of the object itself.
 *
 * This function will only return the first element if there are several.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsString()
 *
 * @return a string, or nullptr.
 */
const char* GDALAttributeReadAsString(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    return hAttr->m_poImpl->ReadAsString();
}

/************************************************************************/
/*                      GDALAttributeReadAsInt()                        */
/************************************************************************/

/** Return the value of an attribute as a integer.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can be converted to integer.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsInt()
 *
 * @return a integer, or INT_MIN in case of error.
 */
int GDALAttributeReadAsInt(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, 0 );
    return hAttr->m_poImpl->ReadAsInt();
}

/************************************************************************/
/*                       GDALAttributeReadAsDouble()                    */
/************************************************************************/

/** Return the value of an attribute as a double.
 *
 * This function will only return the first element if there are several.
 *
 * It can fail if its value can be converted to double.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsDoubl()
 *
 * @return a double value.
 */
double GDALAttributeReadAsDouble(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, 0 );
    return hAttr->m_poImpl->ReadAsDouble();
}

/************************************************************************/
/*                     GDALAttributeReadAsStringArray()                 */
/************************************************************************/

/** Return the value of an attribute as an array of strings.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsStringArray()
 *
 * The return value must be freed with CSLDestroy().
 */
char **GDALAttributeReadAsStringArray(GDALAttributeH hAttr)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    return hAttr->m_poImpl->ReadAsStringArray().StealList();
}

/************************************************************************/
/*                     GDALAttributeReadAsIntArray()                    */
/************************************************************************/

/** Return the value of an attribute as an array of integers.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsIntArray()
 *
 * @param hAttr Attribute
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return array to be freed with CPLFree(), or nullptr.
 */
int *GDALAttributeReadAsIntArray(GDALAttributeH hAttr, size_t* pnCount)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    *pnCount = 0;
    auto tmp(hAttr->m_poImpl->ReadAsIntArray());
    if( tmp.empty() )
        return nullptr;
    auto ret = static_cast<int*>(VSI_MALLOC2_VERBOSE(tmp.size(),
                                                        sizeof(int)));
    if( !ret )
        return nullptr;
    memcpy(ret, tmp.data(), tmp.size() * sizeof(int));
    *pnCount = tmp.size();
    return ret;
}

/************************************************************************/
/*                     GDALAttributeReadAsDoubleArray()                 */
/************************************************************************/

/** Return the value of an attribute as an array of doubles.
 *
 * This is the same as the C++ method GDALAttribute::ReadAsDoubleArray()
 *
 * @param hAttr Attribute
 * @param pnCount Pointer to the number of values returned. Must NOT be NULL.
 * @return array to be freed with CPLFree(), or nullptr.
 */
double *GDALAttributeReadAsDoubleArray(GDALAttributeH hAttr, size_t* pnCount)
{
    VALIDATE_POINTER1( hAttr, __func__, nullptr );
    VALIDATE_POINTER1( pnCount, __func__, nullptr );
    *pnCount = 0;
    auto tmp(hAttr->m_poImpl->ReadAsDoubleArray());
    if( tmp.empty() )
        return nullptr;
    auto ret = static_cast<double*>(VSI_MALLOC2_VERBOSE(tmp.size(),
                                                        sizeof(double)));
    if( !ret )
        return nullptr;
    memcpy(ret, tmp.data(), tmp.size() * sizeof(double));
    *pnCount = tmp.size();
    return ret;
}

/************************************************************************/
/*                     GDALAttributeWriteRaw()                          */
/************************************************************************/

/** Write an attribute from raw values expressed in GetDataType()
 *
 * The values should be provided in the type of GetDataType() and there should
 * be exactly GetTotalElementsCount() of them.
 * If GetDataType() is a string, each value should be a char* pointer.
 *
 * This is the same as the C++ method GDALAttribute::Write(const void*, size_t).
 *
 * @param hAttr Attribute
 * @param pabyValue Buffer of nLen bytes.
 * @param nLength Size of pabyValue in bytes. Should be equal to
 *             GetTotalElementsCount() * GetDataType().GetSize()
 * @return TRUE in case of success.
 */
int GDALAttributeWriteRaw(GDALAttributeH hAttr, const void* pabyValue, size_t nLength)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->Write(pabyValue, nLength);
}

/************************************************************************/
/*                     GDALAttributeWriteString()                       */
/************************************************************************/

/** Write an attribute from a string value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::Write(const char*)
 *
 * @param hAttr Attribute
 * @param pszVal Pointer to a string.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteString(GDALAttributeH hAttr, const char* pszVal)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->Write(pszVal);
}

/************************************************************************/
/*                        GDALAttributeWriteInt()                       */
/************************************************************************/

/** Write an attribute from a integer value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::WriteInt()
 *
 * @param hAttr Attribute
 * @param nVal Value.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteInt(GDALAttributeH hAttr, int nVal)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->WriteInt(nVal);
}

/************************************************************************/
/*                        GDALAttributeWriteDouble()                    */
/************************************************************************/

/** Write an attribute from a double value.
 *
 * Type conversion will be performed if needed. If the attribute contains
 * multiple values, only the first one will be updated.
 *
 * This is the same as the C++ method GDALAttribute::Write(double);
 *
 * @param hAttr Attribute
 * @param dfVal Value.
 *
 * @return TRUE in case of success.
 */
int GDALAttributeWriteDouble(GDALAttributeH hAttr, double dfVal)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->Write(dfVal);
}

/************************************************************************/
/*                       GDALAttributeWriteStringArray()                */
/************************************************************************/

/** Write an attribute from an array of strings.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(CSLConstList)
 *
 * @param hAttr Attribute
 * @param papszValues Array of strings.
 * @return TRUE in case of success.
 */
int GDALAttributeWriteStringArray(GDALAttributeH hAttr, CSLConstList papszValues)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->Write(papszValues);
}


/************************************************************************/
/*                       GDALAttributeWriteDoubleArray()                */
/************************************************************************/

/** Write an attribute from an array of double.
 *
 * Type conversion will be performed if needed.
 *
 * Exactly GetTotalElementsCount() strings must be provided
 *
 * This is the same as the C++ method GDALAttribute::Write(const double *, size_t)
 *
 * @param hAttr Attribute
 * @param padfValues Array of double.
 * @param nCount Should be equal to GetTotalElementsCount().
 * @return TRUE in case of success.
 */
int GDALAttributeWriteDoubleArray(GDALAttributeH hAttr,
                                  const double* padfValues, size_t nCount)
{
    VALIDATE_POINTER1( hAttr, __func__, FALSE );
    return hAttr->m_poImpl->Write(padfValues, nCount);
}

/************************************************************************/
/*                        GDALDimensionRelease()                        */
/************************************************************************/

/** Release the GDAL in-memory object associated with a GDALDimension.
 *
 * Note: when applied on a object coming from a driver, this does not
 * destroy the object in the file, database, etc...
 */
void GDALDimensionRelease(GDALDimensionH hDim)
{
    delete hDim;
}

/************************************************************************/
/*                        GDALDimensionGetName()                        */
/************************************************************************/

/** Return dimension name.
 *
 * This is the same as the C++ method GDALDimension::GetName()
 */
const char *GDALDimensionGetName(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, nullptr );
    return hDim->m_poImpl->GetName().c_str();
}

/************************************************************************/
/*                      GDALDimensionGetFullName()                      */
/************************************************************************/

/** Return dimension full name.
 *
 * This is the same as the C++ method GDALDimension::GetFullName()
 */
const char *GDALDimensionGetFullName(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, nullptr );
    return hDim->m_poImpl->GetFullName().c_str();
}

/************************************************************************/
/*                        GDALDimensionGetType()                        */
/************************************************************************/

/** Return dimension type.
 *
 * This is the same as the C++ method GDALDimension::GetType()
 */
const char *GDALDimensionGetType(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, nullptr );
    return hDim->m_poImpl->GetType().c_str();
}

/************************************************************************/
/*                     GDALDimensionGetDirection()                      */
/************************************************************************/

/** Return dimension direction.
 *
 * This is the same as the C++ method GDALDimension::GetDirection()
 */
const char *GDALDimensionGetDirection(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, nullptr );
    return hDim->m_poImpl->GetDirection().c_str();
}

/************************************************************************/
/*                        GDALDimensionGetSize()                        */
/************************************************************************/

/** Return the size, that is the number of values along the dimension.
 *
 * This is the same as the C++ method GDALDimension::GetSize()
 */
GUInt64 GDALDimensionGetSize(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, 0 );
    return hDim->m_poImpl->GetSize();
}

/************************************************************************/
/*                     GDALDimensionGetIndexingVariable()               */
/************************************************************************/

/** Return the variable that is used to index the dimension (if there is one).
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * The returned value should be freed with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALDimension::GetIndexingVariable()
 */
GDALMDArrayH GDALDimensionGetIndexingVariable(GDALDimensionH hDim)
{
    VALIDATE_POINTER1( hDim, __func__, nullptr );
    auto var(hDim->m_poImpl->GetIndexingVariable());
    if( !var )
        return nullptr;
    return new GDALMDArrayHS(var);
}

/************************************************************************/
/*                      GDALDimensionSetIndexingVariable()              */
/************************************************************************/

/** Set the variable that is used to index the dimension.
 *
 * This is the array, typically one-dimensional, describing the values taken
 * by the dimension.
 *
 * This is the same as the C++ method GDALDimension::SetIndexingVariable()
 *
 * @return TRUE in case of success.
 */
int GDALDimensionSetIndexingVariable(GDALDimensionH hDim, GDALMDArrayH hArray)
{
    VALIDATE_POINTER1( hDim, __func__, FALSE );
    return hDim->m_poImpl->SetIndexingVariable(hArray ? hArray->m_poImpl : nullptr);
}

/************************************************************************/
/*                       GDALDatasetGetRootGroup()                      */
/************************************************************************/

/** Return the root GDALGroup of this dataset.
 *
 * Only valid for multidimensional datasets.
 *
 * The returned value must be freed with GDALGroupRelease().
 *
 * This is the same as the C++ method GDALDataset::GetRootGroup().
 *
 * @since GDAL 3.1
 */
GDALGroupH GDALDatasetGetRootGroup(GDALDatasetH hDS)
{
    VALIDATE_POINTER1( hDS, __func__, nullptr );
    auto poGroup(GDALDataset::FromHandle(hDS)->GetRootGroup());
    return poGroup ? new GDALGroupHS(poGroup) : nullptr;
}


/************************************************************************/
/*                      GDALRasterBandAsMDArray()                        */
/************************************************************************/

/** Return a view of this raster band as a 2D multidimensional GDALMDArray.
 *
 * The band must be linked to a GDALDataset. If this dataset is not already
 * marked as shared, it will be, so that the returned array holds a reference
 * to it.
 *
 * If the dataset has a geotransform attached, the X and Y dimensions of the
 * returned array will have an associated indexing variable.
 *
 * The returned pointer must be released with GDALMDArrayRelease().
 *
 * This is the same as the C++ method GDALRasterBand::AsMDArray().
 *
 * @return a new array, or NULL.
 *
 * @since GDAL 3.1
 */
GDALMDArrayH GDALRasterBandAsMDArray(GDALRasterBandH hBand)
{
    VALIDATE_POINTER1( hBand, __func__, nullptr );
    auto poArray(GDALRasterBand::FromHandle(hBand)->AsMDArray());
    if( !poArray )
        return nullptr;
    return new GDALMDArrayHS(poArray);
}


/************************************************************************/
/*                       GDALMDArrayAsClassicDataset()                  */
/************************************************************************/

/** Return a view of this array as a "classic" GDALDataset (ie 2D)
 *
 * Only 2D or more arrays are supported.
 *
 * In the case of > 2D arrays, additional dimensions will be represented as
 * raster bands.
 *
 * The "reverse" method is GDALRasterBand::AsMDArray().
 *
 * This is the same as the C++ method GDALMDArray::AsClassicDataset().
 *
 * @param hArray Array.
 * @param iXDim Index of the dimension that will be used as the X/width axis.
 * @param iYDim Index of the dimension that will be used as the Y/height axis.
 * @return a new GDALDataset that must be freed with GDALClose(), or nullptr
 */
GDALDatasetH GDALMDArrayAsClassicDataset(GDALMDArrayH hArray,
                                         size_t iXDim, size_t iYDim)
{
    VALIDATE_POINTER1( hArray, __func__, nullptr );
    return GDALDataset::ToHandle(
        hArray->m_poImpl->AsClassicDataset(iXDim, iYDim));
}


//! @cond Doxygen_Suppress

GDALAttributeString::GDALAttributeString(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osValue):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_osValue(osValue)
{}

const std::vector<std::shared_ptr<GDALDimension>>& GDALAttributeString::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALAttributeString::GetDataType() const
{
    return m_dt;
}

bool GDALAttributeString::IRead(const GUInt64* ,
            const size_t* ,
            const GInt64* ,
            const GPtrDiff_t* ,
            const GDALExtendedDataType& bufferDataType,
            void* pDstBuffer) const
{
    if( bufferDataType.GetClass() != GEDTC_STRING )
        return false;
    char* pszStr = static_cast<char*>(VSIMalloc(m_osValue.size() + 1));
    if( !pszStr ) 
        return false;
    memcpy(pszStr, m_osValue.c_str(), m_osValue.size() + 1);
    *static_cast<char**>(pDstBuffer) = pszStr;
    return true;
}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  double dfValue):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_dt(GDALExtendedDataType::Create(GDT_Float64)),
        m_dfValue(dfValue)
{}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  int nValue):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_dt(GDALExtendedDataType::Create(GDT_Int32)),
        m_nValue(nValue)
{}

GDALAttributeNumeric::GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  const std::vector<GUInt32>& anValues):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_dt(GDALExtendedDataType::Create(GDT_UInt32)),
        m_anValuesUInt32(anValues)
{
    m_dims.push_back(std::make_shared<GDALDimension>(
        std::string(), "dim0", std::string(), std::string(), m_anValuesUInt32.size()));
}

const std::vector<std::shared_ptr<GDALDimension>>& GDALAttributeNumeric::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALAttributeNumeric::GetDataType() const
{
    return m_dt;
}

bool GDALAttributeNumeric::IRead(const GUInt64* arrayStartIdx,
                                 const size_t* count,
                                 const GInt64* arrayStep,
                                 const GPtrDiff_t* bufferStride,
                                 const GDALExtendedDataType& bufferDataType,
                                 void* pDstBuffer) const
{
    if( m_dims.empty() )
    {
        if( m_dt.GetNumericDataType() == GDT_Float64 )
            GDALExtendedDataType::CopyValue(&m_dfValue, m_dt, pDstBuffer, bufferDataType);
        else
        {
            CPLAssert( m_dt.GetNumericDataType() == GDT_Int32 );
            GDALExtendedDataType::CopyValue(&m_nValue, m_dt, pDstBuffer, bufferDataType);
        }
    }
    else
    {
        CPLAssert( m_dt.GetNumericDataType() == GDT_UInt32 );
        GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
        for(size_t i = 0; i < count[0]; ++i )
        {
            GDALExtendedDataType::CopyValue(
                &m_anValuesUInt32[static_cast<size_t>(arrayStartIdx[0] + i * arrayStep[0])],
                m_dt,
                pabyDstBuffer,
                bufferDataType);
            pabyDstBuffer += bufferDataType.GetSize() * bufferStride[0];
        }
    }
    return true;
}

GDALMDArrayRegularlySpaced::GDALMDArrayRegularlySpaced(
                const std::string& osParentName,
                const std::string& osName,
                const std::shared_ptr<GDALDimension>& poDim,
                double dfStart, double dfIncrement,
                double dfOffsetInIncrement):
    GDALAbstractMDArray(osParentName, osName),
    GDALMDArray(osParentName, osName),
    m_dfStart(dfStart),
    m_dfIncrement(dfIncrement),
    m_dfOffsetInIncrement(dfOffsetInIncrement),
    m_dims{poDim}
{}

const std::vector<std::shared_ptr<GDALDimension>>& GDALMDArrayRegularlySpaced::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALMDArrayRegularlySpaced::GetDataType() const
{
    return m_dt;
}

std::vector<std::shared_ptr<GDALAttribute>> GDALMDArrayRegularlySpaced::GetAttributes(CSLConstList) const
{
    return m_attributes;
}

void GDALMDArrayRegularlySpaced::AddAttribute(const std::shared_ptr<GDALAttribute>& poAttr)
{
    m_attributes.emplace_back(poAttr);
}

bool GDALMDArrayRegularlySpaced::IRead(const GUInt64* arrayStartIdx,
                                       const size_t* count,
                                       const GInt64* arrayStep,
                                       const GPtrDiff_t* bufferStride,
                                       const GDALExtendedDataType& bufferDataType,
                                       void* pDstBuffer) const
{
    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    for( size_t i = 0; i < count[0]; i++ )
    {
        const double dfVal = m_dfStart +
            (arrayStartIdx[0] + i * arrayStep[0] + m_dfOffsetInIncrement) * m_dfIncrement;
        GDALExtendedDataType::CopyValue(&dfVal, m_dt,
                                        pabyDstBuffer, bufferDataType);
        pabyDstBuffer += bufferStride[0] * bufferDataType.GetSize();
    }
    return true;
}

GDALDimensionWeakIndexingVar::GDALDimensionWeakIndexingVar(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osType,
                  const std::string& osDirection,
                  GUInt64 nSize) :
        GDALDimension(osParentName, osName, osType, osDirection, nSize)
{}

std::shared_ptr<GDALMDArray> GDALDimensionWeakIndexingVar::GetIndexingVariable() const
{
    return m_poIndexingVariable.lock();
}

bool GDALDimensionWeakIndexingVar::SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable)
{
    m_poIndexingVariable = poIndexingVariable;
    return true;
}

//! @endcond
