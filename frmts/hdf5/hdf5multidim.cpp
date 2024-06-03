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
#include "hdf5eosparser.h"
#include "s100.h"

#include "cpl_float.h"

#include <algorithm>
#include <set>
#include <utility>

namespace GDAL
{

/************************************************************************/
/*                               HDF5Group                              */
/************************************************************************/

class HDF5Group final : public GDALGroup
{
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t m_hGroup;
    std::set<std::pair<unsigned long, unsigned long>> m_oSetParentIds{};
    const bool m_bIsEOSGridGroup;
    const bool m_bIsEOSSwathGroup;
    mutable std::shared_ptr<GDALMDArray> m_poXIndexingArray{};
    mutable std::shared_ptr<GDALMDArray> m_poYIndexingArray{};
    mutable std::vector<std::string> m_osListSubGroups{};
    mutable std::vector<std::string> m_osListArrays{};
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_oListAttributes{};
    mutable bool m_bShowAllAttributes = false;
    mutable bool m_bGotDims = false;
    mutable std::vector<std::shared_ptr<GDALDimension>> m_cachedDims{};

    static herr_t GetGroupNamesCallback(hid_t hGroup, const char *pszObjName,
                                        void *);

    static herr_t GetArrayNamesCallback(hid_t hGroup, const char *pszObjName,
                                        void *);

    static herr_t GetAttributesCallback(hid_t hGroup, const char *pszObjName,
                                        void *);

  protected:
    HDF5Group(
        const std::string &osParentName, const std::string &osName,
        const std::shared_ptr<HDF5SharedResources> &poShared,
        const std::set<std::pair<unsigned long, unsigned long>> &oSetParentIds,
        hid_t hGroup, unsigned long objIds[2])
        : GDALGroup(osParentName, osName), m_poShared(poShared),
          m_hGroup(hGroup), m_oSetParentIds(oSetParentIds),
          m_bIsEOSGridGroup(osParentName == "/HDFEOS/GRIDS"),
          m_bIsEOSSwathGroup(osParentName == "/HDFEOS/SWATHS")
    {
        m_oSetParentIds.insert(std::pair(objIds[0], objIds[1]));

        // Force registration of EOS dimensions
        if (m_bIsEOSGridGroup || m_bIsEOSSwathGroup)
        {
            HDF5Group::GetDimensions();
        }
    }

  public:
    static std::shared_ptr<HDF5Group> Create(
        const std::string &osParentName, const std::string &osName,
        const std::shared_ptr<HDF5SharedResources> &poShared,
        const std::set<std::pair<unsigned long, unsigned long>> &oSetParentIds,
        hid_t hGroup, unsigned long objIds[2])
    {
        auto poGroup = std::shared_ptr<HDF5Group>(new HDF5Group(
            osParentName, osName, poShared, oSetParentIds, hGroup, objIds));
        poGroup->SetSelf(poGroup);
        return poGroup;
    }

    ~HDF5Group()
    {
        H5Gclose(m_hGroup);
    }

    hid_t GetID() const
    {
        return m_hGroup;
    }

    std::vector<std::shared_ptr<GDALDimension>>
    GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string &osName,
                                         CSLConstList) const override;

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override;
};

/************************************************************************/
/*                             HDF5Dimension                            */
/************************************************************************/

class HDF5Dimension final : public GDALDimension
{
    std::string m_osGroupFullname;
    std::shared_ptr<HDF5SharedResources> m_poShared;

  public:
    HDF5Dimension(const std::string &osParentName, const std::string &osName,
                  const std::string &osType, const std::string &osDirection,
                  GUInt64 nSize,
                  const std::shared_ptr<HDF5SharedResources> &poShared)
        : GDALDimension(osParentName, osName, osType, osDirection, nSize),
          m_osGroupFullname(osParentName), m_poShared(poShared)
    {
    }

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;
};

/************************************************************************/
/*                           BuildDataType()                            */
/************************************************************************/

static GDALExtendedDataType
BuildDataType(hid_t hDataType, bool &bHasString, bool &bNonNativeDataType,
              const std::vector<std::pair<std::string, hid_t>> &oTypes)
{
    const auto klass = H5Tget_class(hDataType);
    GDALDataType eDT = ::HDF5Dataset::GetDataType(hDataType);
    if (eDT != GDT_Unknown)
    {
#ifdef HDF5_HAVE_FLOAT16
        if (H5Tequal(hDataType, H5T_NATIVE_FLOAT16) ||
            HDF5Dataset::IsNativeCFloat16(hDataType))
        {
            bNonNativeDataType = true;
        }
#endif
        return GDALExtendedDataType::Create(eDT);
    }
    else if (klass == H5T_STRING)
    {
        bHasString = true;
        return GDALExtendedDataType::CreateString();
    }
    else if (klass == H5T_COMPOUND)
    {
        const unsigned nMembers = H5Tget_nmembers(hDataType);
        std::vector<std::unique_ptr<GDALEDTComponent>> components;
        size_t nOffset = 0;
        for (unsigned i = 0; i < nMembers; i++)
        {
            char *pszName = H5Tget_member_name(hDataType, i);
            if (!pszName)
                return GDALExtendedDataType::Create(GDT_Unknown);
            CPLString osCompName(pszName);
            H5free_memory(pszName);
            const auto hMemberType = H5Tget_member_type(hDataType, i);
            if (hMemberType < 0)
                return GDALExtendedDataType::Create(GDT_Unknown);
            const hid_t hNativeMemberType =
                H5Tget_native_type(hMemberType, H5T_DIR_ASCEND);
            auto memberDT = BuildDataType(hNativeMemberType, bHasString,
                                          bNonNativeDataType, oTypes);
            H5Tclose(hNativeMemberType);
            H5Tclose(hMemberType);
            if (memberDT.GetClass() == GEDTC_NUMERIC &&
                memberDT.GetNumericDataType() == GDT_Unknown)
                return GDALExtendedDataType::Create(GDT_Unknown);
            if ((nOffset % memberDT.GetSize()) != 0)
                nOffset += memberDT.GetSize() - (nOffset % memberDT.GetSize());
            if (nOffset != H5Tget_member_offset(hDataType, i))
                bNonNativeDataType = true;
            components.emplace_back(std::unique_ptr<GDALEDTComponent>(
                new GDALEDTComponent(osCompName, nOffset, memberDT)));
            nOffset += memberDT.GetSize();
        }
        if (!components.empty() &&
            (nOffset % components[0]->GetType().GetSize()) != 0)
            nOffset += components[0]->GetType().GetSize() -
                       (nOffset % components[0]->GetType().GetSize());
        if (nOffset != H5Tget_size(hDataType))
            bNonNativeDataType = true;
        std::string osName("unnamed");
        for (const auto &oPair : oTypes)
        {
            const auto hPairNativeType =
                H5Tget_native_type(oPair.second, H5T_DIR_ASCEND);
            const auto matches = H5Tequal(hPairNativeType, hDataType);
            H5Tclose(hPairNativeType);
            if (matches)
            {
                osName = oPair.first;
                break;
            }
        }
        return GDALExtendedDataType::Create(osName, nOffset,
                                            std::move(components));
    }
    else if (klass == H5T_ENUM)
    {
        const auto hParent = H5Tget_super(hDataType);
        const hid_t hNativeParent = H5Tget_native_type(hParent, H5T_DIR_ASCEND);
        auto ret(BuildDataType(hNativeParent, bHasString, bNonNativeDataType,
                               oTypes));
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

static void
GetDataTypesInGroup(hid_t hHDF5, const std::string &osGroupFullName,
                    std::vector<std::pair<std::string, hid_t>> &oTypes)
{
    struct Callback
    {
        static herr_t f(hid_t hGroup, const char *pszObjName, void *user_data)
        {
            auto *poTypes =
                static_cast<std::vector<std::pair<std::string, hid_t>> *>(
                    user_data);
            H5G_stat_t oStatbuf;

            if (H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0)
                return -1;

            if (oStatbuf.type == H5G_TYPE)
            {
                poTypes->push_back(
                    std::pair(pszObjName, H5Topen(hGroup, pszObjName)));
            }

            return 0;
        }
    };

    H5Giterate(hHDF5, osGroupFullName.c_str(), nullptr, &(Callback::f),
               &oTypes);
}

/************************************************************************/
/*                            HDF5Array                                 */
/************************************************************************/

class HDF5Array final : public GDALMDArray
{
    std::string m_osGroupFullname;
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t m_hArray;
    hid_t m_hDataSpace;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    hid_t m_hNativeDT = H5I_INVALID_HID;
    mutable std::vector<std::shared_ptr<GDALAttribute>> m_oListAttributes{};
    mutable bool m_bShowAllAttributes = false;
    bool m_bHasString = false;
    bool m_bHasNonNativeDataType = false;
    mutable bool m_bWarnedNoData = false;
    mutable std::vector<GByte> m_abyNoData{};
    mutable std::string m_osUnit{};
    mutable bool m_bHasDimensionList = false;
    mutable bool m_bHasDimensionLabels = false;
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    haddr_t m_nOffset;
    mutable CPLStringList m_aosStructuralInfo{};

    HDF5Array(const std::string &osParentName, const std::string &osName,
              const std::shared_ptr<HDF5SharedResources> &poShared,
              hid_t hArray, const HDF5Group *poGroup,
              bool bSkipFullDimensionInstantiation);

    void InstantiateDimensions(const std::string &osParentName,
                               const HDF5Group *poGroup);

    bool ReadSlow(const GUInt64 *arrayStartIdx, const size_t *count,
                  const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                  const GDALExtendedDataType &bufferDataType,
                  void *pDstBuffer) const;

    static herr_t GetAttributesCallback(hid_t hArray, const char *pszObjName,
                                        void *);

  protected:
    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    ~HDF5Array();

    static std::shared_ptr<HDF5Array>
    Create(const std::string &osParentName, const std::string &osName,
           const std::shared_ptr<HDF5SharedResources> &poShared, hid_t hArray,
           const HDF5Group *poGroup, bool bSkipFullDimensionInstantiation)
    {
        HDF5_GLOBAL_LOCK();

        auto ar(std::shared_ptr<HDF5Array>(
            new HDF5Array(osParentName, osName, poShared, hArray, poGroup,
                          bSkipFullDimensionInstantiation)));
        if (ar->m_dt.GetClass() == GEDTC_NUMERIC &&
            ar->m_dt.GetNumericDataType() == GDT_Unknown)
        {
            return nullptr;
        }
        ar->SetSelf(ar);
        return ar;
    }

    bool IsWritable() const override
    {
        return !m_poShared->IsReadOnly();
    }

    const std::string &GetFilename() const override
    {
        return m_poShared->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_dims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override;

    std::vector<GUInt64> GetBlockSize() const override;

    CSLConstList GetStructuralInfo() const override;

    const void *GetRawNoDataValue() const override
    {
        return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
    }

    const std::string &GetUnit() const override
    {
        return m_osUnit;
    }

    haddr_t GetFileOffset() const
    {
        return m_nOffset;
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poSRS;
    }

    std::vector<std::shared_ptr<GDALMDArray>>
    GetCoordinateVariables() const override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poShared->GetRootGroup();
    }
};

/************************************************************************/
/*                           HDF5Attribute                              */
/************************************************************************/

class HDF5Attribute final : public GDALAttribute
{
    std::shared_ptr<HDF5SharedResources> m_poShared;
    hid_t m_hAttribute;
    hid_t m_hDataSpace;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    hid_t m_hNativeDT = H5I_INVALID_HID;
    size_t m_nElements = 1;
    bool m_bHasString = false;
    bool m_bHasNonNativeDataType = false;

    HDF5Attribute(const std::string &osGroupFullName,
                  const std::string &osParentName, const std::string &osName,
                  const std::shared_ptr<HDF5SharedResources> &poShared,
                  hid_t hAttribute)
        : GDALAbstractMDArray(osParentName, osName),
          GDALAttribute(osParentName, osName), m_poShared(poShared),
          m_hAttribute(hAttribute), m_hDataSpace(H5Aget_space(hAttribute))
    {
        const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
        std::vector<hsize_t> anDimSizes(nDims);
        if (nDims)
        {
            H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
            for (int i = 0; i < nDims; ++i)
            {
                m_nElements *= static_cast<size_t>(anDimSizes[i]);
                if (nDims == 1 && m_nElements == 1)
                {
                    // Expose 1-dim of size 1 as scalar
                    break;
                }
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(), CPLSPrintf("dim%d", i), std::string(),
                    std::string(), anDimSizes[i]));
            }
        }

        const hid_t hDataType = H5Aget_type(hAttribute);
        m_hNativeDT = H5Tget_native_type(hDataType, H5T_DIR_ASCEND);
        H5Tclose(hDataType);

        std::vector<std::pair<std::string, hid_t>> oTypes;
        if (!osGroupFullName.empty() &&
            H5Tget_class(m_hNativeDT) == H5T_COMPOUND)
        {
            GetDataTypesInGroup(m_poShared->GetHDF5(), osGroupFullName, oTypes);
        }

        m_dt = BuildDataType(m_hNativeDT, m_bHasString, m_bHasNonNativeDataType,
                             oTypes);
        for (auto &oPair : oTypes)
            H5Tclose(oPair.second);
        if (m_dt.GetClass() == GEDTC_NUMERIC &&
            m_dt.GetNumericDataType() == GDT_Unknown)
        {
            CPLDebug("HDF5",
                     "Cannot map data type of %s to a type handled by GDAL",
                     osName.c_str());
        }
    }

  protected:
    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    ~HDF5Attribute();

    static std::shared_ptr<HDF5Attribute>
    Create(const std::string &osGroupFullName, const std::string &osParentName,
           const std::string &osName,
           const std::shared_ptr<HDF5SharedResources> &poShared,
           hid_t hAttribute)
    {
        HDF5_GLOBAL_LOCK();

        auto ar(std::shared_ptr<HDF5Attribute>(new HDF5Attribute(
            osGroupFullName, osParentName, osName, poShared, hAttribute)));
        if (ar->m_dt.GetClass() == GEDTC_NUMERIC &&
            ar->m_dt.GetNumericDataType() == GDT_Unknown)
        {
            return nullptr;
        }
        return ar;
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_dims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }
};

