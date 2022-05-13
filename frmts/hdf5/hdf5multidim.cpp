/******************************************************************************
 *
 * Project:  HDF5 read Driver
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

#include "hdf5dataset.h"

#include <algorithm>
#include <set>
#include <utility>

namespace GDAL
{

/************************************************************************/
/*                               HDF5Group                              */
/************************************************************************/

class HDF5Group final: public GDALGroup
{
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t           m_hGroup;
    std::set<std::pair<unsigned long, unsigned long>> m_oSetParentIds{};
    mutable std::vector<std::string> m_osListSubGroups{};
    mutable std::vector<std::string> m_osListArrays{};
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_oListAttributes{};
    mutable bool m_bShowAllAttributes = false;
    mutable bool m_bGotDims = false;
    mutable std::vector<std::shared_ptr<GDALDimension>> m_cachedDims{};

    static herr_t GetGroupNamesCallback( hid_t hGroup, const char *pszObjName,
                                         void* );

    static herr_t GetArrayNamesCallback( hid_t hGroup, const char *pszObjName,
                                         void* );

    static herr_t GetAttributesCallback( hid_t hGroup, const char *pszObjName,
                                         void* );

public:
    HDF5Group(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF5SharedResources>& poShared,
              const std::set<std::pair<unsigned long, unsigned long>>& oSetParentIds,
              hid_t hGroup,
              unsigned long objIds[2]):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_hGroup(hGroup),
        m_oSetParentIds(oSetParentIds)
    {
        m_oSetParentIds.insert(
            std::pair<unsigned long, unsigned long>(objIds[0], objIds[1]));
    }

    ~HDF5Group()
    {
        H5Gclose(m_hGroup);
    }

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;
};

/************************************************************************/
/*                             HDF5Dimension                            */
/************************************************************************/

class HDF5Dimension final: public GDALDimension
{
    std::string m_osGroupFullname;
    std::shared_ptr<HDF5SharedResources> m_poShared;

public:
    HDF5Dimension(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osType,
                  const std::string& osDirection,
                  GUInt64 nSize,
                  const std::shared_ptr<HDF5SharedResources>& poShared):
        GDALDimension(osParentName, osName, osType, osDirection, nSize),
        m_osGroupFullname(osParentName),
        m_poShared(poShared)
    {
    }

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;
};

/************************************************************************/
/*                           BuildDataType()                            */
/************************************************************************/

static GDALExtendedDataType BuildDataType(hid_t hDataType, bool& bHasVLen, bool& bNonNativeDataType,
                                          const std::vector<std::pair<std::string, hid_t>>& oTypes)
{
    const auto klass = H5Tget_class(hDataType);
    GDALDataType eDT = ::HDF5Dataset::GetDataType(hDataType);
    if( H5Tequal(H5T_NATIVE_SCHAR, hDataType) )
    {
        bNonNativeDataType = true;
        return GDALExtendedDataType::Create(GDT_Int16);
    }
    else if( eDT != GDT_Unknown )
        return GDALExtendedDataType::Create(eDT);
    else if (klass == H5T_STRING )
    {
        if( H5Tis_variable_str(hDataType) )
            bHasVLen = true;
        return GDALExtendedDataType::CreateString();
    }
    else if (klass == H5T_COMPOUND )
    {
        const unsigned nMembers = H5Tget_nmembers(hDataType);
        std::vector<std::unique_ptr<GDALEDTComponent>> components;
        size_t nOffset = 0;
        for(unsigned i = 0; i < nMembers; i++ )
        {
            char* pszName = H5Tget_member_name(hDataType, i);
            if( !pszName )
                return GDALExtendedDataType::Create(GDT_Unknown);
            CPLString osCompName(pszName);
            H5free_memory(pszName);
            const auto hMemberType = H5Tget_member_type(hDataType, i);
            if( hMemberType < 0 )
                return GDALExtendedDataType::Create(GDT_Unknown);
            const hid_t hNativeMemberType =
                H5Tget_native_type(hMemberType, H5T_DIR_ASCEND);
            auto memberDT = BuildDataType(hNativeMemberType, bHasVLen, bNonNativeDataType, oTypes);
            H5Tclose(hNativeMemberType);
            H5Tclose(hMemberType);
            if( memberDT.GetClass() == GEDTC_NUMERIC && memberDT.GetNumericDataType() == GDT_Unknown )
                return GDALExtendedDataType::Create(GDT_Unknown);
            if( (nOffset % memberDT.GetSize()) != 0 )
                nOffset += memberDT.GetSize() - (nOffset % memberDT.GetSize());
            if( nOffset != H5Tget_member_offset(hDataType, i) )
                bNonNativeDataType = true;
            components.emplace_back(std::unique_ptr<GDALEDTComponent>(
                new GDALEDTComponent(osCompName, nOffset, memberDT)));
            nOffset += memberDT.GetSize();
        }
        if( !components.empty() && (nOffset % components[0]->GetType().GetSize()) != 0 )
            nOffset += components[0]->GetType().GetSize() - (nOffset % components[0]->GetType().GetSize());
        if( nOffset != H5Tget_size(hDataType) )
            bNonNativeDataType = true;
        std::string osName("unnamed");
        for( const auto& oPair: oTypes )
        {
            const auto hPairNativeType = H5Tget_native_type(oPair.second, H5T_DIR_ASCEND);
            const auto matches = H5Tequal(hPairNativeType, hDataType);
            H5Tclose(hPairNativeType);
            if( matches )
            {
                osName = oPair.first;
                break;
            }
        }
        return GDALExtendedDataType::Create(osName,
                                            nOffset,
                                            std::move(components));
    }
    else if (klass == H5T_ENUM )
    {
        const auto hParent = H5Tget_super(hDataType);
        const hid_t hNativeParent =
                H5Tget_native_type(hParent, H5T_DIR_ASCEND);
        auto ret(BuildDataType(hNativeParent, bHasVLen, bNonNativeDataType, oTypes));
        H5Tclose(hNativeParent);
        H5Tclose(hParent);
        return ret;
    }
    else
    {
        return GDALExtendedDataType::Create(GDT_Unknown);
    }
}

/************************************************************************/
/*                    GetDataTypesInGroup()                             */
/************************************************************************/

static void GetDataTypesInGroup(hid_t hHDF5,
                                const std::string& osGroupFullName,
                                std::vector<std::pair<std::string, hid_t>>& oTypes)
{
    struct Callback
    {
        static herr_t f(hid_t hGroup, const char *pszObjName, void* user_data)
        {
            std::vector<std::pair<std::string, hid_t>>* poTypes =
             static_cast<std::vector<std::pair<std::string, hid_t>>*>(user_data);
            H5G_stat_t oStatbuf;

            if( H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0  )
                return -1;

            if( oStatbuf.type == H5G_TYPE )
            {
                poTypes->push_back(std::pair<std::string, hid_t>(
                    pszObjName, H5Topen(hGroup, pszObjName)));
            }

            return 0;
        }
    };
    H5Giterate(hHDF5, osGroupFullName.c_str(), nullptr,
               &(Callback::f), &oTypes);
}

/************************************************************************/
/*                            HDF5Array                                 */
/************************************************************************/

class HDF5Array final: public GDALMDArray
{
    std::string     m_osGroupFullname;
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t           m_hArray;
    hid_t           m_hDataSpace;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    hid_t           m_hNativeDT = H5I_INVALID_HID;
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_oListAttributes{};
    mutable bool m_bShowAllAttributes = false;
    bool            m_bHasVLenMember = false;
    bool            m_bHasNonNativeDataType = false;
    mutable std::vector<GByte> m_abyNoData{};
    mutable std::string m_osUnit{};
    mutable bool    m_bHasDimensionList = false;
    mutable bool    m_bHasDimensionLabels = false;
    haddr_t         m_nOffset;

    HDF5Array(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF5SharedResources>& poShared,
              hid_t hArray,
              const HDF5Group* poGroup,
              bool bSkipFullDimensionInstantiation);

    void InstantiateDimensions(const std::string& osParentName,
                               const HDF5Group* poGroup);

    bool ReadSlow(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const;

    static herr_t GetAttributesCallback( hid_t hArray, const char *pszObjName,
                                         void* );

protected:

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    ~HDF5Array();

    static std::shared_ptr<HDF5Array> Create(
                   const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF5SharedResources>& poShared,
                   hid_t hArray,
                   const HDF5Group* poGroup,
                   bool bSkipFullDimensionInstantiation)
    {
        auto ar(std::shared_ptr<HDF5Array>(new HDF5Array(
            osParentName, osName, poShared, hArray, poGroup, bSkipFullDimensionInstantiation)));
        if( ar->m_dt.GetClass() == GEDTC_NUMERIC &&
            ar->m_dt.GetNumericDataType() == GDT_Unknown )
        {
            return nullptr;
        }
        ar->SetSelf(ar);
        return ar;
    }

    bool IsWritable() const override { return !m_poShared->IsReadOnly(); }

    const std::string& GetFilename() const override { return m_poShared->GetFilename(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    const void* GetRawNoDataValue() const override
    {
        return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
    }

    const std::string& GetUnit() const override
    {
        return m_osUnit;
    }

    haddr_t GetFileOffset() const { return m_nOffset; }
};

/************************************************************************/
/*                           HDF5Attribute                              */
/************************************************************************/

class HDF5Attribute final: public GDALAttribute
{
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t           m_hAttribute;
    hid_t           m_hDataSpace;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    hid_t           m_hNativeDT = H5I_INVALID_HID;
    size_t          m_nElements = 1;
    bool            m_bHasVLenMember = false;
    bool            m_bHasNonNativeDataType = false;

    HDF5Attribute(const std::string& osGroupFullName,
                  const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF5SharedResources>& poShared,
                   hid_t hAttribute):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_poShared(poShared),
        m_hAttribute(hAttribute),
        m_hDataSpace(H5Aget_space(hAttribute))
    {
        const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
        std::vector<hsize_t> anDimSizes(nDims);
        if( nDims )
        {
            H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
            for( int i = 0; i < nDims; ++i )
            {
                m_nElements *= static_cast<size_t>(anDimSizes[i]);
                if( nDims == 1 && m_nElements == 1 )
                {
                    // Expose 1-dim of size 1 as scalar
                    break;
                }
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(),
                    CPLSPrintf("dim%d", i),
                    std::string(),
                    std::string(),
                    anDimSizes[i]));
            }
        }

        const hid_t hDataType = H5Aget_type(hAttribute);
        m_hNativeDT = H5Tget_native_type(hDataType, H5T_DIR_ASCEND);
        H5Tclose(hDataType);

        std::vector<std::pair<std::string, hid_t>> oTypes;
        if( !osGroupFullName.empty() &&
            H5Tget_class(m_hNativeDT) == H5T_COMPOUND )
        {
            GetDataTypesInGroup(m_poShared->GetHDF5(), osGroupFullName, oTypes);
        }

        m_dt = BuildDataType(m_hNativeDT, m_bHasVLenMember, m_bHasNonNativeDataType, oTypes);
        for( auto& oPair: oTypes )
            H5Tclose(oPair.second);
        if( m_dt.GetClass() == GEDTC_NUMERIC &&
            m_dt.GetNumericDataType() == GDT_Unknown )
        {
            CPLDebug("HDF5",
                     "Cannot map data type of %s to a type handled by GDAL",
                     osName.c_str());
        }
    }

protected:

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    ~HDF5Attribute();

    static std::shared_ptr<HDF5Attribute> Create(
                   const std::string& osGroupFullName,
                   const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF5SharedResources>& poShared,
                   hid_t hAttribute)
    {
        auto ar(std::shared_ptr<HDF5Attribute>(new HDF5Attribute(
            osGroupFullName, osParentName, osName, poShared, hAttribute)));
        if( ar->m_dt.GetClass() == GEDTC_NUMERIC &&
            ar->m_dt.GetNumericDataType() == GDT_Unknown )
        {
            return nullptr;
        }
        return ar;
    }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }
};

/************************************************************************/
/*                        HDF5SharedResources()                         */
/************************************************************************/

HDF5SharedResources::HDF5SharedResources(const std::string& osFilename):
    m_osFilename(osFilename),
    m_poPAM(std::make_shared<GDALPamMultiDim>(osFilename))
{
}

/************************************************************************/
/*                        ~HDF5SharedResources()                        */
/************************************************************************/

HDF5SharedResources::~HDF5SharedResources()
{
    if( m_hHDF5 > 0 )
        H5Fclose(m_hHDF5);
}