/************************************************************************/
/*                        HDF5SharedResources()                         */
/************************************************************************/

HDF5SharedResources::HDF5SharedResources(const std::string &osFilename)
    : m_osFilename(osFilename),
      m_poPAM(std::make_shared<GDALPamMultiDim>(osFilename))
{
}

/************************************************************************/
/*                        ~HDF5SharedResources()                        */
/************************************************************************/

HDF5SharedResources::~HDF5SharedResources()
{
    HDF5_GLOBAL_LOCK();

    if (m_hHDF5 > 0)
        H5Fclose(m_hHDF5);
}

/************************************************************************/
/*                          Create()                                    */
/************************************************************************/

std::shared_ptr<HDF5SharedResources>
HDF5SharedResources::Create(const std::string &osFilename)
{
    auto poSharedResources = std::shared_ptr<HDF5SharedResources>(
        new HDF5SharedResources(osFilename));
    poSharedResources->m_poSelf = poSharedResources;
    return poSharedResources;
}

/************************************************************************/
/*                           GetRootGroup()                             */
/************************************************************************/

std::shared_ptr<HDF5Group> HDF5SharedResources::GetRootGroup()
{

    H5G_stat_t oStatbuf;
    if (H5Gget_objinfo(m_hHDF5, "/", FALSE, &oStatbuf) < 0)
    {
        return nullptr;
    }
    auto hGroup = H5Gopen(m_hHDF5, "/");
    if (hGroup < 0)
    {
        return nullptr;
    }

    auto poSharedResources = m_poSelf.lock();
    CPLAssert(poSharedResources != nullptr);
    return HDF5Group::Create(std::string(), "/", poSharedResources, {}, hGroup,
                             oStatbuf.objno);
}

/************************************************************************/
/*                         GetDimensions()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
HDF5Group::GetDimensions(CSLConstList) const
{
    HDF5_GLOBAL_LOCK();

    if (m_bGotDims)
        return m_cachedDims;

    struct CallbackData
    {
        std::shared_ptr<HDF5SharedResources> poShared{};
        std::string osFullName{};
        std::vector<std::shared_ptr<GDALDimension>> oListDim{};
    };

    struct Callback
    {
        static herr_t f(hid_t hGroup, const char *pszObjName, void *user_data)
        {
            CallbackData *data = static_cast<CallbackData *>(user_data);
            H5G_stat_t oStatbuf;

            if (H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0)
                return -1;

            if (oStatbuf.type == H5G_DATASET)
            {
                auto hArray = H5Dopen(hGroup, pszObjName);
                if (hArray >= 0)
                {
                    auto ar = HDF5Array::Create(data->osFullName, pszObjName,
                                                data->poShared, hArray, nullptr,
                                                true);
                    if (ar && ar->GetDimensionCount() == 1)
                    {
                        auto attrCLASS = ar->GetAttribute("CLASS");
                        if (attrCLASS && attrCLASS->GetDimensionCount() == 0 &&
                            attrCLASS->GetDataType().GetClass() == GEDTC_STRING)
                        {
                            const char *pszStr = attrCLASS->ReadAsString();
                            if (pszStr && EQUAL(pszStr, "DIMENSION_SCALE"))
                            {
                                auto attrNAME = ar->GetAttribute("NAME");
                                if (attrNAME &&
                                    attrNAME->GetDimensionCount() == 0 &&
                                    attrNAME->GetDataType().GetClass() ==
                                        GEDTC_STRING)
                                {
                                    const char *pszName =
                                        attrNAME->ReadAsString();
                                    if (pszName &&
                                        STARTS_WITH(
                                            pszName,
                                            "This is a netCDF dimension but "
                                            "not a netCDF variable"))
                                    {
                                        data->oListDim.emplace_back(
                                            std::make_shared<GDALDimension>(
                                                data->osFullName, pszObjName,
                                                std::string(), std::string(),
                                                ar->GetDimensions()[0]
                                                    ->GetSize()));
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

    if (m_cachedDims.empty() && m_bIsEOSGridGroup)
    {
        const auto poHDF5EOSParser = m_poShared->GetHDF5EOSParser();
        HDF5EOSParser::GridMetadata oGridMetadata;
        if (poHDF5EOSParser &&
            poHDF5EOSParser->GetGridMetadata(GetName(), oGridMetadata))
        {
            double adfGT[6] = {0};
            const bool bHasGT = oGridMetadata.GetGeoTransform(adfGT) &&
                                adfGT[2] == 0 && adfGT[4] == 0;
            for (auto &oDim : oGridMetadata.aoDimensions)
            {
                if (oDim.osName == "XDim" && bHasGT)
                {
                    auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                        GetFullName(), oDim.osName, GDAL_DIM_TYPE_HORIZONTAL_X,
                        std::string(), oDim.nSize);
                    auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                        GetFullName(), oDim.osName, poDim,
                        adfGT[0] + adfGT[1] / 2, adfGT[1], 0);
                    poDim->SetIndexingVariable(poIndexingVar);
                    m_poXIndexingArray = poIndexingVar;
                    m_poShared->KeepRef(poIndexingVar);
                    m_cachedDims.emplace_back(poDim);
                }
                else if (oDim.osName == "YDim" && bHasGT)
                {
                    auto poDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                        GetFullName(), oDim.osName, GDAL_DIM_TYPE_HORIZONTAL_Y,
                        std::string(), oDim.nSize);
                    auto poIndexingVar = GDALMDArrayRegularlySpaced::Create(
                        GetFullName(), oDim.osName, poDim,
                        adfGT[3] + adfGT[5] / 2, adfGT[5], 0);
                    poDim->SetIndexingVariable(poIndexingVar);
                    m_poYIndexingArray = poIndexingVar;
                    m_poShared->KeepRef(poIndexingVar);
                    m_cachedDims.emplace_back(poDim);
                }
                else
                {
                    m_cachedDims.emplace_back(std::make_shared<GDALDimension>(
                        GetFullName(), oDim.osName, std::string(),
                        std::string(), oDim.nSize));
                }
            }
            m_poShared->RegisterEOSGridDimensions(GetName(), m_cachedDims);
        }
    }
    else if (m_cachedDims.empty() && m_bIsEOSSwathGroup)
    {
        const auto poHDF5EOSParser = m_poShared->GetHDF5EOSParser();
        HDF5EOSParser::SwathMetadata oSwathMetadata;
        if (poHDF5EOSParser &&
            poHDF5EOSParser->GetSwathMetadata(GetName(), oSwathMetadata))
        {
            for (auto &oDim : oSwathMetadata.aoDimensions)
            {
                m_cachedDims.emplace_back(std::make_shared<GDALDimension>(
                    GetFullName(), oDim.osName, std::string(), std::string(),
                    oDim.nSize));
            }
            m_poShared->RegisterEOSSwathDimensions(GetName(), m_cachedDims);
        }
    }

    return m_cachedDims;
}

/************************************************************************/
/*                          GetGroupNamesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetGroupNamesCallback(hid_t hGroup, const char *pszObjName,
                                        void *selfIn)
{
    HDF5Group *self = static_cast<HDF5Group *>(selfIn);
    H5G_stat_t oStatbuf;

    if (H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0)
        return -1;

    if (oStatbuf.type == H5G_GROUP)
    {
        if (self->m_oSetParentIds.find(
                std::pair(oStatbuf.objno[0], oStatbuf.objno[1])) ==
            self->m_oSetParentIds.end())
        {
            self->m_osListSubGroups.push_back(pszObjName);
        }
        else
        {
            CPLDebug("HDF5",
                     "Group %s contains a link to group %s which is "
                     "itself, or one of its ancestor.",
                     self->GetFullName().c_str(), pszObjName);
        }
    }
    return 0;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF5Group::GetGroupNames(CSLConstList) const
{
    HDF5_GLOBAL_LOCK();

    m_osListSubGroups.clear();
    H5Giterate(m_poShared->GetHDF5(), GetFullName().c_str(), nullptr,
               GetGroupNamesCallback,
               const_cast<void *>(static_cast<const void *>(this)));
    return m_osListSubGroups;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF5Group::OpenGroup(const std::string &osName,
                                                CSLConstList) const
{
    HDF5_GLOBAL_LOCK();

    if (m_osListSubGroups.empty())
        GetGroupNames(nullptr);
    if (std::find(m_osListSubGroups.begin(), m_osListSubGroups.end(), osName) ==
        m_osListSubGroups.end())
    {
        return nullptr;
    }

    H5G_stat_t oStatbuf;
    if (H5Gget_objinfo(m_hGroup, osName.c_str(), FALSE, &oStatbuf) < 0)
        return nullptr;

    auto hSubGroup = H5Gopen(m_hGroup, osName.c_str());
    if (hSubGroup < 0)
    {
        return nullptr;
    }
    return HDF5Group::Create(GetFullName(), osName, m_poShared, m_oSetParentIds,
                             hSubGroup, oStatbuf.objno);
}

/************************************************************************/
/*                          GetArrayNamesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetArrayNamesCallback(hid_t hGroup, const char *pszObjName,
                                        void *selfIn)
{
    HDF5Group *self = static_cast<HDF5Group *>(selfIn);
    H5G_stat_t oStatbuf;

    if (H5Gget_objinfo(hGroup, pszObjName, FALSE, &oStatbuf) < 0)
        return -1;

    if (oStatbuf.type == H5G_DATASET)
    {
        auto hArray = H5Dopen(hGroup, pszObjName);
        if (hArray >= 0)
        {
            auto ar(HDF5Array::Create(std::string(), pszObjName,
                                      self->m_poShared, hArray, self, true));
            if (ar)
            {
                auto attrNAME = ar->GetAttribute("NAME");
                if (attrNAME && attrNAME->GetDimensionCount() == 0 &&
                    attrNAME->GetDataType().GetClass() == GEDTC_STRING)
                {
                    const char *pszName = attrNAME->ReadAsString();
                    if (pszName &&
                        STARTS_WITH(pszName, "This is a netCDF dimension but "
                                             "not a netCDF variable"))
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
    HDF5_GLOBAL_LOCK();

    m_osListArrays.clear();
    H5Giterate(m_poShared->GetHDF5(), GetFullName().c_str(), nullptr,
               GetArrayNamesCallback,
               const_cast<void *>(static_cast<const void *>(this)));

    if (m_poXIndexingArray)
        m_osListArrays.push_back(m_poXIndexingArray->GetName());
    if (m_poYIndexingArray)
        m_osListArrays.push_back(m_poYIndexingArray->GetName());

    return m_osListArrays;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF5Group::OpenMDArray(const std::string &osName,
                                                    CSLConstList) const
{
    HDF5_GLOBAL_LOCK();

    if (m_osListArrays.empty())
        GetMDArrayNames(nullptr);
    if (std::find(m_osListArrays.begin(), m_osListArrays.end(), osName) ==
        m_osListArrays.end())
    {
        return nullptr;
    }
    if (m_poXIndexingArray && m_poXIndexingArray->GetName() == osName)
        return m_poXIndexingArray;
    if (m_poYIndexingArray && m_poYIndexingArray->GetName() == osName)
        return m_poYIndexingArray;

    auto hArray = H5Dopen(m_hGroup, osName.c_str());
    if (hArray < 0)
    {
        return nullptr;
    }
    return HDF5Array::Create(GetFullName(), osName, m_poShared, hArray, this,
                             false);
}

/************************************************************************/
/*                          GetAttributesCallback()                     */
/************************************************************************/