/************************************************************************/
/*                         GetDimensions()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> HDF5Group::GetDimensions(CSLConstList) const
{
    if( m_bGotDims )
        return m_cachedDims;

    struct CallbackData
    {
        std::shared_ptr<HDF5SharedResources> poShared{};
        std::string osFullName{};
        std::vector<std::shared_ptr<GDALDimension>> oListDim{};
    };

    struct Callback
    {
        static herr_t f(hid_t hGroup, const char *pszObjName, void* user_data)
        {
            CallbackData* data = static_cast<CallbackData*>(user_data);
            H5G_stat_t oStatbuf;

            if( H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0  )
                return -1;

            if( oStatbuf.type == H5G_DATASET )
            {
                auto hArray = H5Dopen(hGroup, pszObjName);
                if( hArray >= 0 )
                {
                    auto ar = HDF5Array::Create(data->osFullName, pszObjName,
                                                data->poShared, hArray,
                                                nullptr, true);
                    if( ar && ar->GetDimensionCount() == 1 )
                    {
                        auto attrCLASS = ar->GetAttribute("CLASS");
                        if( attrCLASS &&
                            attrCLASS->GetDimensionCount() == 0 &&
                            attrCLASS->GetDataType().GetClass() == GEDTC_STRING )
                        {
                            const char* pszStr = attrCLASS->ReadAsString();
                            if( pszStr && EQUAL(pszStr, "DIMENSION_SCALE") )
                            {
                                auto attrNAME = ar->GetAttribute("NAME");
                                if( attrNAME &&
                                    attrNAME->GetDimensionCount() == 0 &&
                                    attrNAME->GetDataType().GetClass() == GEDTC_STRING )
                                {
                                    const char* pszName = attrNAME->ReadAsString();
                                    if( pszName && STARTS_WITH(pszName,
                                        "This is a netCDF dimension but not a netCDF variable") )
                                    {
                                        data->oListDim.emplace_back(
                                            std::make_shared<GDALDimension>(
                                                data->osFullName, pszObjName,
                                                std::string(), std::string(),
                                                ar->GetDimensions()[0]->GetSize()));
                                        return 0;
                                    }
                                }

                                data->oListDim.emplace_back(
                                    std::make_shared<HDF5Dimension>(
                                        data->osFullName, pszObjName,
                                        std::string(), std::string(),
                                        ar->GetDimensions()[0]->GetSize(),
                                        data->poShared));
                            }
                        }
                    }
                }
            }

            return 0;
        }
    };

    CallbackData data;
    data.poShared = m_poShared;
    data.osFullName = GetFullName();
    H5Giterate(m_poShared->GetHDF5(), GetFullName().c_str(), nullptr,
               &(Callback::f), &data);
    m_bGotDims = true;
    m_cachedDims = data.oListDim;
    return data.oListDim;
}

/************************************************************************/
/*                          GetGroupNamesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetGroupNamesCallback( hid_t hGroup,
                                         const char *pszObjName,
                                         void *selfIn)
{
    HDF5Group* self = static_cast<HDF5Group*>(selfIn);
    H5G_stat_t oStatbuf;

    if( H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0  )
        return -1;

    if( oStatbuf.type == H5G_GROUP )
    {
        if( self->m_oSetParentIds.find(std::pair<unsigned long, unsigned long>(
            oStatbuf.objno[0], oStatbuf.objno[1])) == self->m_oSetParentIds.end() )
        {
            self->m_osListSubGroups.push_back(pszObjName);
        }
        else
        {
            CPLDebug("HDF5",
                     "Group %s contains a link to group %s which is "
                     "itself, or one of its ancestor.",
                     self->GetFullName().c_str(),
                     pszObjName);
        }
    }
    return 0;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF5Group::GetGroupNames(CSLConstList) const
{
    m_osListSubGroups.clear();
    H5Giterate(m_poShared->GetHDF5(), GetFullName().c_str(), nullptr,
               GetGroupNamesCallback,
               const_cast<void*>(static_cast<const void*>(this)));
    return m_osListSubGroups;
}


/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF5Group::OpenGroup(const std::string& osName, CSLConstList) const
{
    if( m_osListSubGroups.empty() )
        GetGroupNames(nullptr);
    if( std::find(m_osListSubGroups.begin(), m_osListSubGroups.end(), osName) ==
            m_osListSubGroups.end() )
    {
        return nullptr;
    }

    H5G_stat_t oStatbuf;
    if( H5Gget_objinfo(m_hGroup, osName.c_str(), FALSE, &oStatbuf) < 0 )
        return nullptr;

    auto hSubGroup = H5Gopen(m_hGroup, osName.c_str());
    if( hSubGroup < 0 )
    {
        return nullptr;
    }
    return std::make_shared<HDF5Group>(GetFullName(), osName,
                                       m_poShared,
                                       m_oSetParentIds,
                                       hSubGroup,
                                       oStatbuf.objno);
}

/************************************************************************/
/*                          GetArrayNamesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetArrayNamesCallback( hid_t hGroup,
                                         const char *pszObjName,
                                         void *selfIn)
{
    HDF5Group* self = static_cast<HDF5Group*>(selfIn);
    H5G_stat_t oStatbuf;

    if( H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0  )
        return -1;

    if( oStatbuf.type == H5G_DATASET )
    {
        auto hArray = H5Dopen(hGroup, pszObjName);
        if( hArray >= 0 )
        {
            auto ar(HDF5Array::Create(std::string(), pszObjName,
                                      self->m_poShared, hArray, self, true));
            if( ar )
            {
                auto attrNAME = ar->GetAttribute("NAME");
                if( attrNAME &&
                    attrNAME->GetDimensionCount() == 0 &&
                    attrNAME->GetDataType().GetClass() == GEDTC_STRING )
                {
                    const char* pszName = attrNAME->ReadAsString();
                    if( pszName && STARTS_WITH(pszName,
                        "This is a netCDF dimension but not a netCDF variable") )
                    {
                        return 0;
                    }
                }
            }
        }
        self->m_osListArrays.push_back(pszObjName);
    }
    return 0;
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF5Group::GetMDArrayNames(CSLConstList) const
{
    m_osListArrays.clear();
    H5Giterate(m_poShared->GetHDF5(), GetFullName().c_str(), nullptr,
               GetArrayNamesCallback,
               const_cast<void*>(static_cast<const void*>(this)));
    return m_osListArrays;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF5Group::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    if( m_osListArrays.empty() )
        GetMDArrayNames(nullptr);
    if( std::find(m_osListArrays.begin(), m_osListArrays.end(), osName) ==
            m_osListArrays.end() )
    {
        return nullptr;
    }
    auto hArray = H5Dopen(m_hGroup, osName.c_str());
    if( hArray < 0 )
    {
        return nullptr;
    }
    return HDF5Array::Create(GetFullName(), osName, m_poShared, hArray,
                             this, false);
}

/************************************************************************/
/*                          GetAttributesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetAttributesCallback( hid_t hGroup,
                                         const char *pszObjName,
                                         void *selfIn)
{
    HDF5Group* self = static_cast<HDF5Group*>(selfIn);
    if( self->m_bShowAllAttributes ||
        (!EQUAL(pszObjName, "_Netcdf4Dimid") &&
         !EQUAL(pszObjName, "_NCProperties")) )
    {
        hid_t hAttr = H5Aopen_name(hGroup, pszObjName);
        if( hAttr > 0 )
        {
            auto attr(HDF5Attribute::Create(self->GetFullName(),
                                            self->GetFullName(),
                                            pszObjName,
                                            self->m_poShared, hAttr));
            if( attr )
            {
                self->m_oListAttributes.emplace_back(attr);
            }
        }
    }
    return 0;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF5Group::GetAttributes(
                                            CSLConstList papszOptions) const
{
    m_oListAttributes.clear();
    m_bShowAllAttributes =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    H5Aiterate(m_hGroup, nullptr,
               GetAttributesCallback,
               const_cast<void*>(static_cast<const void*>(this)));
    return m_oListAttributes;
}

/************************************************************************/
/*                               ~HDF5Array()                           */
/************************************************************************/

HDF5Array::~HDF5Array()
{
    if( m_hArray > 0 )
        H5Dclose(m_hArray);
    if( m_hNativeDT > 0 )
        H5Tclose(m_hNativeDT);
    if( m_hDataSpace > 0 )
        H5Sclose(m_hDataSpace);
}

/************************************************************************/
/*                                HDF5Array()                           */
/************************************************************************/

HDF5Array::HDF5Array(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF5SharedResources>& poShared,
                   hid_t hArray,
                   const HDF5Group* poGroup,
                   bool bSkipFullDimensionInstantiation):
        GDALAbstractMDArray(osParentName, osName),
        GDALMDArray(osParentName, osName),
        m_osGroupFullname(osParentName),
        m_poShared(poShared),
        m_hArray(hArray),
        m_hDataSpace(H5Dget_space(hArray)),
        m_nOffset(H5Dget_offset(hArray))
{
    const hid_t hDataType = H5Dget_type(hArray);
    m_hNativeDT = H5Tget_native_type(hDataType, H5T_DIR_ASCEND);
    H5Tclose(hDataType);

    std::vector<std::pair<std::string, hid_t>> oTypes;
    if( !osParentName.empty() &&
        H5Tget_class(m_hNativeDT) == H5T_COMPOUND )
    {
        GetDataTypesInGroup(m_poShared->GetHDF5(), osParentName, oTypes);
    }

    m_dt = BuildDataType(m_hNativeDT, m_bHasVLenMember, m_bHasNonNativeDataType, oTypes);
    for( auto& oPair: oTypes )
        H5Tclose(oPair.second);

    if( m_dt.GetClass() == GEDTC_NUMERIC &&
        m_dt.GetNumericDataType() == GDT_Unknown )
    {
        CPLDebug("HDF5",
                    "Cannot map data type of %s to a type handled by GDAL",
                    osName.c_str());
        return;
    }

    HDF5Array::GetAttributes();

    if( bSkipFullDimensionInstantiation )
    {
        const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
        std::vector<hsize_t> anDimSizes(nDims);
        if( nDims )
        {
            H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
            for( int i = 0; i < nDims; ++i )
            {
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(),
                    CPLSPrintf("dim%d", i),
                    std::string(),
                    std::string(),
                    anDimSizes[i]));
            }
        }
    }
    else
    {
        InstantiateDimensions(osParentName, poGroup);
    }
}

/************************************************************************/
/*                        InstantiateDimensions()                       */
/************************************************************************/