herr_t HDF5Group::GetAttributesCallback(hid_t hGroup, const char *pszObjName,
                                        void *selfIn)
{
    HDF5Group *self = static_cast<HDF5Group *>(selfIn);
    if (self->m_bShowAllAttributes || (!EQUAL(pszObjName, "_Netcdf4Dimid") &&
                                       !EQUAL(pszObjName, "_NCProperties")))
    {
        hid_t hAttr = H5Aopen_name(hGroup, pszObjName);
        if (hAttr > 0)
        {
            auto attr(HDF5Attribute::Create(self->GetFullName(),
                                            self->GetFullName(), pszObjName,
                                            self->m_poShared, hAttr));
            if (attr)
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

std::vector<std::shared_ptr<GDALAttribute>>
HDF5Group::GetAttributes(CSLConstList papszOptions) const
{
    HDF5_GLOBAL_LOCK();

    m_oListAttributes.clear();
    m_bShowAllAttributes =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    H5Aiterate(m_hGroup, nullptr, GetAttributesCallback,
               const_cast<void *>(static_cast<const void *>(this)));
    return m_oListAttributes;
}

/************************************************************************/
/*                               ~HDF5Array()                           */
/************************************************************************/

HDF5Array::~HDF5Array()
{
    HDF5_GLOBAL_LOCK();

    if (m_hArray > 0)
        H5Dclose(m_hArray);
    if (m_hNativeDT > 0)
        H5Tclose(m_hNativeDT);
    if (m_hDataSpace > 0)
        H5Sclose(m_hDataSpace);
}

/************************************************************************/
/*                                HDF5Array()                           */
/************************************************************************/

HDF5Array::HDF5Array(const std::string &osParentName, const std::string &osName,
                     const std::shared_ptr<HDF5SharedResources> &poShared,
                     hid_t hArray, const HDF5Group *poGroup,
                     bool bSkipFullDimensionInstantiation)
    : GDALAbstractMDArray(osParentName, osName),
      GDALMDArray(osParentName, osName), m_osGroupFullname(osParentName),
      m_poShared(poShared), m_hArray(hArray),
      m_hDataSpace(H5Dget_space(hArray)), m_nOffset(H5Dget_offset(hArray))
{
    const hid_t hDataType = H5Dget_type(hArray);
    m_hNativeDT = H5Tget_native_type(hDataType, H5T_DIR_ASCEND);
    H5Tclose(hDataType);

    std::vector<std::pair<std::string, hid_t>> oTypes;
    if (!osParentName.empty() && H5Tget_class(m_hNativeDT) == H5T_COMPOUND)
    {
        GetDataTypesInGroup(m_poShared->GetHDF5(), osParentName, oTypes);
    }

    m_dt = BuildDataType(m_hNativeDT, m_bHasString, m_bHasNonNativeDataType,
                         oTypes);
    for (auto &oPair : oTypes)
        H5Tclose(oPair.second);

    if (m_dt.GetClass() == GEDTC_NUMERIC &&
        m_dt.GetNumericDataType() == GDT_Unknown)
    {
        CPLDebug("HDF5", "Cannot map data type of %s to a type handled by GDAL",
                 osName.c_str());
        return;
    }

    HDF5Array::GetAttributes();

    // Special case for S102 nodata value that is typically at 1e6
    if (GetFullName() ==
            "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values" &&
        m_dt.GetClass() == GEDTC_COMPOUND &&
        m_dt.GetSize() == 2 * sizeof(float) &&
        m_dt.GetComponents().size() == 2 &&
        m_dt.GetComponents()[0]->GetType().GetNumericDataType() ==
            GDT_Float32 &&
        m_dt.GetComponents()[1]->GetType().GetNumericDataType() == GDT_Float32)
    {
        m_abyNoData.resize(m_dt.GetSize());
        float afNoData[2] = {1e6f, 1e6f};

        if (auto poRootGroup = HDF5Array::GetRootGroup())
        {
            if (const auto poGroupF = poRootGroup->OpenGroup("Group_F"))
            {
                const auto poGroupFArray =
                    poGroupF->OpenMDArray("BathymetryCoverage");
                if (poGroupFArray &&
                    poGroupFArray->GetDataType().GetClass() == GEDTC_COMPOUND &&
                    poGroupFArray->GetDataType().GetComponents().size() == 8 &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[0]
                            ->GetName() == "code" &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[3]
                            ->GetName() == "fillValue" &&
                    poGroupFArray->GetDimensionCount() == 1 &&
                    poGroupFArray->GetDimensions()[0]->GetSize() == 2)
                {
                    auto poFillValue =
                        poGroupFArray->GetView("[\"fillValue\"]");
                    if (poFillValue)
                    {
                        char *pszVal0 = nullptr;
                        char *pszVal1 = nullptr;
                        const GUInt64 anArrayStartIdx0[] = {0};
                        const GUInt64 anArrayStartIdx1[] = {1};
                        const size_t anCount[] = {1};
                        const GInt64 anArrayStep[] = {0};
                        const GPtrDiff_t anBufferStride[] = {0};
                        poFillValue->Read(anArrayStartIdx0, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal0);
                        poFillValue->Read(anArrayStartIdx1, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal1);
                        if (pszVal0 && pszVal1)
                        {
                            afNoData[0] = static_cast<float>(CPLAtof(pszVal0));
                            afNoData[1] = static_cast<float>(CPLAtof(pszVal1));
                        }
                        CPLFree(pszVal0);
                        CPLFree(pszVal1);
                    }
                }
            }
        }

        m_abyNoData.resize(m_dt.GetSize());
        memcpy(m_abyNoData.data(), afNoData, m_abyNoData.size());
    }

    // Special case for S102 QualityOfSurvey nodata value that is typically at 0
    if (GetFullName() ==
            "/QualityOfSurvey/QualityOfSurvey.01/Group_001/values" &&
        m_dt.GetClass() == GEDTC_NUMERIC &&
        m_dt.GetNumericDataType() == GDT_UInt32)
    {
        if (auto poRootGroup = HDF5Array::GetRootGroup())
        {
            if (const auto poGroupF = poRootGroup->OpenGroup("Group_F"))
            {
                const auto poGroupFArray =
                    poGroupF->OpenMDArray("QualityOfSurvey");
                if (poGroupFArray &&
                    poGroupFArray->GetDataType().GetClass() == GEDTC_COMPOUND &&
                    poGroupFArray->GetDataType().GetComponents().size() == 8 &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[0]
                            ->GetName() == "code" &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[3]
                            ->GetName() == "fillValue" &&
                    poGroupFArray->GetDimensionCount() == 1 &&
                    poGroupFArray->GetDimensions()[0]->GetSize() == 1)
                {
                    auto poFillValue =
                        poGroupFArray->GetView("[\"fillValue\"]");
                    if (poFillValue)
                    {
                        char *pszVal0 = nullptr;
                        const GUInt64 anArrayStartIdx0[] = {0};
                        const size_t anCount[] = {1};
                        const GInt64 anArrayStep[] = {0};
                        const GPtrDiff_t anBufferStride[] = {0};
                        poFillValue->Read(anArrayStartIdx0, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal0);
                        if (pszVal0)
                        {
                            const uint32_t nNoData = atoi(pszVal0);
                            m_abyNoData.resize(m_dt.GetSize());
                            memcpy(m_abyNoData.data(), &nNoData,
                                   m_abyNoData.size());
                        }
                        CPLFree(pszVal0);
                    }
                }
            }
        }
    }

    // Special case for S104 nodata value that is typically -9999
    if (STARTS_WITH(GetFullName().c_str(), "/WaterLevel/WaterLevel.01/") &&
        GetFullName().find("/values") != std::string::npos &&
        m_dt.GetClass() == GEDTC_COMPOUND && m_dt.GetSize() == 8 &&
        m_dt.GetComponents().size() == 2 &&
        m_dt.GetComponents()[0]->GetType().GetNumericDataType() ==
            GDT_Float32 &&
        // In theory should be Byte, but 104US00_ches_dcf2_20190606T12Z.h5 uses Int32
        (m_dt.GetComponents()[1]->GetType().GetNumericDataType() == GDT_Byte ||
         m_dt.GetComponents()[1]->GetType().GetNumericDataType() == GDT_Int32))
    {
        m_abyNoData.resize(m_dt.GetSize());
        float fNoData = -9999.0f;

        if (auto poRootGroup = HDF5Array::GetRootGroup())
        {
            if (const auto poGroupF = poRootGroup->OpenGroup("Group_F"))
            {
                const auto poGroupFArray = poGroupF->OpenMDArray("WaterLevel");
                if (poGroupFArray &&
                    poGroupFArray->GetDataType().GetClass() == GEDTC_COMPOUND &&
                    poGroupFArray->GetDataType().GetComponents().size() == 8 &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[0]
                            ->GetName() == "code" &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[3]
                            ->GetName() == "fillValue" &&
                    poGroupFArray->GetDimensionCount() == 1 &&
                    poGroupFArray->GetDimensions()[0]->GetSize() >= 2)
                {
                    auto poFillValue =
                        poGroupFArray->GetView("[\"fillValue\"]");
                    if (poFillValue)
                    {
                        char *pszVal0 = nullptr;
                        const GUInt64 anArrayStartIdx0[] = {0};
                        const size_t anCount[] = {1};
                        const GInt64 anArrayStep[] = {0};
                        const GPtrDiff_t anBufferStride[] = {0};
                        poFillValue->Read(anArrayStartIdx0, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal0);
                        if (pszVal0)
                        {
                            fNoData = static_cast<float>(CPLAtof(pszVal0));
                        }
                        CPLFree(pszVal0);
                    }
                }
            }
        }

        memcpy(m_abyNoData.data(), &fNoData, sizeof(float));
    }

    // Special case for S111 nodata value that is typically -9999
    if (STARTS_WITH(GetFullName().c_str(),
                    "/SurfaceCurrent/SurfaceCurrent.01/") &&
        GetFullName().find("/values") != std::string::npos &&
        m_dt.GetClass() == GEDTC_COMPOUND &&
        m_dt.GetSize() == 2 * sizeof(float) &&
        m_dt.GetComponents().size() == 2 &&
        m_dt.GetComponents()[0]->GetType().GetNumericDataType() ==
            GDT_Float32 &&
        m_dt.GetComponents()[1]->GetType().GetNumericDataType() == GDT_Float32)
    {
        float afNoData[2] = {-9999.0f, -9999.0f};

        if (auto poRootGroup = HDF5Array::GetRootGroup())
        {
            if (const auto poGroupF = poRootGroup->OpenGroup("Group_F"))
            {
                const auto poGroupFArray =
                    poGroupF->OpenMDArray("SurfaceCurrent");
                if (poGroupFArray &&
                    poGroupFArray->GetDataType().GetClass() == GEDTC_COMPOUND &&
                    poGroupFArray->GetDataType().GetComponents().size() == 8 &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[0]
                            ->GetName() == "code" &&
                    poGroupFArray->GetDataType()
                            .GetComponents()[3]
                            ->GetName() == "fillValue" &&
                    poGroupFArray->GetDimensionCount() == 1 &&
                    poGroupFArray->GetDimensions()[0]->GetSize() >= 2)
                {
                    auto poFillValue =
                        poGroupFArray->GetView("[\"fillValue\"]");
                    if (poFillValue)
                    {
                        char *pszVal0 = nullptr;
                        char *pszVal1 = nullptr;
                        const GUInt64 anArrayStartIdx0[] = {0};
                        const GUInt64 anArrayStartIdx1[] = {1};
                        const size_t anCount[] = {1};
                        const GInt64 anArrayStep[] = {0};
                        const GPtrDiff_t anBufferStride[] = {0};
                        poFillValue->Read(anArrayStartIdx0, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal0);
                        poFillValue->Read(anArrayStartIdx1, anCount,
                                          anArrayStep, anBufferStride,
                                          GDALExtendedDataType::CreateString(),
                                          &pszVal1);
                        if (pszVal0 && pszVal1)
                        {
                            afNoData[0] = static_cast<float>(CPLAtof(pszVal0));
                            afNoData[1] = static_cast<float>(CPLAtof(pszVal1));
                        }
                        CPLFree(pszVal0);
                        CPLFree(pszVal1);
                    }
                }
            }
        }

        m_abyNoData.resize(m_dt.GetSize());
        memcpy(m_abyNoData.data(), afNoData, m_abyNoData.size());
    }

    if (bSkipFullDimensionInstantiation)
    {
        const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
        std::vector<hsize_t> anDimSizes(nDims);
        if (nDims)
        {
            H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
            for (int i = 0; i < nDims; ++i)
            {
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(), CPLSPrintf("dim%d", i), std::string(),
                    std::string(), anDimSizes[i]));
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

void HDF5Array::InstantiateDimensions(const std::string &osParentName,
                                      const HDF5Group *poGroup)
{
    const int nDims = H5Sget_simple_extent_ndims(m_hDataSpace);
    std::vector<hsize_t> anDimSizes(nDims);
    if (nDims)
    {
        H5Sget_simple_extent_dims(m_hDataSpace, &anDimSizes[0], nullptr);
    }

    if (nDims == 1)
    {
        auto attrCLASS = GetAttribute("CLASS");
        if (attrCLASS && attrCLASS->GetDimensionCount() == 0 &&
            attrCLASS->GetDataType().GetClass() == GEDTC_STRING)
        {
            const char *pszStr = attrCLASS->ReadAsString();
            if (pszStr && EQUAL(pszStr, "DIMENSION_SCALE"))
            {
                auto attrName = GetAttribute("NAME");
                if (attrName &&
                    attrName->GetDataType().GetClass() == GEDTC_STRING)
                {
                    const char *pszName = attrName->ReadAsString();
                    if (pszName &&
                        STARTS_WITH(pszName, "This is a netCDF dimension but "
                                             "not a netCDF variable"))
                    {
                        m_dims.emplace_back(std::make_shared<GDALDimension>(
                            std::string(), GetName(), std::string(),
                            std::string(), anDimSizes[0]));
                        return;
                    }
                }

                m_dims.emplace_back(std::make_shared<HDF5Dimension>(
                    osParentName, GetName(), std::string(), std::string(),
                    anDimSizes[0], m_poShared));
                return;
            }
        }
    }

    std::map<size_t, std::string> mapDimIndexToDimFullName;

    if (m_bHasDimensionList)
    {
        hid_t hAttr = H5Aopen_name(m_hArray, "DIMENSION_LIST");
        const hid_t hAttrDataType = H5Aget_type(hAttr);
        const hid_t hAttrSpace = H5Aget_space(hAttr);
        if (H5Tget_class(hAttrDataType) == H5T_VLEN &&
            H5Sget_simple_extent_ndims(hAttrSpace) == 1)
        {
            const hid_t hBaseType = H5Tget_super(hAttrDataType);
            if (H5Tget_class(hBaseType) == H5T_REFERENCE)
            {
                hsize_t nSize = 0;
                H5Sget_simple_extent_dims(hAttrSpace, &nSize, nullptr);
                if (nSize == static_cast<hsize_t>(nDims))
                {
                    std::vector<hvl_t> aHvl(static_cast<size_t>(nSize));
                    H5Aread(hAttr, hAttrDataType, &aHvl[0]);
                    for (size_t i = 0; i < nSize; i++)
                    {
                        if (aHvl[i].len == 1 &&
                            H5Rget_obj_type(m_hArray, H5R_OBJECT, aHvl[i].p) ==
                                H5G_DATASET)
                        {
                            std::string referenceName;
                            referenceName.resize(256);
                            auto ret = H5Rget_name(
                                m_poShared->GetHDF5(), H5R_OBJECT, aHvl[i].p,
                                &referenceName[0], referenceName.size());
                            if (ret > 0)
                            {
                                referenceName.resize(ret);
                                mapDimIndexToDimFullName[i] =
                                    std::move(referenceName);
                            }
                        }
                    }
                    H5Dvlen_reclaim(hAttrDataType, hAttrSpace, H5P_DEFAULT,
                                    &aHvl[0]);
                }
            }
            H5Tclose(hBaseType);
        }
        H5Tclose(hAttrDataType);
        H5Sclose(hAttrSpace);
        H5Aclose(hAttr);
    }
    else if (m_bHasDimensionLabels)
    {
        hid_t hAttr = H5Aopen_name(m_hArray, "DIMENSION_LABELS");
        auto attr(HDF5Attribute::Create(m_osGroupFullname, GetFullName(),
                                        "DIMENSION_LABELS", m_poShared, hAttr));
        if (attr && attr->GetDimensionCount() == 1 &&
            attr->GetDataType().GetClass() == GEDTC_STRING)
        {
            auto aosList = attr->ReadAsStringArray();
            if (aosList.size() == nDims)
            {
                for (int i = 0; i < nDims; ++i)
                {
                    if (aosList[i][0] != '\0')
                    {
                        mapDimIndexToDimFullName[i] = aosList[i];
                    }
                }
            }
        }
    }
    else
    {
        // Use HDF-EOS5 metadata if available to create dimensions

        HDF5EOSParser::GridDataFieldMetadata oGridDataFieldMetadata;
        HDF5EOSParser::SwathDataFieldMetadata oSwathDataFieldMetadata;
        HDF5EOSParser::SwathGeolocationFieldMetadata
            oSwathGeolocationFieldMetadata;
        const auto poHDF5EOSParser = m_poShared->GetHDF5EOSParser();
        // Build a "classic" subdataset name from group and array names
        const std::string osSubdatasetName(
            "/" +
            CPLString(osParentName)
                .replaceAll("Data Fields", "Data_Fields")
                .replaceAll("Geolocation Fields", "Geolocation_Fields") +
            "/" + GetName());
        if (poHDF5EOSParser &&
            poHDF5EOSParser->GetGridDataFieldMetadata(osSubdatasetName.c_str(),
                                                      oGridDataFieldMetadata) &&
            oGridDataFieldMetadata.aoDimensions.size() ==
                static_cast<size_t>(nDims))
        {
            std::map<std::string, std::shared_ptr<GDALDimension>> oMap;
            const auto groupDims = m_poShared->GetEOSGridDimensions(
                oGridDataFieldMetadata.poGridMetadata->osGridName);
            for (const auto &dim : groupDims)
            {
                oMap[dim->GetName()] = dim;
            }
            int iDimX = 0;
            int iDimY = 0;
            int iCount = 1;
            for (const auto &oDim : oGridDataFieldMetadata.aoDimensions)
            {
                auto oIter = oMap.find(oDim.osName);
                // HDF5EOSParser guarantees that
                CPLAssert(oIter != oMap.end());
                const auto &poDim = oIter->second;
                if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
                    iDimX = iCount;
                else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
                    iDimY = iCount;
                ++iCount;
                m_dims.emplace_back(poDim);
            }

            auto poSRS = oGridDataFieldMetadata.poGridMetadata->GetSRS();
            if (poSRS)
            {
                m_poSRS = std::shared_ptr<OGRSpatialReference>(poSRS->Clone());
                if (iDimX > 0 && iDimY > 0)
                {
                    if (m_poSRS->GetDataAxisToSRSAxisMapping() ==
                        std::vector<int>{2, 1})
                        m_poSRS->SetDataAxisToSRSAxisMapping({iDimY, iDimX});
                    else
                        m_poSRS->SetDataAxisToSRSAxisMapping({iDimX, iDimY});
                }
            }

            return;
        }
        else if (poHDF5EOSParser &&
                 poHDF5EOSParser->GetSwathDataFieldMetadata(
                     osSubdatasetName.c_str(), oSwathDataFieldMetadata) &&
                 oSwathDataFieldMetadata.aoDimensions.size() ==
                     static_cast<size_t>(nDims))
        {
            std::map<std::string, std::shared_ptr<GDALDimension>> oMap;
            const auto groupDims = m_poShared->GetEOSSwathDimensions(
                oSwathDataFieldMetadata.poSwathMetadata->osSwathName);
            for (const auto &dim : groupDims)
            {
                oMap[dim->GetName()] = dim;
            }
            for (const auto &oDim : oSwathDataFieldMetadata.aoDimensions)
            {
                auto oIter = oMap.find(oDim.osName);
                // HDF5EOSParser guarantees that
                CPLAssert(oIter != oMap.end());
                const auto &poDim = oIter->second;
                m_dims.emplace_back(poDim);
            }

            return;
        }
        else if (poHDF5EOSParser &&
                 poHDF5EOSParser->GetSwathGeolocationFieldMetadata(
                     osSubdatasetName.c_str(),
                     oSwathGeolocationFieldMetadata) &&
                 oSwathGeolocationFieldMetadata.aoDimensions.size() ==
                     static_cast<size_t>(nDims))
        {
            std::map<std::string, std::shared_ptr<GDALDimension>> oMap;
            const auto groupDims = m_poShared->GetEOSSwathDimensions(
                oSwathGeolocationFieldMetadata.poSwathMetadata->osSwathName);
            for (const auto &dim : groupDims)
            {
                oMap[dim->GetName()] = dim;
            }
            for (const auto &oDim : oSwathGeolocationFieldMetadata.aoDimensions)
            {
                auto oIter = oMap.find(oDim.osName);
                // HDF5EOSParser guarantees that
                CPLAssert(oIter != oMap.end());
                const auto &poDim = oIter->second;
                m_dims.emplace_back(poDim);
            }
            return;
        }

        // Special case for S100-family of products (S102, S104, S111)
        const auto SpecialCaseS100 = [&](const std::string &osCoverageName)
        {
            auto poRootGroup = m_poShared->GetRootGroup();
            if (poRootGroup)
            {
                m_poSRS = std::make_shared<OGRSpatialReference>();
                if (S100ReadSRS(poRootGroup.get(), *(m_poSRS.get())))
                {
                    if (m_poSRS->GetDataAxisToSRSAxisMapping() ==
                        std::vector<int>{2, 1})
                        m_poSRS->SetDataAxisToSRSAxisMapping({1, 2});
                    else
                        m_poSRS->SetDataAxisToSRSAxisMapping({2, 1});
                }
                else
                {
                    m_poSRS.reset();
                }

                auto poCoverage =
                    poRootGroup->OpenGroupFromFullname(osCoverageName);
                if (poCoverage)
                {
                    std::vector<std::shared_ptr<GDALMDArray>> apoIndexingVars;
                    if (S100GetDimensions(poCoverage.get(), m_dims,
                                          apoIndexingVars) &&
                        m_dims.size() == 2 &&
                        m_dims[0]->GetSize() == anDimSizes[0] &&
                        m_dims[1]->GetSize() == anDimSizes[1])
                    {
                        for (const auto &poIndexingVar : apoIndexingVars)
                            m_poShared->KeepRef(poIndexingVar);
                        return true;
                    }
                    else
                    {
                        m_dims.clear();
                    }
                }
            }
            return false;
        };

        if (nDims == 2 &&
            GetFullName() ==
                "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values")
        {
            // S102
            if (SpecialCaseS100("/BathymetryCoverage/BathymetryCoverage.01"))
                return;
        }
        else if (nDims == 2 &&
                 GetFullName() ==
                     "/QualityOfSurvey/QualityOfSurvey.01/Group_001/values")
        {
            // S102
            if (SpecialCaseS100("/QualityOfSurvey/QualityOfSurvey.01"))
                return;
        }
        else if (nDims == 2 &&
                 STARTS_WITH(GetFullName().c_str(),
                             "/WaterLevel/WaterLevel.01/") &&
                 GetFullName().find("/values"))
        {
            // S104
            if (SpecialCaseS100("/WaterLevel/WaterLevel.01"))
                return;
        }
        else if (nDims == 2 &&
                 STARTS_WITH(GetFullName().c_str(),
                             "/SurfaceCurrent/SurfaceCurrent.01/") &&
                 GetFullName().find("/values"))
        {
            // S111
            if (SpecialCaseS100("/SurfaceCurrent/SurfaceCurrent.01"))
                return;
        }
    }

    std::map<std::string, std::shared_ptr<GDALDimension>> oMapFullNameToDim;
    // cppcheck-suppress knownConditionTrueFalse
    if (poGroup && !mapDimIndexToDimFullName.empty())
    {
        auto groupDims = poGroup->GetDimensions();
        for (const auto &dim : groupDims)
        {
            oMapFullNameToDim[dim->GetFullName()] = dim;
        }
    }

    for (int i = 0; i < nDims; ++i)
    {
        auto oIter = mapDimIndexToDimFullName.find(static_cast<size_t>(i));
        if (oIter != mapDimIndexToDimFullName.end())
        {
            auto oIter2 = oMapFullNameToDim.find(oIter->second);
            if (oIter2 != oMapFullNameToDim.end())
            {
                m_dims.emplace_back(oIter2->second);
                continue;
            }

            std::string osDimName(oIter->second);
            auto nPos = osDimName.rfind('/');
            if (nPos != std::string::npos)
            {
                const std::string osDimParentName(osDimName.substr(0, nPos));
                osDimName = osDimName.substr(nPos + 1);

                m_dims.emplace_back(std::make_shared<HDF5Dimension>(
                    osDimParentName.empty() ? "/" : osDimParentName, osDimName,
                    std::string(), std::string(), anDimSizes[i], m_poShared));
            }
            else
            {
                m_dims.emplace_back(std::make_shared<GDALDimension>(
                    std::string(), osDimName, std::string(), std::string(),
                    anDimSizes[i]));
            }
        }
        else
        {
            m_dims.emplace_back(std::make_shared<GDALDimension>(
                std::string(), CPLSPrintf("dim%d", i), std::string(),
                std::string(), anDimSizes[i]));
        }
    }
}

/************************************************************************/
/*                      GetCoordinateVariables()                        */
/************************************************************************/

std::vector<std::shared_ptr<GDALMDArray>>
HDF5Array::GetCoordinateVariables() const
{
    std::vector<std::shared_ptr<GDALMDArray>> ret;

    HDF5EOSParser::SwathDataFieldMetadata oSwathDataFieldMetadata;
    const auto poHDF5EOSParser = m_poShared->GetHDF5EOSParser();
    // Build a "classic" subdataset name from group and array names
    const std::string osSubdatasetName(
        "/" +
        CPLString(GetFullName()).replaceAll("Data Fields", "Data_Fields"));
    if (poHDF5EOSParser &&
        poHDF5EOSParser->GetSwathDataFieldMetadata(osSubdatasetName.c_str(),
                                                   oSwathDataFieldMetadata) &&
        oSwathDataFieldMetadata.aoDimensions.size() == GetDimensionCount())
    {
        if (!oSwathDataFieldMetadata.osLongitudeSubdataset.empty() &&
            oSwathDataFieldMetadata.nPixelOffset == 0 &&
            oSwathDataFieldMetadata.nLineOffset == 0 &&
            oSwathDataFieldMetadata.nPixelStep == 1 &&
            oSwathDataFieldMetadata.nLineStep == 1)
        {
            auto poRootGroup = m_poShared->GetRootGroup();
            if (poRootGroup)
            {
                auto poLongitude = poRootGroup->OpenMDArrayFromFullname(
                    CPLString(
                        oSwathDataFieldMetadata.osLongitudeSubdataset.substr(1))
                        .replaceAll("Geolocation_Fields",
                                    "Geolocation Fields"));
                auto poLatitude = poRootGroup->OpenMDArrayFromFullname(
                    CPLString(
                        oSwathDataFieldMetadata.osLatitudeSubdataset.substr(1))
                        .replaceAll("Geolocation_Fields",
                                    "Geolocation Fields"));
                if (poLongitude && poLatitude)
                {
                    ret.push_back(poLongitude);
                    ret.push_back(poLatitude);
                }
            }
        }
    }

    return ret;
}

/************************************************************************/
/*                          GetAttributesCallback()                     */
/************************************************************************/

herr_t HDF5Array::GetAttributesCallback(hid_t hArray, const char *pszObjName,
                                        void *selfIn)
{
    HDF5Array *self = static_cast<HDF5Array *>(selfIn);
    if (self->m_bShowAllAttributes ||
        (strcmp(pszObjName, "_Netcdf4Dimid") != 0 &&
         strcmp(pszObjName, "_Netcdf4Coordinates") != 0 &&
         strcmp(pszObjName, "CLASS") != 0 && strcmp(pszObjName, "NAME") != 0))
    {
        if (EQUAL(pszObjName, "DIMENSION_LIST"))
        {
            self->m_bHasDimensionList = true;
            if (!self->m_bShowAllAttributes)
                return 0;
        }
        if (EQUAL(pszObjName, "DIMENSION_LABELS"))
        {
            self->m_bHasDimensionLabels = true;
            if (!self->m_bShowAllAttributes)
                return 0;
        }

        hid_t hAttr = H5Aopen_name(hArray, pszObjName);
        if (hAttr > 0)
        {
            auto attr(HDF5Attribute::Create(self->m_osGroupFullname,
                                            self->GetFullName(), pszObjName,
                                            self->m_poShared, hAttr));
            if (attr)
            {
                // Used by HDF5-EOS products
                if (EQUAL(pszObjName, "_FillValue") &&
                    self->GetDataType().GetClass() == GEDTC_NUMERIC &&
                    attr->GetDataType().GetClass() == GEDTC_NUMERIC &&
                    attr->GetDimensionCount() == 0)
                {
                    auto oRawResult(attr->ReadAsRaw());
                    if (oRawResult.data())
                    {
                        // Round-trip attribute value to target data type and back
                        // to attribute data type to ensure there is no loss
                        // Normally _FillValue data type should be the same
                        // as the array one, but this is not always the case.
                        // For example NASA GEDI L2B products have Float64
                        // _FillValue for Float32 variables.
                        self->m_abyNoData.resize(self->GetDataType().GetSize());
                        GDALExtendedDataType::CopyValue(
                            oRawResult.data(), attr->GetDataType(),
                            self->m_abyNoData.data(), self->GetDataType());
                        std::vector<GByte> abyTmp(
                            attr->GetDataType().GetSize());
                        GDALExtendedDataType::CopyValue(
                            self->m_abyNoData.data(), self->GetDataType(),
                            abyTmp.data(), attr->GetDataType());
                        std::vector<GByte> abyOri;
                        abyOri.assign(oRawResult.data(),
                                      oRawResult.data() + oRawResult.size());
                        if (abyOri == abyTmp)
                        {
                            if (!self->m_bShowAllAttributes)
                                return 0;
                        }
                        else
                        {
                            self->m_abyNoData.clear();
                            if (!self->m_bWarnedNoData)
                            {
                                self->m_bWarnedNoData = true;
                                char *pszVal = nullptr;
                                GDALExtendedDataType::CopyValue(
                                    oRawResult.data(), attr->GetDataType(),
                                    &pszVal,
                                    GDALExtendedDataType::CreateString());
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Array %s: %s attribute value (%s) is "
                                         "not in "
                                         "the range of the "
                                         "array data type (%s)",
                                         self->GetName().c_str(), pszObjName,
                                         pszVal ? pszVal : "(null)",
                                         GDALGetDataTypeName(
                                             self->GetDataType()
                                                 .GetNumericDataType()));
                                CPLFree(pszVal);
                            }
                        }
                    }
                }

                if (EQUAL(pszObjName, "units") &&
                    attr->GetDataType().GetClass() == GEDTC_STRING &&
                    attr->GetDimensionCount() == 0)
                {
                    const char *pszStr = attr->ReadAsString();
                    if (pszStr)
                    {
                        self->m_osUnit = pszStr;
                        if (!self->m_bShowAllAttributes)
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
std::shared_ptr<GDALAttribute>
HDF5Array::GetAttribute(const std::string &osName) const
{
    const char *const apszOptions[] = {"SHOW_ALL=YES", nullptr};
    if (!m_bShowAllAttributes)
        GetAttributes(apszOptions);
    for (const auto &attr : m_oListAttributes)
    {
        if (attr->GetName() == osName)
            return attr;
    }
    return nullptr;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
HDF5Array::GetAttributes(CSLConstList papszOptions) const
{
    HDF5_GLOBAL_LOCK();

    m_oListAttributes.clear();
    m_bShowAllAttributes =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    H5Aiterate(m_hArray, nullptr, GetAttributesCallback,
               const_cast<void *>(static_cast<const void *>(this)));
    return m_oListAttributes;
}

/************************************************************************/
/*                           GetBlockSize()                             */
/************************************************************************/

std::vector<GUInt64> HDF5Array::GetBlockSize() const
{
    HDF5_GLOBAL_LOCK();

    const auto nDimCount = GetDimensionCount();
    std::vector<GUInt64> res(nDimCount);
    if (res.empty())
        return res;

    const hid_t nListId = H5Dget_create_plist(m_hArray);
    if (nListId > 0)
    {
        if (H5Pget_layout(nListId) == H5D_CHUNKED)
        {
            std::vector<hsize_t> anChunkDims(nDimCount);
            const int nDimSize = H5Pget_chunk(
                nListId, static_cast<int>(nDimCount), &anChunkDims[0]);
            if (static_cast<size_t>(nDimSize) == nDimCount)
            {
                for (size_t i = 0; i < nDimCount; ++i)
                {
                    res[i] = anChunkDims[i];
                }
            }
        }

        H5Pclose(nListId);
    }

    return res;
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

CSLConstList HDF5Array::GetStructuralInfo() const
{
    if (m_aosStructuralInfo.empty())
    {
        HDF5_GLOBAL_LOCK();
        const hid_t nListId = H5Dget_create_plist(m_hArray);
        if (nListId > 0)
        {
            const int nFilters = H5Pget_nfilters(nListId);
            for (int i = 0; i < nFilters; ++i)
            {
                unsigned int flags = 0;
                size_t cd_nelmts = 0;
                char szName[64 + 1] = {0};
                const auto eFilter = H5Pget_filter(
                    nListId, i, &flags, &cd_nelmts, nullptr, 64, szName);
                if (eFilter == H5Z_FILTER_DEFLATE)
                {
                    m_aosStructuralInfo.SetNameValue("COMPRESSION", "DEFLATE");
                }
                else if (eFilter == H5Z_FILTER_SZIP)
                {
                    m_aosStructuralInfo.SetNameValue("COMPRESSION", "SZIP");
                }
                else if (eFilter == H5Z_FILTER_SHUFFLE)
                {
                    m_aosStructuralInfo.SetNameValue("FILTER", "SHUFFLE");
                }
                else
                {
                    CPLDebug("HDF5", "Filter used: %s", szName);
                }
            }
            H5Pclose(nListId);
        }
    }
    return m_aosStructuralInfo.List();
}

/************************************************************************/
/*                           CopyBuffer()                               */
/************************************************************************/

static void CopyBuffer(size_t nDims, const size_t *count,
                       const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                       const GDALExtendedDataType &bufferDataType,
                       GByte *pabySrc, void *pDstBuffer)
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabySrcBufferStack(nDims + 1);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    std::vector<GPtrDiff_t> anSrcStride(nDims);
    std::vector<size_t> anSrcOffset(nDims + 1);
    size_t nCurStride = nBufferDataTypeSize;
    for (size_t i = nDims; i > 0;)
    {
        --i;
        anSrcStride[i] = arrayStep[i] > 0
                             ? nCurStride
                             : -static_cast<GPtrDiff_t>(nCurStride);
        anSrcOffset[i] = arrayStep[i] > 0 ? 0 : (count[i] - 1) * nCurStride;
        nCurStride *= count[i];
    }
    pabySrcBufferStack[0] = pabySrc + anSrcOffset[0];
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if (iDim == nDims)
    {
        memcpy(pabyDstBufferStack[nDims], pabySrcBufferStack[nDims],
               nBufferDataTypeSize);
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabySrcBufferStack[iDim] =
                pabySrcBufferStack[iDim - 1] + anSrcOffset[iDim];
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] +=
                bufferStride[iDim] * nBufferDataTypeSize;
            pabySrcBufferStack[iDim] += anSrcStride[iDim];
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                             ReadSlow()                               */
/************************************************************************/

bool HDF5Array::ReadSlow(const GUInt64 *arrayStartIdx, const size_t *count,
                         const GInt64 *arrayStep,
                         const GPtrDiff_t *bufferStride,
                         const GDALExtendedDataType &bufferDataType,
                         void *pDstBuffer) const
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    if (nBufferDataTypeSize == 0)
        return false;
    const size_t nDims(m_dims.size());
    size_t nEltCount = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        nEltCount *= count[i];
    }

    // Only for testing
    const char *pszThreshold =
        CPLGetConfigOption("GDAL_HDF5_TEMP_ARRAY_ALLOC_SIZE", "16777216");
    const GUIntBig nThreshold =
        CPLScanUIntBig(pszThreshold, static_cast<int>(strlen(pszThreshold)));
    if (nEltCount == 1 || nEltCount <= nThreshold / nBufferDataTypeSize)
    {
        CPLDebug("HDF5", "Using slow path");
        std::vector<GByte> abyTemp;
        try
        {
            abyTemp.resize(nEltCount * nBufferDataTypeSize);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
        std::vector<GUInt64> anStart(nDims);
        std::vector<GInt64> anStep(nDims);
        for (size_t i = 0; i < nDims; i++)
        {
            if (arrayStep[i] >= 0)
            {
                anStart[i] = arrayStartIdx[i];
                anStep[i] = arrayStep[i];
            }
            else
            {
                // Use double negation so that operations occur only on
                // positive quantities to avoid an artificial negative signed
                // integer to unsigned conversion.
                anStart[i] =
                    arrayStartIdx[i] - ((-arrayStep[i]) * (count[i] - 1));
                anStep[i] = -arrayStep[i];
            }
        }
        std::vector<GPtrDiff_t> anStride(nDims);
        size_t nCurStride = 1;
        for (size_t i = nDims; i > 0;)
        {
            --i;
            anStride[i] = nCurStride;
            nCurStride *= count[i];
        }
        if (!IRead(anStart.data(), count, anStep.data(), anStride.data(),
                   bufferDataType, &abyTemp[0]))
        {
            return false;
        }
        CopyBuffer(nDims, count, arrayStep, bufferStride, bufferDataType,
                   &abyTemp[0], pDstBuffer);
        return true;
    }

    std::vector<GUInt64> arrayStartIdxHalf;
    std::vector<size_t> countHalf;
    size_t iDimToSplit = nDims;
    // Find the first dimension that has at least 2 elements, to split along
    // it
    for (size_t i = 0; i < nDims; ++i)
    {
        arrayStartIdxHalf.push_back(arrayStartIdx[i]);
        countHalf.push_back(count[i]);
        if (count[i] >= 2 && iDimToSplit == nDims)
        {
            iDimToSplit = i;
        }
    }

    CPLAssert(iDimToSplit != nDims);

    countHalf[iDimToSplit] /= 2;
    if (!ReadSlow(arrayStartIdxHalf.data(), countHalf.data(), arrayStep,
                  bufferStride, bufferDataType, pDstBuffer))
    {
        return false;
    }
    arrayStartIdxHalf[iDimToSplit] = static_cast<GUInt64>(
        arrayStep[iDimToSplit] > 0
            ? arrayStartIdx[iDimToSplit] +
                  arrayStep[iDimToSplit] * countHalf[iDimToSplit]
            : arrayStartIdx[iDimToSplit] -
                  (-arrayStep[iDimToSplit]) * countHalf[iDimToSplit]);
    GByte *pOtherHalfDstBuffer =
        static_cast<GByte *>(pDstBuffer) + bufferStride[iDimToSplit] *
                                               countHalf[iDimToSplit] *
                                               nBufferDataTypeSize;
    countHalf[iDimToSplit] = count[iDimToSplit] - countHalf[iDimToSplit];
    return ReadSlow(arrayStartIdxHalf.data(), countHalf.data(), arrayStep,
                    bufferStride, bufferDataType, pOtherHalfDstBuffer);
}

/************************************************************************/
/*                       IngestVariableStrings()                        */
/************************************************************************/

static void IngestVariableStrings(void *pDstBuffer, hid_t hBufferType,
                                  size_t nDims, const size_t *count,
                                  const GPtrDiff_t *bufferStride)
{
    std::vector<hsize_t> anCountOne(nDims, 1);
    const hid_t hMemSpaceOne =
        nDims == 0 ? H5Screate(H5S_SCALAR)
                   : H5Screate_simple(static_cast<int>(nDims),
                                      anCountOne.data(), nullptr);
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if (iDim == nDims)
    {
        void *old_ptr = pabyDstBufferStack[nDims];
        const char *pszSrcStr = *static_cast<char **>(old_ptr);
        char *pszNewStr = pszSrcStr ? VSIStrdup(pszSrcStr) : nullptr;
        H5Dvlen_reclaim(hBufferType, hMemSpaceOne, H5P_DEFAULT, old_ptr);
        *static_cast<char **>(old_ptr) = pszNewStr;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * sizeof(char *);
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
    H5Sclose(hMemSpaceOne);
}

/************************************************************************/
/*                    IngestFixedLengthStrings()                        */
/************************************************************************/

static void IngestFixedLengthStrings(void *pDstBuffer, const void *pTemp,
                                     hid_t hBufferType, size_t nDims,
                                     const size_t *count,
                                     const GPtrDiff_t *bufferStride)
{
    const size_t nStringSize = H5Tget_size(hBufferType);
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    const GByte *pabySrcBuffer = static_cast<const GByte *>(pTemp);
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;
    const bool bSpacePad = H5Tget_strpad(hBufferType) == H5T_STR_SPACEPAD;
lbl_next_depth:
    if (iDim == nDims)
    {
        char *pszStr = static_cast<char *>(VSIMalloc(nStringSize + 1));
        if (pszStr)
        {
            memcpy(pszStr, pabySrcBuffer, nStringSize);
            size_t nIter = nStringSize;
            if (bSpacePad)
            {
                while (nIter >= 1 && pszStr[nIter - 1] == ' ')
                {
                    nIter--;
                }
            }
            pszStr[nIter] = 0;
        }
        void *ptr = pabyDstBufferStack[nDims];
        *static_cast<char **>(ptr) = pszStr;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * sizeof(char *);
            pabySrcBuffer += nStringSize;
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                   GetHDF5DataTypeFromGDALDataType()                  */
/************************************************************************/

static hid_t
GetHDF5DataTypeFromGDALDataType(const GDALExtendedDataType &dt, hid_t hNativeDT,
                                const GDALExtendedDataType &bufferDataType)
{
    hid_t hBufferType = H5I_INVALID_HID;
    switch (bufferDataType.GetNumericDataType())
    {
        case GDT_Byte:
            hBufferType = H5Tcopy(H5T_NATIVE_UCHAR);
            break;
        case GDT_Int8:
            hBufferType = H5Tcopy(H5T_NATIVE_SCHAR);
            break;
        case GDT_UInt16:
            hBufferType = H5Tcopy(H5T_NATIVE_USHORT);
            break;
        case GDT_Int16:
            hBufferType = H5Tcopy(H5T_NATIVE_SHORT);
            break;
        case GDT_UInt32:
            hBufferType = H5Tcopy(H5T_NATIVE_UINT);
            break;
        case GDT_Int32:
            hBufferType = H5Tcopy(H5T_NATIVE_INT);
            break;
        case GDT_UInt64:
            hBufferType = H5Tcopy(H5T_NATIVE_UINT64);
            break;
        case GDT_Int64:
            hBufferType = H5Tcopy(H5T_NATIVE_INT64);
            break;
        case GDT_Float32:
            hBufferType = H5Tcopy(H5T_NATIVE_FLOAT);
            break;
        case GDT_Float64:
            hBufferType = H5Tcopy(H5T_NATIVE_DOUBLE);
            break;
        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat32:
        case GDT_CFloat64:
            if (bufferDataType != dt)
            {
                return H5I_INVALID_HID;
            }
            else
            {
                hBufferType = H5Tcopy(hNativeDT);
                break;
            }
        case GDT_Unknown:
        case GDT_TypeCount:
            return H5I_INVALID_HID;
    }
    return hBufferType;
}

/************************************************************************/
/*                        FreeDynamicMemory()                           */
/************************************************************************/

static void FreeDynamicMemory(GByte *pabyPtr, hid_t hDataType)
{
    const auto klass = H5Tget_class(hDataType);
    if (klass == H5T_STRING && H5Tis_variable_str(hDataType))
    {
        auto hDataSpace = H5Screate(H5S_SCALAR);
        H5Dvlen_reclaim(hDataType, hDataSpace, H5P_DEFAULT, pabyPtr);
        H5Sclose(hDataSpace);
    }
    else if (klass == H5T_COMPOUND)
    {
        const unsigned nMembers = H5Tget_nmembers(hDataType);
        for (unsigned i = 0; i < nMembers; i++)
        {
            const auto nOffset = H5Tget_member_offset(hDataType, i);
            auto hMemberType = H5Tget_member_type(hDataType, i);
            if (hMemberType < 0)
                continue;
            FreeDynamicMemory(pabyPtr + nOffset, hMemberType);
            H5Tclose(hMemberType);
        }
    }
}

/************************************************************************/
/*                   CreateMapTargetComponentsToSrc()                   */
/************************************************************************/

static std::vector<unsigned>
CreateMapTargetComponentsToSrc(hid_t hSrcDataType,
                               const GDALExtendedDataType &dstDataType)
{
    CPLAssert(H5Tget_class(hSrcDataType) == H5T_COMPOUND);
    CPLAssert(dstDataType.GetClass() == GEDTC_COMPOUND);

    const unsigned nMembers = H5Tget_nmembers(hSrcDataType);
    std::map<std::string, unsigned> oMapSrcCompNameToIdx;
    for (unsigned i = 0; i < nMembers; i++)
    {
        char *pszName = H5Tget_member_name(hSrcDataType, i);
        if (pszName)
        {
            oMapSrcCompNameToIdx[pszName] = i;
            H5free_memory(pszName);
        }
    }

    std::vector<unsigned> ret;
    const auto &comps = dstDataType.GetComponents();
    ret.reserve(comps.size());
    for (const auto &comp : comps)
    {
        auto oIter = oMapSrcCompNameToIdx.find(comp->GetName());
        CPLAssert(oIter != oMapSrcCompNameToIdx.end());
        ret.emplace_back(oIter->second);
    }
    return ret;
}

/************************************************************************/
/*                            CopyValue()                               */
/************************************************************************/

static void CopyValue(const GByte *pabySrcBuffer, hid_t hSrcDataType,
                      GByte *pabyDstBuffer,
                      const GDALExtendedDataType &dstDataType,
                      const std::vector<unsigned> &mapDstCompsToSrcComps)
{
    const auto klass = H5Tget_class(hSrcDataType);
    if (klass == H5T_STRING)
    {
        if (H5Tis_variable_str(hSrcDataType))
        {
            GDALExtendedDataType::CopyValue(
                pabySrcBuffer, GDALExtendedDataType::CreateString(),
                pabyDstBuffer, dstDataType);
        }
        else
        {
            size_t nStringSize = H5Tget_size(hSrcDataType);
            char *pszStr = static_cast<char *>(VSIMalloc(nStringSize + 1));
            if (pszStr)
            {
                memcpy(pszStr, pabySrcBuffer, nStringSize);
                pszStr[nStringSize] = 0;
            }
            GDALExtendedDataType::CopyValue(
                &pszStr, GDALExtendedDataType::CreateString(), pabyDstBuffer,
                dstDataType);
            CPLFree(pszStr);
        }
    }
    else if (klass == H5T_COMPOUND)
    {
        if (dstDataType.GetClass() != GEDTC_COMPOUND)
        {
            const auto eSrcDataType = ::HDF5Dataset::GetDataType(hSrcDataType);
            // Typically source is complex data type
#ifdef HDF5_HAVE_FLOAT16
            if (eSrcDataType == GDT_CFloat32 &&
                ::HDF5Dataset::IsNativeCFloat16(hSrcDataType))
            {
                if (dstDataType.GetNumericDataType() == GDT_CFloat32)
                {
                    for (int j = 0; j <= 1; ++j)
                    {
                        uint16_t nVal16;
                        memcpy(&nVal16, pabySrcBuffer + j * sizeof(nVal16),
                               sizeof(nVal16));
                        const uint32_t nVal32 = CPLHalfToFloat(nVal16);
                        memcpy(pabyDstBuffer + j * sizeof(float), &nVal32,
                               sizeof(nVal32));
                    }
                }
                else if (dstDataType.GetNumericDataType() == GDT_CFloat64)
                {
                    for (int j = 0; j <= 1; ++j)
                    {
                        uint16_t nVal16;
                        memcpy(&nVal16, pabySrcBuffer + j * sizeof(nVal16),
                               sizeof(nVal16));
                        const uint32_t nVal32 = CPLHalfToFloat(nVal16);
                        float fVal;
                        memcpy(&fVal, &nVal32, sizeof(fVal));
                        double dfVal = fVal;
                        memcpy(pabyDstBuffer + j * sizeof(double), &dfVal,
                               sizeof(dfVal));
                    }
                }
                return;
            }

#endif
            auto srcDataType(GDALExtendedDataType::Create(eSrcDataType));
            if (srcDataType.GetClass() == GEDTC_NUMERIC &&
                srcDataType.GetNumericDataType() != GDT_Unknown)
            {
                GDALExtendedDataType::CopyValue(pabySrcBuffer, srcDataType,
                                                pabyDstBuffer, dstDataType);
            }
        }
        else
        {
            const auto &comps = dstDataType.GetComponents();
            CPLAssert(comps.size() == mapDstCompsToSrcComps.size());
            for (size_t iDst = 0; iDst < comps.size(); ++iDst)
            {
                const unsigned iSrc = mapDstCompsToSrcComps[iDst];
                auto hMemberType = H5Tget_member_type(hSrcDataType, iSrc);
                const std::vector<unsigned> mapDstSubCompsToSrcSubComps(
                    (H5Tget_class(hMemberType) == H5T_COMPOUND &&
                     comps[iDst]->GetType().GetClass() == GEDTC_COMPOUND)
                        ? CreateMapTargetComponentsToSrc(hMemberType,
                                                         comps[iDst]->GetType())
                        : std::vector<unsigned>());
                CopyValue(pabySrcBuffer +
                              H5Tget_member_offset(hSrcDataType, iSrc),
                          hMemberType, pabyDstBuffer + comps[iDst]->GetOffset(),
                          comps[iDst]->GetType(), mapDstSubCompsToSrcSubComps);
                H5Tclose(hMemberType);
            }
        }
    }
    else if (klass == H5T_ENUM)
    {
        auto hParent = H5Tget_super(hSrcDataType);
        CopyValue(pabySrcBuffer, hParent, pabyDstBuffer, dstDataType, {});
        H5Tclose(hParent);
    }
#ifdef HDF5_HAVE_FLOAT16
    else if (H5Tequal(hSrcDataType, H5T_NATIVE_FLOAT16))
    {
        uint16_t nVal16;
        memcpy(&nVal16, pabySrcBuffer, sizeof(nVal16));
        const uint32_t nVal32 = CPLHalfToFloat(nVal16);
        float fVal;
        memcpy(&fVal, &nVal32, sizeof(fVal));
        GDALExtendedDataType::CopyValue(
            &fVal, GDALExtendedDataType::Create(GDT_Float32), pabyDstBuffer,
            dstDataType);
    }
#endif
    else
    {
        GDALDataType eDT = ::HDF5Dataset::GetDataType(hSrcDataType);
        GDALExtendedDataType::CopyValue(pabySrcBuffer,
                                        GDALExtendedDataType::Create(eDT),
                                        pabyDstBuffer, dstDataType);
    }
}

/************************************************************************/
/*                        CopyToFinalBuffer()                           */
/************************************************************************/

static void CopyToFinalBuffer(void *pDstBuffer, const void *pTemp, size_t nDims,
                              const size_t *count,
                              const GPtrDiff_t *bufferStride,
                              hid_t hSrcDataType,
                              const GDALExtendedDataType &bufferDataType)
{
    const size_t nSrcDataTypeSize(H5Tget_size(hSrcDataType));
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    const GByte *pabySrcBuffer = static_cast<const GByte *>(pTemp);
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;
    const std::vector<unsigned> mapDstCompsToSrcComps(
        (H5Tget_class(hSrcDataType) == H5T_COMPOUND &&
         bufferDataType.GetClass() == GEDTC_COMPOUND)
            ? CreateMapTargetComponentsToSrc(hSrcDataType, bufferDataType)
            : std::vector<unsigned>());

    bool bFastCopyOfCompoundToSingleComponentCompound = false;
    GDALDataType eSrcTypeComp = GDT_Unknown;
    size_t nSrcOffset = 0;
    GDALDataType eDstTypeComp = GDT_Unknown;
    int bufferStrideLastDim = 0;
    if (nDims > 0 && mapDstCompsToSrcComps.size() == 1 &&
        bufferDataType.GetComponents()[0]->GetType().GetClass() ==
            GEDTC_NUMERIC)
    {
        auto hMemberType =
            H5Tget_member_type(hSrcDataType, mapDstCompsToSrcComps[0]);
        eSrcTypeComp = HDF5Dataset::GetDataType(hMemberType);
        if (eSrcTypeComp != GDT_Unknown)
        {
            nSrcOffset =
                H5Tget_member_offset(hSrcDataType, mapDstCompsToSrcComps[0]);
            eDstTypeComp = bufferDataType.GetComponents()[0]
                               ->GetType()
                               .GetNumericDataType();
            bufferStrideLastDim = static_cast<int>(bufferStride[nDims - 1] *
                                                   bufferDataType.GetSize());
            bFastCopyOfCompoundToSingleComponentCompound = true;
        }
    }

lbl_next_depth:
    if (bFastCopyOfCompoundToSingleComponentCompound && iDim == nDims - 1)
    {
        GDALCopyWords64(pabySrcBuffer + nSrcOffset, eSrcTypeComp,
                        static_cast<int>(nSrcDataTypeSize),
                        pabyDstBufferStack[iDim], eDstTypeComp,
                        static_cast<int>(bufferStrideLastDim), count[iDim]);
        pabySrcBuffer += count[iDim] * nSrcDataTypeSize;
    }
    else if (iDim == nDims)
    {
        CopyValue(pabySrcBuffer, hSrcDataType, pabyDstBufferStack[nDims],
                  bufferDataType, mapDstCompsToSrcComps);
        pabySrcBuffer += nSrcDataTypeSize;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] +=
                bufferStride[iDim] * bufferDataType.GetSize();
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF5Array::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                      const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                      const GDALExtendedDataType &bufferDataType,
                      void *pDstBuffer) const
{
    HDF5_GLOBAL_LOCK();

    const size_t nDims(m_dims.size());
    std::vector<H5OFFSET_TYPE> anOffset(nDims);
    std::vector<hsize_t> anCount(nDims);
    std::vector<hsize_t> anStep(nDims);

    size_t nEltCount = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        if (count[i] != 1 && (arrayStep[i] < 0 || bufferStride[i] < 0))
        {
            return ReadSlow(arrayStartIdx, count, arrayStep, bufferStride,
                            bufferDataType, pDstBuffer);
        }
        anOffset[i] = static_cast<hsize_t>(arrayStartIdx[i]);
        anCount[i] = static_cast<hsize_t>(count[i]);
        anStep[i] = static_cast<hsize_t>(count[i] == 1 ? 1 : arrayStep[i]);
        nEltCount *= count[i];
    }

    if (IsTransposedRequest(count, bufferStride))
    {
        return ReadForTransposedRequest(arrayStartIdx, count, arrayStep,
                                        bufferStride, bufferDataType,
                                        pDstBuffer);
    }

    hid_t hBufferType = H5I_INVALID_HID;
    GByte *pabyTemp = nullptr;
    if (m_dt.GetClass() == GEDTC_STRING)
    {
        if (bufferDataType.GetClass() != GEDTC_STRING)
        {
            return false;
        }
        hBufferType = H5Tcopy(m_hNativeDT);
        if (!H5Tis_variable_str(m_hNativeDT))
        {
            const size_t nStringSize = H5Tget_size(m_hNativeDT);
            pabyTemp = static_cast<GByte *>(
                VSI_MALLOC2_VERBOSE(nStringSize, nEltCount));
            if (pabyTemp == nullptr)
                return false;
        }
    }
    else if (bufferDataType.GetClass() == GEDTC_NUMERIC &&
             m_dt.GetClass() == GEDTC_NUMERIC &&
             !GDALDataTypeIsComplex(m_dt.GetNumericDataType()) &&
             !GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()))
    {
        // Compatibility with older libhdf5 that doesn't like requesting
        // an enum to an integer
        if (H5Tget_class(m_hNativeDT) == H5T_ENUM)
        {
            auto hParent = H5Tget_super(m_hNativeDT);
            if (H5Tequal(hParent, H5T_NATIVE_UCHAR) ||
                H5Tequal(hParent, H5T_NATIVE_SCHAR) ||
                H5Tequal(hParent, H5T_NATIVE_USHORT) ||
                H5Tequal(hParent, H5T_NATIVE_SHORT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT) ||
                H5Tequal(hParent, H5T_NATIVE_INT) ||
                H5Tequal(hParent, H5T_NATIVE_UINT64) ||
                H5Tequal(hParent, H5T_NATIVE_INT64))
            {
                hBufferType = H5Tcopy(m_hNativeDT);
                if (m_dt != bufferDataType)
                {
                    const size_t nDataTypeSize = H5Tget_size(m_hNativeDT);
                    pabyTemp = static_cast<GByte *>(
                        VSI_MALLOC2_VERBOSE(nDataTypeSize, nEltCount));
                    if (pabyTemp == nullptr)
                    {
                        H5Tclose(hBufferType);
                        return false;
                    }
                }
            }
            H5Tclose(hParent);
        }
        if (hBufferType == H5I_INVALID_HID)
        {
            hBufferType = GetHDF5DataTypeFromGDALDataType(m_dt, m_hNativeDT,
                                                          bufferDataType);
            if (hBufferType == H5I_INVALID_HID)
            {
                VSIFree(pabyTemp);
                return false;
            }
        }
    }
    else
    {
        hBufferType = H5Tcopy(m_hNativeDT);
        if (m_dt != bufferDataType || m_bHasString || m_bHasNonNativeDataType)
        {
            const size_t nDataTypeSize = H5Tget_size(m_hNativeDT);
            pabyTemp = static_cast<GByte *>(
                VSI_MALLOC2_VERBOSE(nDataTypeSize, nEltCount));
            if (pabyTemp == nullptr)
            {
                H5Tclose(hBufferType);
                return false;
            }
        }
    }

    // Select block from file space.
    herr_t status;
    if (nDims)
    {
        status =
            H5Sselect_hyperslab(m_hDataSpace, H5S_SELECT_SET, anOffset.data(),
                                anStep.data(), anCount.data(), nullptr);
        if (status < 0)
        {
            H5Tclose(hBufferType);
            VSIFree(pabyTemp);
            return false;
        }
    }

    // Create memory data space
    const hid_t hMemSpace = nDims == 0
                                ? H5Screate(H5S_SCALAR)
                                : H5Screate_simple(static_cast<int>(nDims),
                                                   anCount.data(), nullptr);
    if (nDims)
    {
        std::vector<H5OFFSET_TYPE> anMemOffset(nDims);
        status =
            H5Sselect_hyperslab(hMemSpace, H5S_SELECT_SET, anMemOffset.data(),
                                nullptr, anCount.data(), nullptr);
        if (status < 0)
        {
            H5Tclose(hBufferType);
            H5Sclose(hMemSpace);
            VSIFree(pabyTemp);
            return false;
        }
    }

    status = H5Dread(m_hArray, hBufferType, hMemSpace, m_hDataSpace,
                     H5P_DEFAULT, pabyTemp ? pabyTemp : pDstBuffer);

    if (status >= 0)
    {
        if (H5Tis_variable_str(hBufferType))
        {
            IngestVariableStrings(pDstBuffer, hBufferType, nDims, count,
                                  bufferStride);
        }
        else if (pabyTemp && bufferDataType.GetClass() == GEDTC_STRING)
        {
            IngestFixedLengthStrings(pDstBuffer, pabyTemp, hBufferType, nDims,
                                     count, bufferStride);
        }
        else if (pabyTemp)
        {
            CopyToFinalBuffer(pDstBuffer, pabyTemp, nDims, count, bufferStride,
                              m_hNativeDT, bufferDataType);

            if (m_bHasString)
            {
                const size_t nBufferTypeSize = H5Tget_size(hBufferType);
                GByte *pabyPtr = pabyTemp;
                for (size_t i = 0; i < nEltCount; ++i)
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
    HDF5_GLOBAL_LOCK();

    if (m_hAttribute > 0)
        H5Aclose(m_hAttribute);
    if (m_hNativeDT > 0)
        H5Tclose(m_hNativeDT);
    if (m_hDataSpace > 0)
        H5Sclose(m_hDataSpace);
}

/************************************************************************/
/*                       CopyAllAttrValuesInto()                        */
/************************************************************************/

static void CopyAllAttrValuesInto(size_t nDims, const GUInt64 *arrayStartIdx,
                                  const size_t *count, const GInt64 *arrayStep,
                                  const GPtrDiff_t *bufferStride,
                                  const GDALExtendedDataType &bufferDataType,
                                  void *pDstBuffer, hid_t hSrcBufferType,
                                  const void *pabySrcBuffer)
{
    const size_t nBufferDataTypeSize(bufferDataType.GetSize());
    const size_t nSrcDataTypeSize(H5Tget_size(hSrcBufferType));
    std::vector<size_t> anStackCount(nDims);
    std::vector<const GByte *> pabySrcBufferStack(nDims + 1);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    const std::vector<unsigned> mapDstCompsToSrcComps(
        (H5Tget_class(hSrcBufferType) == H5T_COMPOUND &&
         bufferDataType.GetClass() == GEDTC_COMPOUND)
            ? CreateMapTargetComponentsToSrc(hSrcBufferType, bufferDataType)
            : std::vector<unsigned>());

    pabySrcBufferStack[0] = static_cast<const GByte *>(pabySrcBuffer);
    if (nDims > 0)
        pabySrcBufferStack[0] += arrayStartIdx[0] * nSrcDataTypeSize;
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;
lbl_next_depth:
    if (iDim == nDims)
    {
        CopyValue(pabySrcBufferStack[nDims], hSrcBufferType,
                  pabyDstBufferStack[nDims], bufferDataType,
                  mapDstCompsToSrcComps);
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            pabySrcBufferStack[iDim] = pabySrcBufferStack[iDim - 1];
            if (iDim < nDims)
            {
                pabySrcBufferStack[iDim] +=
                    arrayStartIdx[iDim] * nSrcDataTypeSize;
            }
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] +=
                bufferStride[iDim] * nBufferDataTypeSize;
            pabySrcBufferStack[iDim] += arrayStep[iDim] * nSrcDataTypeSize;
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF5Attribute::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                          const GInt64 *arrayStep,
                          const GPtrDiff_t *bufferStride,
                          const GDALExtendedDataType &bufferDataType,
                          void *pDstBuffer) const
{
    HDF5_GLOBAL_LOCK();

    const size_t nDims(m_dims.size());
    if (m_dt.GetClass() == GEDTC_STRING)
    {
        if (bufferDataType.GetClass() != GEDTC_STRING)
        {
            return false;
        }

        if (!H5Tis_variable_str(m_hNativeDT))
        {
            const size_t nStringSize = H5Tget_size(m_hNativeDT);
            GByte *pabyTemp = static_cast<GByte *>(
                VSI_CALLOC_VERBOSE(nStringSize, m_nElements));
            if (pabyTemp == nullptr)
                return false;
            if (H5Sget_simple_extent_type(m_hDataSpace) != H5S_NULL &&
                H5Aread(m_hAttribute, m_hNativeDT, pabyTemp) < 0)
            {
                VSIFree(pabyTemp);
                return false;
            }
            CopyAllAttrValuesInto(nDims, arrayStartIdx, count, arrayStep,
                                  bufferStride, bufferDataType, pDstBuffer,
                                  m_hNativeDT, pabyTemp);
            VSIFree(pabyTemp);
        }
        else
        {
            void *pabyTemp = VSI_CALLOC_VERBOSE(sizeof(char *), m_nElements);
            if (pabyTemp == nullptr)
                return false;
            if (H5Sget_simple_extent_type(m_hDataSpace) != H5S_NULL &&
                H5Aread(m_hAttribute, m_hNativeDT, pabyTemp) < 0)
            {
                VSIFree(pabyTemp);
                return false;
            }
            CopyAllAttrValuesInto(nDims, arrayStartIdx, count, arrayStep,
                                  bufferStride, bufferDataType, pDstBuffer,
                                  m_hNativeDT, pabyTemp);
            H5Dvlen_reclaim(m_hNativeDT, m_hDataSpace, H5P_DEFAULT, pabyTemp);
            VSIFree(pabyTemp);
        }
        return true;
    }

    hid_t hBufferType = H5I_INVALID_HID;
    if (m_dt.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC &&
        !GDALDataTypeIsComplex(m_dt.GetNumericDataType()) &&
        !GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()))
    {
        // Compatibility with older libhdf5 that doesn't like requesting
        // an enum to an integer
        if (H5Tget_class(m_hNativeDT) == H5T_ENUM)
        {
            auto hParent = H5Tget_super(m_hNativeDT);
            if (H5Tequal(hParent, H5T_NATIVE_UCHAR) ||
                H5Tequal(hParent, H5T_NATIVE_SCHAR) ||
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
        if (hBufferType == H5I_INVALID_HID)
        {
            hBufferType = GetHDF5DataTypeFromGDALDataType(m_dt, m_hNativeDT,
                                                          bufferDataType);
        }
    }
    else
    {
        hBufferType = H5Tcopy(m_hNativeDT);
    }

    if (hBufferType == H5I_INVALID_HID)
        return false;

    const size_t nBufferTypeSize(H5Tget_size(hBufferType));
    GByte *pabyTemp =
        static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nBufferTypeSize, m_nElements));
    if (pabyTemp == nullptr)
    {
        H5Tclose(hBufferType);
        return false;
    }
    if (H5Aread(m_hAttribute, hBufferType, pabyTemp) < 0)
    {
        VSIFree(pabyTemp);
        return false;
    }
    CopyAllAttrValuesInto(nDims, arrayStartIdx, count, arrayStep, bufferStride,
                          bufferDataType, pDstBuffer, hBufferType, pabyTemp);
    if (bufferDataType.GetClass() == GEDTC_COMPOUND && m_bHasString)
    {
        GByte *pabyPtr = pabyTemp;
        for (size_t i = 0; i < m_nElements; ++i)
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
    HDF5_GLOBAL_LOCK();

    auto hGroup = H5Gopen(m_poShared->GetHDF5(), m_osGroupFullname.c_str());
    if (hGroup >= 0)
    {
        auto hArray = H5Dopen(hGroup, GetName().c_str());
        H5Gclose(hGroup);
        if (hArray >= 0)
        {
            auto ar(HDF5Array::Create(m_osGroupFullname, GetName(), m_poShared,
                                      hArray, nullptr, false));
            auto attrName = ar->GetAttribute("NAME");
            if (attrName && attrName->GetDataType().GetClass() == GEDTC_STRING)
            {
                const char *pszName = attrName->ReadAsString();
                if (pszName &&
                    STARTS_WITH(
                        pszName,
                        "This is a netCDF dimension but not a netCDF variable"))
                {
                    return nullptr;
                }
            }
            return ar;
        }
    }
    return nullptr;
}

}  // namespace GDAL

/************************************************************************/
/*                           OpenMultiDim()                             */
/************************************************************************/

GDALDataset *HDF5Dataset::OpenMultiDim(GDALOpenInfo *poOpenInfo)
{
    HDF5_GLOBAL_LOCK();

    const char *pszFilename = STARTS_WITH(poOpenInfo->pszFilename, "HDF5:")
                                  ? poOpenInfo->pszFilename + strlen("HDF5:")
                                  : poOpenInfo->pszFilename;

    // Try opening the dataset.
    auto hHDF5 = GDAL_HDF5Open(pszFilename);
    if (hHDF5 < 0)
    {
        return nullptr;
    }

    auto poSharedResources = GDAL::HDF5SharedResources::Create(pszFilename);
    poSharedResources->m_hHDF5 = hHDF5;

    auto poGroup(OpenGroup(poSharedResources));
    if (poGroup == nullptr)
    {
        return nullptr;
    }

    auto poDS(new HDF5Dataset());
    poDS->m_poRootGroup = std::move(poGroup);

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Setup/check for pam .aux.xml.
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                            OpenGroup()                               */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF5Dataset::OpenGroup(
    const std::shared_ptr<GDAL::HDF5SharedResources> &poSharedResources)
{
    HDF5_GLOBAL_LOCK();

    auto poGroup = poSharedResources->GetRootGroup();
    if (!poGroup)
        return nullptr;

    if (HDF5EOSParser::HasHDFEOS(poGroup->GetID()))
    {
        poSharedResources->m_poHDF5EOSParser =
            std::make_unique<HDF5EOSParser>();
        if (poSharedResources->m_poHDF5EOSParser->Parse(poGroup->GetID()))
        {
            CPLDebug("HDF5", "Successfully parsed HDFEOS metadata");
        }
        else
        {
            poSharedResources->m_poHDF5EOSParser.reset();
        }
    }

    return poGroup;
}