void HDF5Array::InstantiateDimensions(const std::string& osParentName,
                                      const HDF5Group* poGroup)
{
    const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
    std::vector<hsize_t> anDimSizes(nDims);
    if( nDims )
    {
        H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
    }

    if( nDims == 1 )
    {
        auto attrCLASS = GetAttribute("CLASS");
        if( attrCLASS &&
            attrCLASS->GetDimensionCount() == 0 &&
            attrCLASS->GetDataType().GetClass() == GEDTC_STRING )
        {
            const char* pszStr = attrCLASS->ReadAsString();
            if( pszStr && EQUAL(pszStr, "DIMENSION_SCALE") )
            {
                auto attrName = GetAttribute("NAME");
                if( attrName &&
                    attrName->GetDataType().GetClass() == GEDTC_STRING )
                {
                    const char* pszName = attrName->ReadAsString();
                    if( pszName &&
                        STARTS_WITH(pszName,
                            "This is a netCDF dimension but not a netCDF variable") )
                    {
                        m_dims.emplace_back(
                            std::make_shared<GDALDimension>(
                                std::string(), GetName(),
                                std::string(), std::string(),
                                anDimSizes[0]));
                        return;
                    }
                }

                m_dims.emplace_back(
                    std::make_shared<HDF5Dimension>(
                        osParentName, GetName(),
                        std::string(), std::string(),
                        anDimSizes[0],
                        m_poShared));
                return;
            }
        }
    }

    std::map<size_t, std::string> mapDimIndexToDimFullName;

    if( m_bHasDimensionList )
    {
        hid_t hAttr = H5Aopen_name(m_hArray, "DIMENSION_LIST");
        const hid_t hAttrDataType = H5Aget_type(hAttr);
        const hid_t hAttrSpace = H5Aget_space(hAttr);
        if( H5Tget_class(hAttrDataType) == H5T_VLEN &&
            H5Sget_simple_extent_ndims(hAttrSpace) == 1 )
        {
            const hid_t hBaseType = H5Tget_super(hAttrDataType);
            if( H5Tget_class(hBaseType) == H5T_REFERENCE )
            {
                hsize_t nSize = 0;
                H5Sget_simple_extent_dims(hAttrSpace, &nSize, nullptr);
                if( nSize == static_cast<hsize_t>(nDims) )
                {
                    std::vector<hvl_t> aHvl(static_cast<size_t>(nSize));
                    H5Aread(hAttr, hAttrDataType, &aHvl[0]);
                    for( size_t i = 0; i < nSize; i++ )
                    {
                        if( aHvl[i].len == 1 &&
                            H5Rget_obj_type(m_hArray, H5R_OBJECT, aHvl[i].p) == H5G_DATASET )
                        {
                            std::string referenceName;
                            referenceName.resize(256);
                            auto ret = H5Rget_name(
                                m_poShared->GetHDF5(), H5R_OBJECT, aHvl[i].p,
                                &referenceName[0], referenceName.size());
                            if( ret > 0 )
                            {
                                referenceName.resize(ret);
                                mapDimIndexToDimFullName[i] = referenceName;
                            }
                        }
                    }
                    H5Dvlen_reclaim(hAttrDataType, hAttrSpace, H5P_DEFAULT, &aHvl[0]);
                }
            }
            H5Tclose(hBaseType);
        }
        H5Tclose(hAttrDataType);
        H5Sclose(hAttrSpace);
        H5Aclose(hAttr);
    }
    else if( m_bHasDimensionLabels )
    {
        hid_t hAttr = H5Aopen_name(m_hArray, "DIMENSION_LABELS");
        auto attr(HDF5Attribute::Create(m_osGroupFullname, GetFullName(),
                                        "DIMENSION_LABELS", m_poShared,
                                        hAttr));
        if( attr && attr->GetDimensionCount() == 1 &&
            attr->GetDataType().GetClass() == GEDTC_STRING )
        {
            auto aosList = attr->ReadAsStringArray();
            if( aosList.size() == nDims )
            {
                for( int i = 0; i < nDims; ++i )
                {
                    if( aosList[i][0] != '\0' )
                    {
                        mapDimIndexToDimFullName[i] = aosList[i];
                    }
                }
            }
        }
    }

    std::map<std::string, std::shared_ptr<GDALDimension>> oMapFullNameToDim;
    // cppcheck-suppress knownConditionTrueFalse
    if( poGroup && !mapDimIndexToDimFullName.empty() )
    {
        auto groupDims = poGroup->GetDimensions();
        for( auto dim: groupDims )
        {
            oMapFullNameToDim[dim->GetFullName()] = dim;
        }
    }

    for( int i = 0; i < nDims; ++i )
    {
        auto oIter = mapDimIndexToDimFullName.find(static_cast<size_t>(i));
        if( oIter != mapDimIndexToDimFullName.end() )
        {
            auto oIter2 = oMapFullNameToDim.find(oIter->second);
            if( oIter2 != oMapFullNameToDim.end() )
            {
                m_dims.emplace_back(oIter2->second);
                continue;
            }

            std::string osDimName(oIter->second);
            auto nPos = osDimName.rfind('/');
            if( nPos != std::string::npos )
            {
                std::string osDimParentName(osDimName.substr(0, nPos));
                osDimName = osDimName.substr(nPos + 1);

                m_dims.emplace_back(
                        std::make_shared<HDF5Dimension>(
                            osDimParentName.empty() ? "/" : osDimParentName,
                            osDimName,
                            std::string(), std::string(),
                            anDimSizes[i],
                            m_poShared));
            }
            else
            {
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(),
                    osDimName,
                    std::string(),
                    std::string(),
                    anDimSizes[i]));
            }
        }
        else
        {
            m_dims.emplace_back(std::make_shared<GDALDimension>(
                std::string(),
                CPLSPrintf("dim%d", i),
                std::string(),
                std::string(),
                anDimSizes[i]));
        }
    }
}

/************************************************************************/
/*                          GetAttributesCallback()                     */
/************************************************************************/

herr_t HDF5Array::GetAttributesCallback( hid_t hArray,
                                         const char *pszObjName,
                                         void *selfIn)
{
    HDF5Array* self = static_cast<HDF5Array*>(selfIn);
    if( self->m_bShowAllAttributes ||
        (strcmp(pszObjName, "_Netcdf4Dimid") != 0 &&
         strcmp(pszObjName, "_Netcdf4Coordinates") != 0 &&
         strcmp(pszObjName, "CLASS") != 0 &&
         strcmp(pszObjName, "NAME") != 0) )
    {
        if( EQUAL(pszObjName, "DIMENSION_LIST") )
        {
            self->m_bHasDimensionList = true;
            if( !self->m_bShowAllAttributes )
                return 0;
        }
        if( EQUAL(pszObjName, "DIMENSION_LABELS") )
        {
            self->m_bHasDimensionLabels = true;
            if( !self->m_bShowAllAttributes )
                return 0;
        }

        hid_t hAttr = H5Aopen_name(hArray, pszObjName);
        if( hAttr > 0 )
        {
            auto attr(HDF5Attribute::Create(self->m_osGroupFullname,
                                            self->GetFullName(),
                                            pszObjName,
                                            self->m_poShared, hAttr));
            if( attr )
            {
                // Used by HDF5-EOS products
                if( EQUAL(pszObjName, "_FillValue") &&
                    self->GetDataType() == attr->GetDataType() &&
                    attr->GetDimensionCount() == 0 )
                {
                    if( self->GetDataType().GetClass() == GEDTC_NUMERIC )
                    {
                        auto raw(attr->ReadAsRaw());
                        if( raw.data() )
                        {
                            self->m_abyNoData.assign(raw.data(),
                                                    raw.data() + raw.size());
                        }
                    }
                    if( !self->m_bShowAllAttributes )
                        return 0;
                }

                if( EQUAL(pszObjName, "units") &&
                    attr->GetDataType().GetClass() == GEDTC_STRING &&
                    attr->GetDimensionCount() == 0 )
                {
                    const char* pszStr = attr->ReadAsString();
                    if( pszStr )
                    {
                        self->m_osUnit = pszStr;
                        if( !self->m_bShowAllAttributes )
                            return 0;
                    }
                }

                self->m_oListAttributes.emplace_back(attr);
            }
        }
    }
    return 0;
}


/************************************************************************/
/*                       GetAttributeFromAttributes()                   */
/************************************************************************/

/** Possible fallback implementation for GetAttribute() using GetAttributes().
 */
std::shared_ptr<GDALAttribute> HDF5Array::GetAttribute(
                                            const std::string& osName) const
{
    const char* const apszOptions[] = { "SHOW_ALL=YES", nullptr };
    if( !m_bShowAllAttributes )
        GetAttributes(apszOptions);
    for( const auto& attr: m_oListAttributes )
    {
        if( attr->GetName() == osName )
            return attr;
    }
    return nullptr;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF5Array::GetAttributes(
                                            CSLConstList papszOptions) const
{
    m_oListAttributes.clear();
    m_bShowAllAttributes =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    H5Aiterate(m_hArray, nullptr,
               GetAttributesCallback,
               const_cast<void*>(static_cast<const void*>(this)));
    return m_oListAttributes;
}

/************************************************************************/
/*                           CopyBuffer()                               */
/************************************************************************/

static void CopyBuffer(size_t nDims,
                       const size_t* count,
                       const GInt64* arrayStep,
                       const GPtrDiff_t* bufferStride,
                       const GDALExtendedDataType& bufferDataType,
                       GByte* pabySrc,
                       void* pDstBuffer)
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte*> pabySrcBufferStack(nDims + 1);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    std::vector<GPtrDiff_t> anSrcStride(nDims);
    std::vector<size_t> anSrcOffset(nDims+1);
    size_t nCurStride = nBufferDataTypeSize;
    for( size_t i = nDims; i > 0; )
    {
        --i;
        anSrcStride[i] = arrayStep[i] > 0 ? nCurStride :
                                    -static_cast<GPtrDiff_t>(nCurStride);
        anSrcOffset[i] = arrayStep[i] > 0 ? 0 : (count[i] - 1) * nCurStride;
        nCurStride *= count[i];
    }
    pabySrcBufferStack[0] = pabySrc + anSrcOffset[0];
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        memcpy(pabyDstBufferStack[nDims], pabySrcBufferStack[nDims],
               nBufferDataTypeSize);
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabySrcBufferStack[iDim] = pabySrcBufferStack[iDim-1] + anSrcOffset[iDim];
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * nBufferDataTypeSize;
            pabySrcBufferStack[iDim] += anSrcStride[iDim];
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                             ReadSlow()                               */
/************************************************************************/

bool HDF5Array::ReadSlow(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    if( nBufferDataTypeSize == 0 )
        return false;
    const size_t nDims(m_dims.size());
    size_t nEltCount = 1;
    for( size_t i = 0; i < nDims; ++i )
    {
        nEltCount *= count[i];
    }

    // Only for testing
    const char* pszThreshold = CPLGetConfigOption(
        "GDAL_HDF5_TEMP_ARRAY_ALLOC_SIZE", "10000000");
    const GUIntBig nThreshold = CPLScanUIntBig(
        pszThreshold, static_cast<int>(strlen(pszThreshold)));
    if( nEltCount < nThreshold / nBufferDataTypeSize )
    {
        CPLDebug("HDF5", "Using slow path");
        std::vector<GByte> abyTemp;
        try
        {
            abyTemp.resize(nEltCount * nBufferDataTypeSize);
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
        std::vector<GUInt64> anStart(nDims);
        std::vector<GInt64> anStep(nDims);
        for( size_t i = 0; i < nDims; i++ )
        {
            if( arrayStep[i] >= 0 )
            {
                anStart[i] = arrayStartIdx[i];
                anStep[i] = arrayStep[i];
            }
            else
            {
                // Use double negation so that operations occur only on
                // positive quantities to avoid an artificial negative signed
                // integer to unsigned conversion.
                anStart[i] = arrayStartIdx[i] - ((-arrayStep[i]) * (count[i]-1));
                anStep[i] = -arrayStep[i];
            }
        }
        std::vector<GPtrDiff_t> anStride(nDims);
        size_t nCurStride = 1;
        for( size_t i = nDims; i > 0; )
        {
            --i;
            anStride[i] = nCurStride;
            nCurStride *= count[i];
        }
        if( !IRead(anStart.data(), count, anStep.data(), anStride.data(),
                   bufferDataType, &abyTemp[0]) )
        {
            return false;
        }
        CopyBuffer(nDims, count, arrayStep, bufferStride, bufferDataType,
                   &abyTemp[0], pDstBuffer);
        return true;
    }

    CPLDebug("HDF5", "Using very slow path");
    std::vector<GUInt64> anStart(nDims);
    const std::vector<size_t> anCount(nDims, 1);
    const std::vector<GInt64> anStep(nDims, 1);
    const std::vector<GPtrDiff_t> anStride(nDims, 1);

    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        if( !IRead(anStart.data(), anCount.data(), anStep.data(), anStride.data(),
                   bufferDataType, pabyDstBufferStack[nDims]) )
        {
            return false;
        }
    }
    else
    {
        anStart[iDim] = arrayStartIdx[iDim];
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * nBufferDataTypeSize;
            anStart[iDim] = CPLUnsanitizedAdd<GUInt64>(anStart[iDim], arrayStep[iDim]);
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
    return true;
}

/************************************************************************/
/*                       IngestVariableStrings()                        */
/************************************************************************/

static void IngestVariableStrings(void* pDstBuffer, hid_t hBufferType,
                                  size_t nDims,
                                  const size_t* count,
                                  const GPtrDiff_t* bufferStride)
{
    std::vector<hsize_t> anCountOne(nDims, 1);
    const hid_t hMemSpaceOne = nDims == 0 ?
        H5Screate(H5S_SCALAR) :
        H5Screate_simple(static_cast<int>(nDims), anCountOne.data(), nullptr);
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        void* old_ptr = pabyDstBufferStack[nDims];
        const char* pszSrcStr = *static_cast<char**>(old_ptr);
        char* pszNewStr = pszSrcStr ? VSIStrdup(pszSrcStr) : nullptr;
        H5Dvlen_reclaim(hBufferType, hMemSpaceOne, H5P_DEFAULT, old_ptr);
        *static_cast<char**>(old_ptr) = pszNewStr;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * sizeof(char*);
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
    H5Sclose(hMemSpaceOne);
}

/************************************************************************/
/*                    IngestFixedLengthStrings()                        */
/************************************************************************/

static void IngestFixedLengthStrings(void* pDstBuffer,
                                     const void* pTemp,
                                     hid_t hBufferType,
                                     size_t nDims,
                                     const size_t* count,
                                     const GPtrDiff_t* bufferStride)
{
    const size_t nStringSize = H5Tget_size(hBufferType);
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    const GByte* pabySrcBuffer = static_cast<const GByte*>(pTemp);
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
    const bool bSpacePad = H5Tget_strpad(hBufferType) == H5T_STR_SPACEPAD;
lbl_next_depth:
    if( iDim == nDims )
    {
        char* pszStr = static_cast<char*>(VSIMalloc(nStringSize + 1));
        if( pszStr )
        {
            memcpy(pszStr, pabySrcBuffer, nStringSize);
            size_t nIter = nStringSize;
            if( bSpacePad )
            {
                while( nIter >= 1 && pszStr[nIter-1] == ' ' )
                {
                    nIter --;
                }
            }
            pszStr[nIter] = 0;
        }
        void* ptr = pabyDstBufferStack[nDims];
        *static_cast<char**>(ptr) = pszStr;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * sizeof(char*);
            pabySrcBuffer += nStringSize;
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
}


/************************************************************************/
/*                   GetHDF5DataTypeFromGDALDataType()                  */
/************************************************************************/

static hid_t GetHDF5DataTypeFromGDALDataType(const GDALExtendedDataType& dt,
                                             hid_t hNativeDT,
                                             const GDALExtendedDataType& bufferDataType)
{
    hid_t hBufferType = H5I_INVALID_HID;
    switch( bufferDataType.GetNumericDataType() )
    {
        case GDT_Byte: hBufferType = H5Tcopy(H5T_NATIVE_UCHAR); break;
        case GDT_UInt16: hBufferType = H5Tcopy(H5T_NATIVE_USHORT); break;
        case GDT_Int16: hBufferType = H5Tcopy(H5T_NATIVE_SHORT); break;
        case GDT_UInt32: hBufferType = H5Tcopy(H5T_NATIVE_UINT); break;
        case GDT_Int32: hBufferType = H5Tcopy(H5T_NATIVE_INT); break;
        case GDT_UInt64: hBufferType = H5Tcopy(H5T_NATIVE_UINT64); break;
        case GDT_Int64: hBufferType = H5Tcopy(H5T_NATIVE_INT64); break;
        case GDT_Float32: hBufferType = H5Tcopy(H5T_NATIVE_FLOAT); break;
        case GDT_Float64: hBufferType = H5Tcopy(H5T_NATIVE_DOUBLE); break;
        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat32:
        case GDT_CFloat64:
            if( bufferDataType != dt )
            {
                return H5I_INVALID_HID;
            }
            else
            {
                hBufferType = H5Tcopy(hNativeDT);
                break;
            }
        default:
            return H5I_INVALID_HID;
    }
    return hBufferType;
}

/************************************************************************/
/*                        FreeDynamicMemory()                           */
/************************************************************************/

static void FreeDynamicMemory(GByte* pabyPtr, hid_t hDataType)
{
    const auto klass = H5Tget_class(hDataType);
    if( klass == H5T_STRING && H5Tis_variable_str(hDataType) )
    {
        auto hDataSpace = H5Screate(H5S_SCALAR);
        H5Dvlen_reclaim(hDataType, hDataSpace, H5P_DEFAULT, pabyPtr);
        H5Sclose(hDataSpace);
    }
    else if (klass == H5T_COMPOUND )
    {
        const unsigned nMembers = H5Tget_nmembers(hDataType);
        for(unsigned i = 0; i < nMembers; i++ )
        {
            const auto nOffset = H5Tget_member_offset(hDataType, i);
            auto hMemberType = H5Tget_member_type(hDataType, i);
            if( hMemberType < 0 )
                continue;
            FreeDynamicMemory(pabyPtr + nOffset, hMemberType);
            H5Tclose(hMemberType);
        }
    }
}

/************************************************************************/
/*                   CreateMapTargetComponentsToSrc()                   */
/************************************************************************/

static std::vector<unsigned> CreateMapTargetComponentsToSrc(
                hid_t hSrcDataType, const GDALExtendedDataType& dstDataType)
{
    CPLAssert( H5Tget_class(hSrcDataType) == H5T_COMPOUND );
    CPLAssert( dstDataType.GetClass() == GEDTC_COMPOUND );

    const unsigned nMembers = H5Tget_nmembers(hSrcDataType);
    std::map<std::string, unsigned> oMapSrcCompNameToIdx;
    for(unsigned i = 0; i < nMembers; i++ )
    {
        char* pszName = H5Tget_member_name(hSrcDataType, i);
        if( pszName )
            oMapSrcCompNameToIdx[pszName] = i;
    }

    std::vector<unsigned> ret;
    const auto& comps = dstDataType.GetComponents();
    ret.reserve(comps.size());
    for( const auto& comp: comps )
    {
        auto oIter = oMapSrcCompNameToIdx.find(comp->GetName());
        CPLAssert( oIter != oMapSrcCompNameToIdx.end() );
        ret.emplace_back(oIter->second);
    }
    return ret;
}

/************************************************************************/
/*                            CopyValue()                               */
/************************************************************************/

static void CopyValue(const GByte* pabySrcBuffer, hid_t hSrcDataType,
                      GByte* pabyDstBuffer, const GDALExtendedDataType& dstDataType,
                      const std::vector<unsigned>& mapDstCompsToSrcComps)
{
    const auto klass = H5Tget_class(hSrcDataType);
    if( klass == H5T_STRING )
    {
        if( H5Tis_variable_str(hSrcDataType) )
        {
            GDALExtendedDataType::CopyValue(
                pabySrcBuffer, GDALExtendedDataType::CreateString(),
                pabyDstBuffer, dstDataType);
        }
        else
        {
            size_t nStringSize = H5Tget_size(hSrcDataType);
            char* pszStr = static_cast<char*>(VSIMalloc(nStringSize + 1));
            if( pszStr )
            {
                memcpy(pszStr, pabySrcBuffer, nStringSize);
                pszStr[nStringSize] = 0;
            }
            GDALExtendedDataType::CopyValue(
                &pszStr, GDALExtendedDataType::CreateString(),
                pabyDstBuffer, dstDataType);
            CPLFree(pszStr);
        }
    }
    else if( klass == H5T_COMPOUND )
    {
        if( dstDataType.GetClass() != GEDTC_COMPOUND )
        {
            // Typically source is complex data type
            auto srcDataType(GDALExtendedDataType::Create(::HDF5Dataset::GetDataType(hSrcDataType)));
            if( srcDataType.GetClass() == GEDTC_NUMERIC &&
                srcDataType.GetNumericDataType() != GDT_Unknown )
            {
                GDALExtendedDataType::CopyValue(
                    pabySrcBuffer, srcDataType,
                    pabyDstBuffer, dstDataType);
            }
        }
        else
        {
            const auto& comps = dstDataType.GetComponents();
            CPLAssert( comps.size() == mapDstCompsToSrcComps.size() );
            const std::vector<unsigned> empty;
            for( size_t iDst = 0; iDst < comps.size(); ++iDst )
            {
                const unsigned iSrc = mapDstCompsToSrcComps[iDst];
                auto hMemberType = H5Tget_member_type(hSrcDataType, iSrc);
                CopyValue( pabySrcBuffer + H5Tget_member_offset(hSrcDataType, iSrc),
                           hMemberType,
                           pabyDstBuffer + comps[iDst]->GetOffset(),
                           comps[iDst]->GetType(), empty );
                H5Tclose(hMemberType);
            }
        }
    }
    else if( klass == H5T_ENUM )
    {
        auto hParent = H5Tget_super(hSrcDataType);
        CopyValue(pabySrcBuffer, hParent, pabyDstBuffer, dstDataType, {});
        H5Tclose(hParent);
    }
    else
    {
        if( H5Tequal(H5T_NATIVE_SCHAR, hSrcDataType) )
        {
            const GInt16 nVal = *reinterpret_cast<const signed char*>(pabySrcBuffer);
            // coverity[overrun-buffer-val]
            GDALExtendedDataType::CopyValue(
                &nVal, GDALExtendedDataType::Create(GDT_Int16),
                pabyDstBuffer, dstDataType);
        }
        else
        {
            GDALDataType eDT = ::HDF5Dataset::GetDataType(hSrcDataType);
            GDALExtendedDataType::CopyValue(
                pabySrcBuffer, GDALExtendedDataType::Create(eDT),
                pabyDstBuffer, dstDataType);
        }
    }
}

/************************************************************************/
/*                        CopyToFinalBuffer()                           */
/************************************************************************/

static void CopyToFinalBuffer(void* pDstBuffer,
                           const void* pTemp,
                           size_t nDims,
                           const size_t* count,
                           const GPtrDiff_t* bufferStride,
                           hid_t hSrcDataType,
                           const GDALExtendedDataType& bufferDataType)
{
    const size_t nSrcDataTypeSize(H5Tget_size(hSrcDataType));
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    const GByte* pabySrcBuffer = static_cast<const GByte*>(pTemp);
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
    const auto mapDstCompsToSrcComps =
        (H5Tget_class(hSrcDataType) == H5T_COMPOUND &&
         bufferDataType.GetClass() == GEDTC_COMPOUND) ?
        CreateMapTargetComponentsToSrc(hSrcDataType, bufferDataType) :
        std::vector<unsigned>();

lbl_next_depth:
    if( iDim == nDims )
    {
        CopyValue(
            pabySrcBuffer, hSrcDataType,
            pabyDstBufferStack[nDims], bufferDataType,
            mapDstCompsToSrcComps);
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * bufferDataType.GetSize();
            pabySrcBuffer += nSrcDataTypeSize;
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF5Array::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    const size_t nDims(m_dims.size());
    std::vector<H5OFFSET_TYPE> anOffset(nDims);
    std::vector<hsize_t> anCount(nDims);
    std::vector<hsize_t> anStep(nDims);

    size_t nEltCount = 1;
    for( size_t i = 0; i < nDims; ++i )
    {
        if( count[i] != 1 && (arrayStep[i] < 0 || bufferStride[i] < 0) )
        {
            return ReadSlow(arrayStartIdx, count, arrayStep, bufferStride,
                            bufferDataType, pDstBuffer);
        }
        anOffset[i] = static_cast<hsize_t>(arrayStartIdx[i]);
        anCount[i] = static_cast<hsize_t>(count[i]);
        anStep[i] = static_cast<hsize_t>(count[i] == 1 ? 1 : arrayStep[i]);
        nEltCount *= count[i];
    }
    size_t nCurStride = 1;
    for( size_t i = nDims; i > 0; )
    {
        --i;
        if( count[i] != 1 && static_cast<size_t>(bufferStride[i]) != nCurStride )
        {
            return ReadSlow(arrayStartIdx, count, arrayStep, bufferStride,
                            bufferDataType, pDstBuffer);
        }
        nCurStride *= count[i];
    }

    hid_t hBufferType = H5I_INVALID_HID;
    GByte* pabyTemp = nullptr;
    if( m_dt.GetClass() == GEDTC_STRING )
    {
        if( bufferDataType.GetClass() != GEDTC_STRING )
        {
            return false;
        }
        hBufferType = H5Tcopy(m_hNativeDT);
        if( !H5Tis_variable_str(m_hNativeDT) )
        {
            const size_t nStringSize = H5Tget_size(m_hNativeDT);
            pabyTemp = static_cast<GByte*>(
                VSI_MALLOC2_VERBOSE(nStringSize, nEltCount));
            if( pabyTemp == nullptr )
                return false;
        }
    }
    else if( bufferDataType.GetClass() == GEDTC_NUMERIC &&
             m_dt.GetClass() == GEDTC_NUMERIC &&
             !GDALDataTypeIsComplex(m_dt.GetNumericDataType()) &&
             !GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()) )
    {
        // Compatibility with older libhdf5 that doesn't like requesting
        // an enum to an integer
        if (H5Tget_class(m_hNativeDT) == H5T_ENUM )
        {
            auto hParent = H5Tget_super(m_hNativeDT);
            if( H5Tequal(hParent, H5T_NATIVE_UCHAR) ||
                H5Tequal(hParent, H5T_NATIVE_USHORT) ||
                H5Tequal(hParent, H5T_NATIVE_SHORT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT) ||
                H5Tequal(hParent, H5T_NATIVE_INT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT64) ||
                H5Tequal(hParent, H5T_NATIVE_INT64) )
            {
                hBufferType = H5Tcopy(m_hNativeDT);
                if( m_dt != bufferDataType )
                {
                    const size_t nDataTypeSize = H5Tget_size(m_hNativeDT);
                    pabyTemp = static_cast<GByte*>(
                        VSI_MALLOC2_VERBOSE(nDataTypeSize, nEltCount));
                    if( pabyTemp == nullptr )
                    {
                        H5Tclose(hBufferType);
                        return false;
                    }
                }
            }
            H5Tclose(hParent);
        }
        if( hBufferType == H5I_INVALID_HID )
        {
            hBufferType = GetHDF5DataTypeFromGDALDataType(
                m_dt, m_hNativeDT, bufferDataType);
            if( hBufferType == H5I_INVALID_HID )
            {
                VSIFree(pabyTemp);
                return false;
            }
        }
    }
    else
    {
        hBufferType = H5Tcopy(m_hNativeDT);
        if( m_dt != bufferDataType || m_bHasVLenMember || m_bHasNonNativeDataType )
        {
            const size_t nDataTypeSize = H5Tget_size(m_hNativeDT);
            pabyTemp = static_cast<GByte*>(
                VSI_MALLOC2_VERBOSE(nDataTypeSize, nEltCount));
            if( pabyTemp == nullptr )
            {
                H5Tclose(hBufferType);
                return false;
            }
        }
    }

    // Select block from file space.
    herr_t status;
    if( nDims )
    {
        status = H5Sselect_hyperslab(m_hDataSpace,
                                            H5S_SELECT_SET,
                                            anOffset.data(),
                                            anStep.data(),
                                            anCount.data(),
                                            nullptr);
        if( status < 0 )
        {
            H5Tclose(hBufferType);
            VSIFree(pabyTemp);
            return false;
        }
    }

    // Create memory data space
    const hid_t hMemSpace = nDims == 0 ?
        H5Screate(H5S_SCALAR) :
        H5Screate_simple(static_cast<int>(nDims), anCount.data(), nullptr);
    if( nDims )
    {
        std::vector<H5OFFSET_TYPE> anMemOffset(nDims);
        status =  H5Sselect_hyperslab(hMemSpace,
                                    H5S_SELECT_SET,
                                    anMemOffset.data(),
                                    nullptr,
                                    anCount.data(),
                                    nullptr);
        if( status < 0 )
        {
            H5Tclose(hBufferType);
            H5Sclose(hMemSpace);
            VSIFree(pabyTemp);
            return false;
        }
    }

    status = H5Dread(m_hArray, hBufferType, hMemSpace,
                     m_hDataSpace, H5P_DEFAULT,
                     pabyTemp ? pabyTemp : pDstBuffer);

    if( status >= 0 )
    {
        if( H5Tis_variable_str(hBufferType) )
        {
            IngestVariableStrings(pDstBuffer, hBufferType,
                                nDims, count, bufferStride);
        }
        else if( pabyTemp && bufferDataType.GetClass() == GEDTC_STRING )
        {
            IngestFixedLengthStrings(pDstBuffer, pabyTemp,
                                    hBufferType,
                                    nDims, count, bufferStride);
        }
        else if( pabyTemp )
        {
            CopyToFinalBuffer(pDstBuffer, pabyTemp,
                           nDims, count, bufferStride,
                           m_hNativeDT, bufferDataType);

            if( m_bHasVLenMember )
            {
                const size_t nBufferTypeSize = H5Tget_size(hBufferType);
                GByte* pabyPtr = pabyTemp;
                for(size_t i = 0; i < nEltCount; ++i )
                {
                    FreeDynamicMemory(pabyPtr, hBufferType);
                    pabyPtr += nBufferTypeSize;
                }
            }
        }
    }

    H5Tclose(hBufferType);
    H5Sclose(hMemSpace);
    VSIFree(pabyTemp);

    return status >= 0;
}

/************************************************************************/
/*                           ~HDF5Attribute()                           */
/************************************************************************/

HDF5Attribute::~HDF5Attribute()
{
    if( m_hAttribute > 0 )
        H5Aclose(m_hAttribute);
    if( m_hNativeDT > 0 )
        H5Tclose(m_hNativeDT);
    if( m_hDataSpace > 0 )
        H5Sclose(m_hDataSpace);
}

/************************************************************************/
/*                       CopyAllAttrValuesInto()                        */
/************************************************************************/

static void CopyAllAttrValuesInto(size_t nDims,
                                  const GUInt64* arrayStartIdx,
                                  const size_t* count,
                                  const GInt64* arrayStep,
                                  const GPtrDiff_t* bufferStride,
                                  const GDALExtendedDataType& bufferDataType,
                                  void* pDstBuffer,
                                  hid_t hSrcBufferType,
                                  const void* pabySrcBuffer)
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    const size_t nSrcDataTypeSize(H5Tget_size(hSrcBufferType));
    std::vector<size_t> anStackCount(nDims);
    std::vector<const GByte*> pabySrcBufferStack(nDims + 1);
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    const auto mapDstCompsToSrcComps =
        (H5Tget_class(hSrcBufferType) == H5T_COMPOUND &&
         bufferDataType.GetClass() == GEDTC_COMPOUND) ?
        CreateMapTargetComponentsToSrc(hSrcBufferType, bufferDataType) :
        std::vector<unsigned>();

    pabySrcBufferStack[0] = static_cast<const GByte*>(pabySrcBuffer);
    if( nDims > 0 )
        pabySrcBufferStack[0] += arrayStartIdx[0] * nSrcDataTypeSize;
    pabyDstBufferStack[0] = static_cast<GByte*>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        CopyValue(
            pabySrcBufferStack[nDims], hSrcBufferType,
            pabyDstBufferStack[nDims], bufferDataType,
            mapDstCompsToSrcComps);
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            pabySrcBufferStack[iDim] = pabySrcBufferStack[iDim-1];
            if( iDim < nDims )
            {
                pabySrcBufferStack[iDim] += arrayStartIdx[iDim] * nSrcDataTypeSize;
            }
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * nBufferDataTypeSize;
            pabySrcBufferStack[iDim] += arrayStep[iDim] * nSrcDataTypeSize;
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF5Attribute::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    const size_t nDims(m_dims.size());
    if( m_dt.GetClass() == GEDTC_STRING )
    {
        if( bufferDataType.GetClass() != GEDTC_STRING )
        {
            return false;
        }

        if( !H5Tis_variable_str(m_hNativeDT) )
        {
            const size_t nStringSize = H5Tget_size(m_hNativeDT);
            GByte* pabyTemp = static_cast<GByte*>(
                VSI_CALLOC_VERBOSE(nStringSize, m_nElements));
            if( pabyTemp == nullptr )
                return false;
            if( H5Sget_simple_extent_type(m_hDataSpace) != H5S_NULL &&
                H5Aread(m_hAttribute, m_hNativeDT, pabyTemp) < 0 )
            {
                VSIFree(pabyTemp);
                return false;
            }
            CopyAllAttrValuesInto(nDims,
                     arrayStartIdx, count, arrayStep, bufferStride,
                     bufferDataType, pDstBuffer,
                     m_hNativeDT, pabyTemp);
            VSIFree(pabyTemp);
        }
        else
        {
            void* pabyTemp =
                VSI_CALLOC_VERBOSE(sizeof(char*), m_nElements);
            if( pabyTemp == nullptr )
                return false;
            if( H5Sget_simple_extent_type(m_hDataSpace) != H5S_NULL &&
                H5Aread(m_hAttribute, m_hNativeDT, pabyTemp) < 0 )
            {
                VSIFree(pabyTemp);
                return false;
            }
            CopyAllAttrValuesInto(nDims,
                     arrayStartIdx, count, arrayStep, bufferStride,
                     bufferDataType, pDstBuffer,
                     m_hNativeDT, pabyTemp);
            H5Dvlen_reclaim(m_hNativeDT, m_hDataSpace, H5P_DEFAULT,
                            pabyTemp);
            VSIFree(pabyTemp);
        }
        return true;
    }

    hid_t hBufferType = H5I_INVALID_HID;
    if( m_dt.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC &&
        !GDALDataTypeIsComplex(m_dt.GetNumericDataType()) &&
        !GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()) )
    {
        // Compatibility with older libhdf5 that doesn't like requesting
        // an enum to an integer
        if (H5Tget_class(m_hNativeDT) == H5T_ENUM )
        {
            auto hParent = H5Tget_super(m_hNativeDT);
            if( H5Tequal(hParent, H5T_NATIVE_UCHAR) ||
                H5Tequal(hParent, H5T_NATIVE_USHORT) ||
                H5Tequal(hParent, H5T_NATIVE_SHORT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT) ||
                H5Tequal(hParent, H5T_NATIVE_INT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT64) ||
                H5Tequal(hParent, H5T_NATIVE_INT64))
            {
                hBufferType = H5Tcopy(m_hNativeDT);
            }
            H5Tclose(hParent);
        }
        if( hBufferType == H5I_INVALID_HID )
        {
            hBufferType = GetHDF5DataTypeFromGDALDataType(
                m_dt, m_hNativeDT, bufferDataType);
        }
    }
    else
    {
        hBufferType = H5Tcopy(m_hNativeDT);
    }

    if( hBufferType == H5I_INVALID_HID )
        return false;

    const size_t nBufferTypeSize(H5Tget_size(hBufferType));
    GByte* pabyTemp = static_cast<GByte*>(
            VSI_MALLOC2_VERBOSE(nBufferTypeSize, m_nElements));
    if( pabyTemp == nullptr )
    {
        H5Tclose(hBufferType);
        return false;
    }
    if( H5Aread(m_hAttribute, hBufferType, pabyTemp) < 0 )
    {
        VSIFree(pabyTemp);
        return false;
    }
    CopyAllAttrValuesInto(nDims,
                arrayStartIdx, count, arrayStep, bufferStride,
                bufferDataType, pDstBuffer,
                hBufferType, pabyTemp);
    if( bufferDataType.GetClass() == GEDTC_COMPOUND && m_bHasVLenMember )
    {
        GByte* pabyPtr = pabyTemp;
        for(size_t i = 0; i < m_nElements; ++i )
        {
            FreeDynamicMemory(pabyPtr, hBufferType);
            pabyPtr += nBufferTypeSize;
        }
    }
    VSIFree(pabyTemp);
    H5Tclose(hBufferType);
    return true;
}

/************************************************************************/
/*                         GetIndexingVariable()                        */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF5Dimension::GetIndexingVariable() const
{
    auto hGroup = H5Gopen(m_poShared->GetHDF5(), m_osGroupFullname.c_str());
    if( hGroup >= 0 )
    {
        auto hArray = H5Dopen(hGroup, GetName().c_str());
        H5Gclose(hGroup);
        if( hArray >= 0 )
        {
            auto ar(HDF5Array::Create(m_osGroupFullname, GetName(), m_poShared,
                                    hArray, nullptr, false));
            auto attrName = ar->GetAttribute("NAME");
            if( attrName &&
                attrName->GetDataType().GetClass() == GEDTC_STRING )
            {
                const char* pszName = attrName->ReadAsString();
                if( pszName &&
                    STARTS_WITH(pszName,
                        "This is a netCDF dimension but not a netCDF variable") )
                {
                    return nullptr;
                }
            }
            return ar;
        }
    }
    return nullptr;
}

} // namespace

/************************************************************************/
/*                           OpenMultiDim()                             */
/************************************************************************/

GDALDataset *HDF5Dataset::OpenMultiDim( GDALOpenInfo *poOpenInfo )
{
    const char* pszFilename =
        STARTS_WITH(poOpenInfo->pszFilename, "HDF5:") ?
            poOpenInfo->pszFilename + strlen("HDF5:") :
            poOpenInfo->pszFilename;

    // Try opening the dataset.
    auto hHDF5 = GDAL_HDF5Open(pszFilename);
    if( hHDF5 < 0 )
    {
        return nullptr;
    }

    auto poSharedResources = std::make_shared<GDAL::HDF5SharedResources>(pszFilename);
    poSharedResources->m_hHDF5 = hHDF5;

    auto poGroup(OpenGroup(poSharedResources));
    if( poGroup == nullptr )
    {
        return nullptr;
    }

    auto poDS(new HDF5Dataset());
    poDS->m_poRootGroup = poGroup;

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Setup/check for pam .aux.xml.
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                            OpenGroup()                               */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF5Dataset::OpenGroup(std::shared_ptr<GDAL::HDF5SharedResources> poSharedResources)
{
    H5G_stat_t oStatbuf;
    if(  H5Gget_objinfo(poSharedResources->m_hHDF5, "/", FALSE, &oStatbuf) < 0 )
    {
        return nullptr;
    }
    auto hGroup = H5Gopen(poSharedResources->m_hHDF5, "/");
    if( hGroup < 0 )
    {
        return nullptr;
    }

    return std::shared_ptr<GDALGroup>(new GDAL::HDF5Group(std::string(), "/",
                                                          poSharedResources, {},
                                                          hGroup,
                                                          oStatbuf.objno));
}
