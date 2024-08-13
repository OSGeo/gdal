/******************************************************************************
 *
 * Project:  netCDF read/write Driver
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

#include <algorithm>
#include <limits>
#include <map>

#include "netcdfdataset.h"
#include "netcdfdrivercore.h"

#include "netcdf_mem.h"

/************************************************************************/
/*                         netCDFSharedResources                        */
/************************************************************************/

class netCDFSharedResources
{
    friend class netCDFDataset;

    bool m_bImappIsInElements = true;
    bool m_bReadOnly = true;
    bool m_bIsNC4 = false;
    int m_cdfid = 0;
#ifdef ENABLE_NCDUMP
    bool m_bFileToDestroyAtClosing = false;
#endif
    CPLString m_osFilename{};
#ifdef ENABLE_UFFD
    cpl_uffd_context *m_pUffdCtx = nullptr;
#endif
    VSILFILE *m_fpVSIMEM = nullptr;
    bool m_bDefineMode = false;
    std::map<int, int> m_oMapDimIdToGroupId{};
    bool m_bIsInIndexingVariable = false;
    std::shared_ptr<GDALPamMultiDim> m_poPAM{};
    std::map<int, std::weak_ptr<GDALDimension>> m_oCachedDimensions{};

  public:
    explicit netCDFSharedResources(const std::string &osFilename);
    ~netCDFSharedResources();

    inline int GetCDFId() const
    {
        return m_cdfid;
    }

    inline bool IsReadOnly() const
    {
        return m_bReadOnly;
    }

    inline bool IsNC4() const
    {
        return m_bIsNC4;
    }

    bool SetDefineMode(bool bNewDefineMode);
    int GetBelongingGroupOfDim(int startgid, int dimid);

    inline bool GetImappIsInElements() const
    {
        return m_bImappIsInElements;
    }

    void SetIsInGetIndexingVariable(bool b)
    {
        m_bIsInIndexingVariable = b;
    }

    bool GetIsInIndexingVariable() const
    {
        return m_bIsInIndexingVariable;
    }

    const std::string &GetFilename() const
    {
        return m_osFilename;
    }

    const std::shared_ptr<GDALPamMultiDim> &GetPAM()
    {
        return m_poPAM;
    }

    void CacheDimension(int dimid, const std::shared_ptr<GDALDimension> &poDim)
    {
        m_oCachedDimensions[dimid] = poDim;
    }

    std::shared_ptr<GDALDimension> GetCachedDimension(int dimid) const
    {
        auto oIter = m_oCachedDimensions.find(dimid);
        if (oIter == m_oCachedDimensions.end())
            return nullptr;
        return oIter->second.lock();
    }
};

/************************************************************************/
/*                       netCDFSharedResources()                        */
/************************************************************************/

netCDFSharedResources::netCDFSharedResources(const std::string &osFilename)
    : m_bImappIsInElements(false), m_osFilename(osFilename),
      m_poPAM(std::make_shared<GDALPamMultiDim>(osFilename))
{
    // netcdf >= 4.4 uses imapp argument of nc_get/put_varm as a stride in
    // elements, whereas earlier versions use bytes.
    CPLStringList aosVersionNumbers(
        CSLTokenizeString2(nc_inq_libvers(), ".", 0));
    m_bImappIsInElements = false;
    if (aosVersionNumbers.size() >= 3)
    {
        m_bImappIsInElements =
            (atoi(aosVersionNumbers[0]) > 4 || atoi(aosVersionNumbers[1]) >= 4);
    }
}

/************************************************************************/
/*                       GetBelongingGroupOfDim()                       */
/************************************************************************/

int netCDFSharedResources::GetBelongingGroupOfDim(int startgid, int dimid)
{
    // Am I missing a netCDF API to do this directly ?
    auto oIter = m_oMapDimIdToGroupId.find(dimid);
    if (oIter != m_oMapDimIdToGroupId.end())
        return oIter->second;

    int gid = startgid;
    while (true)
    {
        int nbDims = 0;
        NCDF_ERR(nc_inq_ndims(gid, &nbDims));
        if (nbDims > 0)
        {
            std::vector<int> dimids(nbDims);
            NCDF_ERR(nc_inq_dimids(gid, &nbDims, &dimids[0], FALSE));
            for (int i = 0; i < nbDims; i++)
            {
                m_oMapDimIdToGroupId[dimid] = gid;
                if (dimids[i] == dimid)
                    return gid;
            }
        }
        int nParentGID = 0;
        if (nc_inq_grp_parent(gid, &nParentGID) != NC_NOERR)
            return startgid;
        gid = nParentGID;
    }
}

/************************************************************************/
/*                            SetDefineMode()                           */
/************************************************************************/

bool netCDFSharedResources::SetDefineMode(bool bNewDefineMode)
{
    // Do nothing if already in new define mode
    // or if dataset is in read-only mode or if dataset is NC4 format.
    if (m_bDefineMode == bNewDefineMode || m_bReadOnly || m_bIsNC4)
        return true;

    CPLDebug("GDAL_netCDF", "SetDefineMode(%d) new=%d, old=%d", m_cdfid,
             static_cast<int>(bNewDefineMode), static_cast<int>(m_bDefineMode));

    m_bDefineMode = bNewDefineMode;

    int status;
    if (m_bDefineMode)
        status = nc_redef(m_cdfid);
    else
        status = nc_enddef(m_cdfid);

    NCDF_ERR(status);
    return status == NC_NOERR;
}

/************************************************************************/
/*                        netCDFAttributeHolder                         */
/************************************************************************/

class netCDFAttributeHolder CPL_NON_FINAL
{
  protected:
    std::map<std::string, GDALAttribute *> m_oMapAttributes{};

  public:
    void RegisterAttribute(GDALAttribute *poAttr)
    {
        m_oMapAttributes[poAttr->GetName()] = poAttr;
    }

    void UnRegisterAttribute(GDALAttribute *poAttr)
    {
        m_oMapAttributes.erase(poAttr->GetName());
    }
};

/************************************************************************/
/*                           netCDFGroup                                */
/************************************************************************/

class netCDFGroup final : public GDALGroup, public netCDFAttributeHolder
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    int m_gid = 0;
    CPLStringList m_aosStructuralInfo{};
    std::weak_ptr<netCDFGroup> m_poParent{};
    std::set<GDALGroup *> m_oSetGroups{};
    std::set<GDALDimension *> m_oSetDimensions{};
    std::set<GDALMDArray *> m_oSetArrays{};

    static std::string retrieveName(int gid)
    {
        CPLMutexHolderD(&hNCMutex);
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_grpname(gid, szName));
        return szName;
    }

    void RegisterSubGroup(GDALGroup *poSubGroup)
    {
        m_oSetGroups.insert(poSubGroup);
    }

    void UnRegisterSubGroup(GDALGroup *poSubGroup)
    {
        m_oSetGroups.erase(poSubGroup);
    }

  protected:
    friend class netCDFDimension;

    void RegisterDimension(GDALDimension *poDim)
    {
        m_oSetDimensions.insert(poDim);
    }

    void UnRegisterDimension(GDALDimension *poDim)
    {
        m_oSetDimensions.erase(poDim);
    }

    friend class netCDFVariable;

    void RegisterArray(GDALMDArray *poArray)
    {
        m_oSetArrays.insert(poArray);
    }

    void UnRegisterArray(GDALMDArray *poArray)
    {
        m_oSetArrays.erase(poArray);
    }

    void NotifyChildrenOfRenaming() override;

    netCDFGroup(const std::shared_ptr<netCDFSharedResources> &poShared,
                int gid);

  public:
    ~netCDFGroup();

    static std::shared_ptr<netCDFGroup>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared, int cdfid);

    static std::shared_ptr<netCDFGroup>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared,
           const std::shared_ptr<netCDFGroup> &poParent, int nSubGroupId);

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALDimension>>
    GetDimensions(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALGroup> CreateGroup(const std::string &osName,
                                           CSLConstList papszOptions) override;

    std::shared_ptr<GDALDimension>
    CreateDimension(const std::string &osName, const std::string &osType,
                    const std::string &osDirection, GUInt64 nSize,
                    CSLConstList papszOptions) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(
        const std::string &osName,
        const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
        const GDALExtendedDataType &oDataType,
        CSLConstList papszOptions) override;

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions) override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions) override;

    CSLConstList GetStructuralInfo() const override;

    void ClearStatistics() override;

    bool Rename(const std::string &osNewName) override;
};

/************************************************************************/
/*                   netCDFVirtualGroupBySameDimension                  */
/************************************************************************/

class netCDFVirtualGroupBySameDimension final : public GDALGroup
{
    // the real group to which we derived this virtual group from
    std::shared_ptr<netCDFGroup> m_poGroup;
    std::string m_osDimName{};

  protected:
    netCDFVirtualGroupBySameDimension(
        const std::shared_ptr<netCDFGroup> &poGroup,
        const std::string &osDimName);

  public:
    static std::shared_ptr<netCDFVirtualGroupBySameDimension>
    Create(const std::shared_ptr<netCDFGroup> &poGroup,
           const std::string &osDimName);

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                         netCDFDimension                              */
/************************************************************************/

class netCDFDimension final : public GDALDimension
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    int m_gid = 0;
    int m_dimid = 0;
    std::weak_ptr<netCDFGroup> m_poParent{};

    static std::string retrieveName(int cfid, int dimid)
    {
        CPLMutexHolderD(&hNCMutex);
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_dimname(cfid, dimid, szName));
        return szName;
    }

    static GUInt64 retrieveSize(int cfid, int dimid)
    {
        CPLMutexHolderD(&hNCMutex);
        size_t nDimLen = 0;
        NCDF_ERR(nc_inq_dimlen(cfid, dimid, &nDimLen));
        return nDimLen;
    }

  public:
    netCDFDimension(const std::shared_ptr<netCDFSharedResources> &poShared,
                    int cfid, int dimid, size_t nForcedSize,
                    const std::string &osType);

    ~netCDFDimension();

    static std::shared_ptr<netCDFDimension>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared,
           const std::shared_ptr<netCDFGroup> &poParent, int cfid, int dimid,
           size_t nForcedSize, const std::string &osType);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;

    int GetId() const
    {
        return m_dimid;
    }

    GUInt64 GetActualSize() const
    {
        return retrieveSize(m_gid, m_dimid);
    }

    void SetSize(GUInt64 nNewSize)
    {
        m_nSize = nNewSize;
    }

    bool Rename(const std::string &osNewName) override;
};

/************************************************************************/
/*                         netCDFAttribute                              */
/************************************************************************/

class netCDFVariable;

class netCDFAttribute final : public GDALAttribute
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    std::weak_ptr<netCDFAttributeHolder> m_poParent;
    int m_gid = 0;
    int m_varid = 0;
    size_t m_nTextLength = 0;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    nc_type m_nAttType = NC_NAT;
    mutable std::unique_ptr<GDALExtendedDataType> m_dt;
    mutable bool m_bPerfectDataTypeMatch = false;

  protected:
    netCDFAttribute(const std::shared_ptr<netCDFSharedResources> &poShared,
                    int gid, int varid, const std::string &name);

    netCDFAttribute(const std::shared_ptr<netCDFSharedResources> &poShared,
                    int gid, int varid, const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions);

    bool
    IRead(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
          const size_t *count,             // array of size GetDimensionCount()
          const GInt64 *arrayStep,         // step in elements
          const GPtrDiff_t *bufferStride,  // stride in elements
          const GDALExtendedDataType &bufferDataType,
          void *pDstBuffer) const override;

    bool
    IWrite(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
           const size_t *count,             // array of size GetDimensionCount()
           const GInt64 *arrayStep,         // step in elements
           const GPtrDiff_t *bufferStride,  // stride in elements
           const GDALExtendedDataType &bufferDataType,
           const void *pSrcBuffer) override;

  public:
    ~netCDFAttribute() override;

    static std::shared_ptr<netCDFAttribute>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared,
           const std::shared_ptr<netCDFAttributeHolder> &poParent, int gid,
           int varid, const std::string &name);

    static std::shared_ptr<netCDFAttribute>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared,
           const std::shared_ptr<netCDFAttributeHolder> &poParent, int gid,
           int varid, const std::string &osName,
           const std::vector<GUInt64> &anDimensions,
           const GDALExtendedDataType &oDataType, CSLConstList papszOptions);

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_dims;
    }

    const GDALExtendedDataType &GetDataType() const override;

    bool Rename(const std::string &osNewName) override;
};

/************************************************************************/
/*                         netCDFVariable                               */
/************************************************************************/

class netCDFVariable final : public GDALPamMDArray, public netCDFAttributeHolder
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    std::weak_ptr<netCDFGroup> m_poParent{};
    int m_gid = 0;
    int m_varid = 0;
    int m_nDims = 0;
    mutable std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    mutable nc_type m_nVarType = NC_NAT;
    mutable std::unique_ptr<GDALExtendedDataType> m_dt;
    mutable bool m_bPerfectDataTypeMatch = false;
    mutable std::vector<GByte> m_abyNoData{};
    mutable bool m_bGetRawNoDataValueHasRun = false;
    bool m_bHasWrittenData = true;
    bool m_bUseDefaultFillAsNoData = false;
    std::string m_osUnit{};
    CPLStringList m_aosStructuralInfo{};
    mutable bool m_bSRSRead = false;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};
    bool m_bWriteGDALTags = true;
    size_t m_nTextLength = 0;
    mutable std::vector<GUInt64> m_cachedArrayStartIdx{};
    mutable std::vector<size_t> m_cachedCount{};
    mutable std::shared_ptr<GDALMDArray> m_poCachedArray{};

    void ConvertNCToGDAL(GByte *) const;
    void ConvertGDALToNC(GByte *) const;

    bool ReadOneElement(const GDALExtendedDataType &src_datatype,
                        const GDALExtendedDataType &bufferDataType,
                        const size_t *array_idx, void *pDstBuffer) const;

    bool WriteOneElement(const GDALExtendedDataType &dst_datatype,
                         const GDALExtendedDataType &bufferDataType,
                         const size_t *array_idx, const void *pSrcBuffer) const;

    template <typename BufferType, typename NCGetPutVar1FuncType,
              typename ReadOrWriteOneElementType>
    bool
    IReadWriteGeneric(const size_t *arrayStartIdx, const size_t *count,
                      const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                      const GDALExtendedDataType &bufferDataType,
                      BufferType buffer, NCGetPutVar1FuncType NCGetPutVar1Func,
                      ReadOrWriteOneElementType ReadOrWriteOneElement) const;

    template <typename BufferType, typename NCGetPutVar1FuncType,
              typename NCGetPutVaraFuncType, typename NCGetPutVarmFuncType,
              typename ReadOrWriteOneElementType>
    bool IReadWrite(const bool bIsRead, const GUInt64 *arrayStartIdx,
                    const size_t *count, const GInt64 *arrayStep,
                    const GPtrDiff_t *bufferStride,
                    const GDALExtendedDataType &bufferDataType,
                    BufferType buffer, NCGetPutVar1FuncType NCGetPutVar1Func,
                    NCGetPutVaraFuncType NCGetPutVaraFunc,
                    NCGetPutVarmFuncType NCGetPutVarmFunc,
                    ReadOrWriteOneElementType ReadOrWriteOneElement) const;

  protected:
    netCDFVariable(const std::shared_ptr<netCDFSharedResources> &poShared,
                   int gid, int varid,
                   const std::vector<std::shared_ptr<GDALDimension>> &dims,
                   CSLConstList papszOptions);

    bool
    IRead(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
          const size_t *count,             // array of size GetDimensionCount()
          const GInt64 *arrayStep,         // step in elements
          const GPtrDiff_t *bufferStride,  // stride in elements
          const GDALExtendedDataType &bufferDataType,
          void *pDstBuffer) const override;

    bool
    IWrite(const GUInt64 *arrayStartIdx,    // array of size GetDimensionCount()
           const size_t *count,             // array of size GetDimensionCount()
           const GInt64 *arrayStep,         // step in elements
           const GPtrDiff_t *bufferStride,  // stride in elements
           const GDALExtendedDataType &bufferDataType,
           const void *pSrcBuffer) override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;

    void NotifyChildrenOfRenaming() override;

    bool SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                       double dfMean, double dfStdDev, GUInt64 nValidCount,
                       CSLConstList papszOptions) override;

  public:
    static std::shared_ptr<netCDFVariable>
    Create(const std::shared_ptr<netCDFSharedResources> &poShared,
           const std::shared_ptr<netCDFGroup> &poParent, int gid, int varid,
           const std::vector<std::shared_ptr<GDALDimension>> &dims,
           CSLConstList papszOptions, bool bCreate)
    {
        auto var(std::shared_ptr<netCDFVariable>(
            new netCDFVariable(poShared, gid, varid, dims, papszOptions)));
        var->SetSelf(var);
        var->m_poParent = poParent;
        if (poParent)
            poParent->RegisterArray(var.get());
        var->m_bHasWrittenData = !bCreate;
        return var;
    }

    ~netCDFVariable() override;

    void SetUseDefaultFillAsNoData(bool b)
    {
        m_bUseDefaultFillAsNoData = b;
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
    GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions) override;

    bool DeleteAttribute(const std::string &osName,
                         CSLConstList papszOptions) override;

    const void *GetRawNoDataValue() const override;

    bool SetRawNoDataValue(const void *) override;

    std::vector<GUInt64> GetBlockSize() const override;

    CSLConstList GetStructuralInfo() const override;

    const std::string &GetUnit() const override
    {
        return m_osUnit;
    }

    bool SetUnit(const std::string &osUnit) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;

    bool SetSpatialRef(const OGRSpatialReference *poSRS) override;

    double GetOffset(bool *pbHasOffset,
                     GDALDataType *peStorageType) const override;

    double GetScale(bool *pbHasScale,
                    GDALDataType *peStorageType) const override;

    bool SetOffset(double dfOffset, GDALDataType eStorageType) override;

    bool SetScale(double dfScale, GDALDataType eStorageType) override;

    std::vector<std::shared_ptr<GDALMDArray>>
    GetCoordinateVariables() const override;

    bool Resize(const std::vector<GUInt64> &anNewDimSizes,
                CSLConstList) override;

    int GetGroupId() const
    {
        return m_gid;
    }

    int GetVarId() const
    {
        return m_varid;
    }

    static std::string retrieveName(int gid, int varid)
    {
        CPLMutexHolderD(&hNCMutex);
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_varname(gid, varid, szName));
        return szName;
    }

    bool Rename(const std::string &osNewName) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return netCDFGroup::Create(m_poShared, nullptr, m_gid);
    }
};

/************************************************************************/
/*                       ~netCDFSharedResources()                       */
/************************************************************************/

netCDFSharedResources::~netCDFSharedResources()
{
    CPLMutexHolderD(&hNCMutex);

    if (m_cdfid > 0)
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "calling nc_close( %d)", m_cdfid);
#endif
        int status = GDAL_nc_close(m_cdfid);
        NCDF_ERR(status);
    }

#ifdef ENABLE_UFFD
    if (m_pUffdCtx)
    {
        NETCDF_UFFD_UNMAP(m_pUffdCtx);
    }
#endif

    if (m_fpVSIMEM)
        VSIFCloseL(m_fpVSIMEM);

#ifdef ENABLE_NCDUMP
    if (m_bFileToDestroyAtClosing)
        VSIUnlink(m_osFilename);
#endif
}

/************************************************************************/
/*                     NCDFGetParentGroupName()                         */
/************************************************************************/

static CPLString NCDFGetParentGroupName(int gid)
{
    int nParentGID = 0;
    if (nc_inq_grp_parent(gid, &nParentGID) != NC_NOERR)
        return std::string();
    return NCDFGetGroupFullName(nParentGID);
}

/************************************************************************/
/*                             netCDFGroup()                            */
/************************************************************************/

netCDFGroup::netCDFGroup(const std::shared_ptr<netCDFSharedResources> &poShared,
                         int gid)
    : GDALGroup(NCDFGetParentGroupName(gid), retrieveName(gid)),
      m_poShared(poShared), m_gid(gid)
{
    if (m_gid == m_poShared->GetCDFId())
    {
        int nFormat = 0;
        nc_inq_format(m_gid, &nFormat);
        if (nFormat == NC_FORMAT_CLASSIC)
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "CLASSIC");
        }
#ifdef NC_FORMAT_64BIT_OFFSET
        else if (nFormat == NC_FORMAT_64BIT_OFFSET)
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "64BIT_OFFSET");
        }
#endif
#ifdef NC_FORMAT_CDF5
        else if (nFormat == NC_FORMAT_CDF5)
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "CDF5");
        }
#endif
        else if (nFormat == NC_FORMAT_NETCDF4)
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "NETCDF4");
        }
        else if (nFormat == NC_FORMAT_NETCDF4_CLASSIC)
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "NETCDF4_CLASSIC");
        }
    }
}

/************************************************************************/
/*                            ~netCDFGroup()                            */
/************************************************************************/

netCDFGroup::~netCDFGroup()
{
    auto poParent = m_poParent.lock();
    if (poParent)
        poParent->UnRegisterSubGroup(this);
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

/* static */
std::shared_ptr<netCDFGroup>
netCDFGroup::Create(const std::shared_ptr<netCDFSharedResources> &poShared,
                    int cdfid)
{
    auto poGroup =
        std::shared_ptr<netCDFGroup>(new netCDFGroup(poShared, cdfid));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

/* static */
std::shared_ptr<netCDFGroup>
netCDFGroup::Create(const std::shared_ptr<netCDFSharedResources> &poShared,
                    const std::shared_ptr<netCDFGroup> &poParent,
                    int nSubGroupId)
{
    auto poSubGroup = netCDFGroup::Create(poShared, nSubGroupId);
    poSubGroup->m_poParent = poParent;
    if (poParent)
        poParent->RegisterSubGroup(poSubGroup.get());
    return poSubGroup;
}

/************************************************************************/
/*                             CreateGroup()                            */
/************************************************************************/

std::shared_ptr<GDALGroup>
netCDFGroup::CreateGroup(const std::string &osName,
                         CSLConstList /*papszOptions*/)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty group name not supported");
        return nullptr;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int nSubGroupId = -1;
    int ret = nc_def_grp(m_gid, osName.c_str(), &nSubGroupId);
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return nullptr;
    return netCDFGroup::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        nSubGroupId);
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension>
netCDFGroup::CreateDimension(const std::string &osName,
                             const std::string &osType, const std::string &,
                             GUInt64 nSize, CSLConstList papszOptions)
{
    const bool bUnlimited =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "UNLIMITED", "FALSE"));
    if (static_cast<size_t>(nSize) != nSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid size");
        return nullptr;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int nDimId = -1;
    NCDF_ERR(nc_def_dim(m_gid, osName.c_str(),
                        static_cast<size_t>(bUnlimited ? 0 : nSize), &nDimId));
    if (nDimId < 0)
        return nullptr;
    return netCDFDimension::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        m_gid, nDimId, static_cast<size_t>(nSize), osType);
}

/************************************************************************/
/*                     CreateOrGetComplexDataType()                     */
/************************************************************************/

static int CreateOrGetComplexDataType(int gid, GDALDataType eDT)
{
    const char *pszName = "";
    int nSubTypeId = NC_NAT;
    switch (eDT)
    {
        case GDT_CInt16:
            pszName = "ComplexInt16";
            nSubTypeId = NC_SHORT;
            break;
        case GDT_CInt32:
            pszName = "ComplexInt32";
            nSubTypeId = NC_INT;
            break;
        case GDT_CFloat32:
            pszName = "ComplexFloat32";
            nSubTypeId = NC_FLOAT;
            break;
        case GDT_CFloat64:
            pszName = "ComplexFloat64";
            nSubTypeId = NC_DOUBLE;
            break;
        default:
            CPLAssert(false);
            break;
    }
    int nTypeId = NC_NAT;
    if (nc_inq_typeid(gid, pszName, &nTypeId) == NC_NOERR)
    {
        // We could check that the type definition is really the one we want
        return nTypeId;
    }
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    NCDF_ERR(nc_def_compound(gid, nDTSize, pszName, &nTypeId));
    if (nTypeId != NC_NAT)
    {
        NCDF_ERR(nc_insert_compound(gid, nTypeId, "real", 0, nSubTypeId));
        NCDF_ERR(
            nc_insert_compound(gid, nTypeId, "imag", nDTSize / 2, nSubTypeId));
    }
    return nTypeId;
}

/************************************************************************/
/*                    CreateOrGetCompoundDataType()                     */
/************************************************************************/

static int CreateOrGetType(int gid, const GDALExtendedDataType &oType);

static int CreateOrGetCompoundDataType(int gid,
                                       const GDALExtendedDataType &oType)
{
    int nTypeId = NC_NAT;
    if (nc_inq_typeid(gid, oType.GetName().c_str(), &nTypeId) == NC_NOERR)
    {
        // We could check that the type definition is really the one we want
        return nTypeId;
    }
    NCDF_ERR(nc_def_compound(gid, oType.GetSize(), oType.GetName().c_str(),
                             &nTypeId));
    if (nTypeId != NC_NAT)
    {
        for (const auto &comp : oType.GetComponents())
        {
            int nSubTypeId = CreateOrGetType(gid, comp->GetType());
            if (nSubTypeId == NC_NAT)
                return NC_NAT;
            NCDF_ERR(nc_insert_compound(gid, nTypeId, comp->GetName().c_str(),
                                        comp->GetOffset(), nSubTypeId));
        }
    }
    return nTypeId;
}

/************************************************************************/
/*                         CreateOrGetType()                            */
/************************************************************************/

static int CreateOrGetType(int gid, const GDALExtendedDataType &oType)
{
    int nTypeId = NC_NAT;
    const auto typeClass = oType.GetClass();
    if (typeClass == GEDTC_NUMERIC)
    {
        switch (oType.GetNumericDataType())
        {
            case GDT_Byte:
                nTypeId = NC_UBYTE;
                break;
            case GDT_Int8:
                nTypeId = NC_BYTE;
                break;
            case GDT_UInt16:
                nTypeId = NC_USHORT;
                break;
            case GDT_Int16:
                nTypeId = NC_SHORT;
                break;
            case GDT_UInt32:
                nTypeId = NC_UINT;
                break;
            case GDT_Int32:
                nTypeId = NC_INT;
                break;
            case GDT_UInt64:
                nTypeId = NC_UINT64;
                break;
            case GDT_Int64:
                nTypeId = NC_INT64;
                break;
            case GDT_Float32:
                nTypeId = NC_FLOAT;
                break;
            case GDT_Float64:
                nTypeId = NC_DOUBLE;
                break;
            case GDT_CInt16:
            case GDT_CInt32:
            case GDT_CFloat32:
            case GDT_CFloat64:
                nTypeId =
                    CreateOrGetComplexDataType(gid, oType.GetNumericDataType());
                break;
            default:
                break;
        }
    }
    else if (typeClass == GEDTC_STRING)
    {
        nTypeId = NC_STRING;
    }
    else if (typeClass == GEDTC_COMPOUND)
    {
        nTypeId = CreateOrGetCompoundDataType(gid, oType);
    }
    return nTypeId;
}

/************************************************************************/
/*                            CreateMDArray()                           */
/************************************************************************/

std::shared_ptr<GDALMDArray> netCDFGroup::CreateMDArray(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oType, CSLConstList papszOptions)
{
    if (osName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty array name not supported");
        return nullptr;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int nVarId = -1;
    std::vector<int> anDimIds;
    std::vector<std::shared_ptr<GDALDimension>> dims;
    for (const auto &dim : aoDimensions)
    {
        int nDimId = -1;
        auto netCDFDim = std::dynamic_pointer_cast<netCDFDimension>(dim);
        if (netCDFDim)
        {
            nDimId = netCDFDim->GetId();
        }
        else
        {
            if (nc_inq_dimid(m_gid, dim->GetName().c_str(), &nDimId) ==
                NC_NOERR)
            {
                netCDFDim = netCDFDimension::Create(
                    m_poShared,
                    std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
                    m_gid, nDimId, 0, dim->GetType());
                if (netCDFDim->GetSize() != dim->GetSize())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Dimension %s already exists, "
                             "but with a size of " CPL_FRMT_GUIB,
                             dim->GetName().c_str(),
                             static_cast<GUIntBig>(netCDFDim->GetSize()));
                }
            }
            else
            {
                netCDFDim =
                    std::dynamic_pointer_cast<netCDFDimension>(CreateDimension(
                        dim->GetName(), dim->GetType(), dim->GetDirection(),
                        dim->GetSize(), nullptr));
                if (!netCDFDim)
                    return nullptr;
                nDimId = netCDFDim->GetId();
            }
        }
        anDimIds.push_back(nDimId);
        dims.emplace_back(netCDFDim);
    }
    int nTypeId = CreateOrGetType(m_gid, oType);
    if (nTypeId == NC_NAT)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unhandled data type");
        return nullptr;
    }
    const char *pszType = CSLFetchNameValueDef(papszOptions, "NC_TYPE", "");
    if ((EQUAL(pszType, "") || EQUAL(pszType, "NC_CHAR")) && dims.size() == 1 &&
        oType.GetClass() == GEDTC_STRING && oType.GetMaxStringLength() > 0)
    {
        nTypeId = NC_CHAR;
        auto dimLength =
            std::dynamic_pointer_cast<netCDFDimension>(CreateDimension(
                aoDimensions[0]->GetName() + "_length", std::string(),
                std::string(), oType.GetMaxStringLength(), nullptr));
        if (!dimLength)
            return nullptr;
        anDimIds.push_back(dimLength->GetId());
    }
    else if (EQUAL(pszType, "NC_BYTE"))
        nTypeId = NC_BYTE;
    else if (EQUAL(pszType, "NC_INT64"))
        nTypeId = NC_INT64;
    else if (EQUAL(pszType, "NC_UINT64"))
        nTypeId = NC_UINT64;
    NCDF_ERR(nc_def_var(m_gid, osName.c_str(), nTypeId,
                        static_cast<int>(anDimIds.size()),
                        anDimIds.empty() ? nullptr : anDimIds.data(), &nVarId));
    if (nVarId < 0)
        return nullptr;

    const char *pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if (pszBlockSize &&
        /* ignore for now BLOCKSIZE for 1-dim string variables created as 2-dim
         */
        anDimIds.size() == aoDimensions.size())
    {
        auto aszTokens(CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if (static_cast<size_t>(aszTokens.size()) != aoDimensions.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return nullptr;
        }
        if (!aoDimensions.empty())
        {
            std::vector<size_t> anChunkSize(aoDimensions.size());
            for (size_t i = 0; i < anChunkSize.size(); ++i)
            {
                anChunkSize[i] =
                    static_cast<size_t>(CPLAtoGIntBig(aszTokens[i]));
            }
            int ret =
                nc_def_var_chunking(m_gid, nVarId, NC_CHUNKED, &anChunkSize[0]);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return nullptr;
        }
    }

    const char *pszCompress = CSLFetchNameValue(papszOptions, "COMPRESS");
    if (pszCompress && EQUAL(pszCompress, "DEFLATE"))
    {
        int nZLevel = NCDF_DEFLATE_LEVEL;
        const char *pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
        if (pszZLevel != nullptr)
        {
            nZLevel = atoi(pszZLevel);
            if (!(nZLevel >= 1 && nZLevel <= 9))
            {
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "ZLEVEL=%s value not recognised, ignoring.",
                         pszZLevel);
                nZLevel = NCDF_DEFLATE_LEVEL;
            }
        }
        int ret = nc_def_var_deflate(m_gid, nVarId, TRUE /* shuffle */,
                                     TRUE /* deflate on */, nZLevel);
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return nullptr;
    }

    const char *pszFilter = CSLFetchNameValue(papszOptions, "FILTER");
    if (pszFilter)
    {
#ifdef NC_EFILTER
        const auto aosTokens(
            CPLStringList(CSLTokenizeString2(pszFilter, ",", 0)));
        if (!aosTokens.empty())
        {
            const unsigned nFilterId =
                static_cast<unsigned>(CPLAtoGIntBig(aosTokens[0]));
            std::vector<unsigned> anParams;
            for (int i = 1; i < aosTokens.size(); ++i)
            {
                anParams.push_back(
                    static_cast<unsigned>(CPLAtoGIntBig(aosTokens[i])));
            }
            int ret = nc_def_var_filter(m_gid, nVarId, nFilterId,
                                        anParams.size(), anParams.data());
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return nullptr;
        }
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "netCDF 4.6 or later needed for FILTER option");
        return nullptr;
#endif
    }

    const bool bChecksum =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "CHECKSUM", "FALSE"));
    if (bChecksum)
    {
        int ret = nc_def_var_fletcher32(m_gid, nVarId, TRUE);
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return nullptr;
    }

    return netCDFVariable::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        m_gid, nVarId, dims, papszOptions, true);
}

/************************************************************************/
/*                          CreateAttribute()                           */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFGroup::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    return netCDFAttribute::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        m_gid, NC_GLOBAL, osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                         DeleteAttribute()                            */
/************************************************************************/

bool netCDFGroup::DeleteAttribute(const std::string &osName,
                                  CSLConstList /*papszOptions*/)
{
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret = nc_del_att(m_gid, NC_GLOBAL, osName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    auto it = m_oMapAttributes.find(osName);
    if (it != m_oMapAttributes.end())
    {
        it->second->Deleted();
        m_oMapAttributes.erase(it);
    }

    return true;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string>
netCDFGroup::GetGroupNames(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nSubGroups = 0;
    NCDF_ERR(nc_inq_grps(m_gid, &nSubGroups, nullptr));
    if (nSubGroups == 0)
    {
        if (EQUAL(CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""),
                  "SAME_DIMENSION"))
        {
            std::vector<std::string> names;
            std::set<std::string> oSetDimNames;
            for (const auto &osArrayName : GetMDArrayNames(nullptr))
            {
                const auto poArray = OpenMDArray(osArrayName, nullptr);
                const auto &apoDims = poArray->GetDimensions();
                if (apoDims.size() == 1)
                {
                    const auto &osDimName = apoDims[0]->GetName();
                    if (oSetDimNames.find(osDimName) == oSetDimNames.end())
                    {
                        oSetDimNames.insert(osDimName);
                        names.emplace_back(osDimName);
                    }
                }
            }
            return names;
        }
        return {};
    }
    std::vector<int> anSubGroupdsIds(nSubGroups);
    NCDF_ERR(nc_inq_grps(m_gid, nullptr, &anSubGroupdsIds[0]));
    std::vector<std::string> names;
    names.reserve(nSubGroups);
    for (const auto &subgid : anSubGroupdsIds)
    {
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_grpname(subgid, szName));
        if (GetFullName() == "/" && EQUAL(szName, "METADATA"))
        {
            const auto poMetadata = OpenGroup(szName);
            if (poMetadata && poMetadata->OpenGroup("ISO_METADATA"))
                continue;
        }
        names.emplace_back(szName);
    }
    return names;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup>
netCDFGroup::OpenGroup(const std::string &osName,
                       CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nSubGroups = 0;
    // This is weird but nc_inq_grp_ncid() succeeds on a single group file.
    NCDF_ERR(nc_inq_grps(m_gid, &nSubGroups, nullptr));
    if (nSubGroups == 0)
    {
        if (EQUAL(CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""),
                  "SAME_DIMENSION"))
        {
            const auto oCandidateGroupNames = GetGroupNames(papszOptions);
            for (const auto &osCandidateGroupName : oCandidateGroupNames)
            {
                if (osCandidateGroupName == osName)
                {
                    auto poThisGroup = netCDFGroup::Create(m_poShared, m_gid);
                    return netCDFVirtualGroupBySameDimension::Create(
                        poThisGroup, osName);
                }
            }
        }
        return nullptr;
    }
    int nSubGroupId = 0;
    if (nc_inq_grp_ncid(m_gid, osName.c_str(), &nSubGroupId) != NC_NOERR ||
        nSubGroupId <= 0)
        return nullptr;
    return netCDFGroup::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        nSubGroupId);
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string>
netCDFGroup::GetMDArrayNames(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nVars = 0;
    NCDF_ERR(nc_inq_nvars(m_gid, &nVars));
    if (nVars == 0)
        return {};
    std::vector<int> anVarIds(nVars);
    NCDF_ERR(nc_inq_varids(m_gid, nullptr, &anVarIds[0]));
    std::vector<std::string> names;
    names.reserve(nVars);
    const bool bAll =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    const bool bZeroDim =
        bAll ||
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ZERO_DIM", "NO"));
    const bool bCoordinates =
        bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions,
                                                 "SHOW_COORDINATES", "YES"));
    const bool bBounds =
        bAll ||
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_BOUNDS", "YES"));
    const bool bIndexing =
        bAll ||
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_INDEXING", "YES"));
    const bool bTime =
        bAll ||
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_TIME", "YES"));
    std::set<std::string> ignoreList;
    if (!bCoordinates || !bBounds)
    {
        for (const auto &varid : anVarIds)
        {
            char **papszTokens = nullptr;
            if (!bCoordinates)
            {
                char *pszTemp = nullptr;
                if (NCDFGetAttr(m_gid, varid, "coordinates", &pszTemp) ==
                    CE_None)
                    papszTokens = NCDFTokenizeCoordinatesAttribute(pszTemp);
                CPLFree(pszTemp);
            }
            if (!bBounds)
            {
                char *pszTemp = nullptr;
                if (NCDFGetAttr(m_gid, varid, "bounds", &pszTemp) == CE_None &&
                    pszTemp != nullptr && !EQUAL(pszTemp, ""))
                    papszTokens = CSLAddString(papszTokens, pszTemp);
                CPLFree(pszTemp);
            }
            for (char **iter = papszTokens; iter && iter[0]; ++iter)
                ignoreList.insert(*iter);
            CSLDestroy(papszTokens);
        }
    }

    const bool bGroupBySameDimension = EQUAL(
        CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""), "SAME_DIMENSION");
    for (const auto &varid : anVarIds)
    {
        int nVarDims = 0;
        NCDF_ERR(nc_inq_varndims(m_gid, varid, &nVarDims));
        if (nVarDims == 0 && !bZeroDim)
        {
            continue;
        }
        if (nVarDims == 1 && bGroupBySameDimension)
        {
            continue;
        }

        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_varname(m_gid, varid, szName));
        if (!bIndexing && nVarDims == 1)
        {
            int nDimId = 0;
            NCDF_ERR(nc_inq_vardimid(m_gid, varid, &nDimId));
            char szDimName[NC_MAX_NAME + 1] = {};
            NCDF_ERR(nc_inq_dimname(m_gid, nDimId, szDimName));
            if (strcmp(szDimName, szName) == 0)
            {
                continue;
            }
        }

        if (!bTime)
        {
            char *pszTemp = nullptr;
            bool bSkip = false;
            if (NCDFGetAttr(m_gid, varid, "standard_name", &pszTemp) == CE_None)
            {
                bSkip = pszTemp && strcmp(pszTemp, "time") == 0;
            }
            CPLFree(pszTemp);
            if (bSkip)
            {
                continue;
            }
        }

        if (ignoreList.find(szName) == ignoreList.end())
        {
            names.emplace_back(szName);
        }
    }
    return names;
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool netCDFGroup::Rename(const std::string &osNewName)
{
    if (m_poShared->IsReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Rename() not supported on read-only file");
        return false;
    }
    if (osNewName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Empty name not supported");
        return false;
    }
    if (m_osName == "/")
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot rename root group");
        return false;
    }

    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret = nc_rename_grp(m_gid, osNewName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                       NotifyChildrenOfRenaming()                     */
/************************************************************************/

void netCDFGroup::NotifyChildrenOfRenaming()
{
    for (const auto poSubGroup : m_oSetGroups)
        poSubGroup->ParentRenamed(m_osFullName);

    for (const auto poDim : m_oSetDimensions)
        poDim->ParentRenamed(m_osFullName);

    for (const auto poArray : m_oSetArrays)
        poArray->ParentRenamed(m_osFullName);

    for (const auto &iter : m_oMapAttributes)
        iter.second->ParentRenamed(m_osFullName);
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray>
netCDFGroup::OpenMDArray(const std::string &osName,
                         CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nVarId = 0;
    if (nc_inq_varid(m_gid, osName.c_str(), &nVarId) != NC_NOERR)
        return nullptr;
    auto poVar = netCDFVariable::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        m_gid, nVarId, std::vector<std::shared_ptr<GDALDimension>>(), nullptr,
        false);
    if (poVar)
    {
        poVar->SetUseDefaultFillAsNoData(CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "USE_DEFAULT_FILL_AS_NODATA", "NO")));
    }
    return poVar;
}

/************************************************************************/
/*                         GetDimensions()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
netCDFGroup::GetDimensions(CSLConstList) const
{
    CPLMutexHolderD(&hNCMutex);
    int nbDims = 0;
    NCDF_ERR(nc_inq_ndims(m_gid, &nbDims));
    if (nbDims == 0)
        return {};
    std::vector<int> dimids(nbDims);
    NCDF_ERR(nc_inq_dimids(m_gid, &nbDims, &dimids[0], FALSE));
    std::vector<std::shared_ptr<GDALDimension>> res;
    for (int i = 0; i < nbDims; i++)
    {
        auto poCachedDim = m_poShared->GetCachedDimension(dimids[i]);
        if (poCachedDim == nullptr)
        {
            poCachedDim = netCDFDimension::Create(
                m_poShared,
                std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()), m_gid,
                dimids[i], 0, std::string());
            m_poShared->CacheDimension(dimids[i], poCachedDim);
        }
        res.emplace_back(poCachedDim);
    }
    return res;
}

/************************************************************************/
/*                         GetAttribute()                               */
/************************************************************************/

static const char *const apszJSONMDKeys[] = {
    "ISO_METADATA",  "ESA_METADATA",        "EOP_METADATA",
    "QA_STATISTICS", "GRANULE_DESCRIPTION", "ALGORITHM_SETTINGS"};

std::shared_ptr<GDALAttribute>
netCDFGroup::GetAttribute(const std::string &osName) const
{
    CPLMutexHolderD(&hNCMutex);
    int nAttId = -1;
    if (nc_inq_attid(m_gid, NC_GLOBAL, osName.c_str(), &nAttId) != NC_NOERR)
    {
        if (GetFullName() == "/")
        {
            for (const char *key : apszJSONMDKeys)
            {
                if (osName == key)
                {
                    auto poMetadata = OpenGroup("METADATA");
                    if (poMetadata)
                    {
                        auto poSubMetadata =
                            std::dynamic_pointer_cast<netCDFGroup>(
                                poMetadata->OpenGroup(key));
                        if (poSubMetadata)
                        {
                            const auto osJson =
                                NCDFReadMetadataAsJson(poSubMetadata->m_gid);
                            return std::make_shared<GDALAttributeString>(
                                GetFullName(), key, osJson, GEDTST_JSON);
                        }
                    }
                    break;
                }
            }
        }
        return nullptr;
    }
    return netCDFAttribute::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()),
        m_gid, NC_GLOBAL, osName);
}

/************************************************************************/
/*                         GetAttributes()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
netCDFGroup::GetAttributes(CSLConstList) const
{
    CPLMutexHolderD(&hNCMutex);
    std::vector<std::shared_ptr<GDALAttribute>> res;
    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(m_gid, NC_GLOBAL, &nbAttr));
    res.reserve(nbAttr);
    for (int i = 0; i < nbAttr; i++)
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(m_gid, NC_GLOBAL, i, szAttrName));
        if (!EQUAL(szAttrName, "_NCProperties"))
        {
            res.emplace_back(netCDFAttribute::Create(
                m_poShared,
                std::dynamic_pointer_cast<netCDFGroup>(m_pSelf.lock()), m_gid,
                NC_GLOBAL, szAttrName));
        }
    }

    if (GetFullName() == "/")
    {
        auto poMetadata = OpenGroup("METADATA");
        if (poMetadata)
        {
            for (const char *key : apszJSONMDKeys)
            {
                auto poSubMetadata = std::dynamic_pointer_cast<netCDFGroup>(
                    poMetadata->OpenGroup(key));
                if (poSubMetadata)
                {
                    const auto osJson =
                        NCDFReadMetadataAsJson(poSubMetadata->m_gid);
                    res.emplace_back(std::make_shared<GDALAttributeString>(
                        GetFullName(), key, osJson, GEDTST_JSON));
                }
            }
        }
    }

    return res;
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

CSLConstList netCDFGroup::GetStructuralInfo() const
{
    return m_aosStructuralInfo.List();
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void netCDFGroup::ClearStatistics()
{
    m_poShared->GetPAM()->ClearStatistics();
}

/************************************************************************/
/*                   netCDFVirtualGroupBySameDimension()                */
/************************************************************************/

netCDFVirtualGroupBySameDimension::netCDFVirtualGroupBySameDimension(
    const std::shared_ptr<netCDFGroup> &poGroup, const std::string &osDimName)
    : GDALGroup(poGroup->GetName(), osDimName), m_poGroup(poGroup),
      m_osDimName(osDimName)
{
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

/* static */ std::shared_ptr<netCDFVirtualGroupBySameDimension>
netCDFVirtualGroupBySameDimension::Create(
    const std::shared_ptr<netCDFGroup> &poGroup, const std::string &osDimName)
{
    auto poNewGroup = std::shared_ptr<netCDFVirtualGroupBySameDimension>(
        new netCDFVirtualGroupBySameDimension(poGroup, osDimName));
    poNewGroup->SetSelf(poNewGroup);
    return poNewGroup;
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string>
netCDFVirtualGroupBySameDimension::GetMDArrayNames(CSLConstList) const
{
    const auto srcNames = m_poGroup->GetMDArrayNames(nullptr);
    std::vector<std::string> names;
    for (const auto &srcName : srcNames)
    {
        auto poArray = m_poGroup->OpenMDArray(srcName, nullptr);
        if (poArray)
        {
            const auto &apoArrayDims = poArray->GetDimensions();
            if (apoArrayDims.size() == 1 &&
                apoArrayDims[0]->GetName() == m_osDimName)
            {
                names.emplace_back(srcName);
            }
        }
    }
    return names;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray>
netCDFVirtualGroupBySameDimension::OpenMDArray(const std::string &osName,
                                               CSLConstList papszOptions) const
{
    return m_poGroup->OpenMDArray(osName, papszOptions);
}

/************************************************************************/
/*                           netCDFDimension()                          */
/************************************************************************/

netCDFDimension::netCDFDimension(
    const std::shared_ptr<netCDFSharedResources> &poShared, int cfid, int dimid,
    size_t nForcedSize, const std::string &osType)
    : GDALDimension(NCDFGetGroupFullName(cfid), retrieveName(cfid, dimid),
                    osType,         // type
                    std::string(),  // direction
                    nForcedSize ? nForcedSize : retrieveSize(cfid, dimid)),
      m_poShared(poShared), m_gid(cfid), m_dimid(dimid)
{
    if (m_osType.empty() && nForcedSize == 0)
    {
        auto var =
            std::dynamic_pointer_cast<netCDFVariable>(GetIndexingVariable());
        if (var)
        {
            const auto gid = var->GetGroupId();
            const auto varid = var->GetVarId();
            const auto varname = var->GetName().c_str();
            if (NCDFIsVarLongitude(gid, varid, varname) ||
                NCDFIsVarProjectionX(gid, varid, varname))
            {
                m_osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                auto attrPositive = var->GetAttribute(CF_UNITS);
                if (attrPositive)
                {
                    const auto val = attrPositive->ReadAsString();
                    if (val)
                    {
                        if (EQUAL(val, CF_DEGREES_EAST))
                        {
                            m_osDirection = "EAST";
                        }
                    }
                }
            }
            else if (NCDFIsVarLatitude(gid, varid, varname) ||
                     NCDFIsVarProjectionY(gid, varid, varname))
            {
                m_osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                auto attrPositive = var->GetAttribute(CF_UNITS);
                if (attrPositive)
                {
                    const auto val = attrPositive->ReadAsString();
                    if (val)
                    {
                        if (EQUAL(val, CF_DEGREES_NORTH))
                        {
                            m_osDirection = "NORTH";
                        }
                    }
                }
            }
            else if (NCDFIsVarVerticalCoord(gid, varid, varname))
            {
                m_osType = GDAL_DIM_TYPE_VERTICAL;
                auto attrPositive = var->GetAttribute("positive");
                if (attrPositive)
                {
                    const auto val = attrPositive->ReadAsString();
                    if (val)
                    {
                        if (EQUAL(val, "up"))
                        {
                            m_osDirection = "UP";
                        }
                        else if (EQUAL(val, "down"))
                        {
                            m_osDirection = "DOWN";
                        }
                    }
                }
            }
            else if (NCDFIsVarTimeCoord(gid, varid, varname))
            {
                m_osType = GDAL_DIM_TYPE_TEMPORAL;
            }
        }
    }
}

/************************************************************************/
/*                          ~netCDFDimension()                          */
/************************************************************************/

netCDFDimension::~netCDFDimension()
{
    auto poParent = m_poParent.lock();
    if (poParent)
        poParent->UnRegisterDimension(this);
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

/* static */
std::shared_ptr<netCDFDimension>
netCDFDimension::Create(const std::shared_ptr<netCDFSharedResources> &poShared,
                        const std::shared_ptr<netCDFGroup> &poParent, int cfid,
                        int dimid, size_t nForcedSize,
                        const std::string &osType)
{
    auto poDim(std::make_shared<netCDFDimension>(poShared, cfid, dimid,
                                                 nForcedSize, osType));
    poDim->m_poParent = poParent;
    if (poParent)
        poParent->RegisterDimension(poDim.get());
    return poDim;
}

/************************************************************************/
/*                         GetIndexingVariable()                        */
/************************************************************************/

namespace
{
struct SetIsInGetIndexingVariable
{
    netCDFSharedResources *m_poShared;

    explicit SetIsInGetIndexingVariable(
        netCDFSharedResources *poSharedResources)
        : m_poShared(poSharedResources)
    {
        m_poShared->SetIsInGetIndexingVariable(true);
    }

    ~SetIsInGetIndexingVariable()
    {
        m_poShared->SetIsInGetIndexingVariable(false);
    }
};
}  // namespace

std::shared_ptr<GDALMDArray> netCDFDimension::GetIndexingVariable() const
{
    if (m_poShared->GetIsInIndexingVariable())
        return nullptr;

    SetIsInGetIndexingVariable setterIsInGetIndexingVariable(m_poShared.get());

    CPLMutexHolderD(&hNCMutex);

    // First try to find a variable in this group with the same name as the
    // dimension
    int nVarId = 0;
    if (nc_inq_varid(m_gid, GetName().c_str(), &nVarId) == NC_NOERR)
    {
        int nDims = 0;
        NCDF_ERR(nc_inq_varndims(m_gid, nVarId, &nDims));
        int nVarType = NC_NAT;
        NCDF_ERR(nc_inq_vartype(m_gid, nVarId, &nVarType));
        if (nDims == 1 || (nDims == 2 && nVarType == NC_CHAR))
        {
            int anDimIds[2] = {};
            NCDF_ERR(nc_inq_vardimid(m_gid, nVarId, anDimIds));
            if (anDimIds[0] == m_dimid)
            {
                if (nDims == 2)
                {
                    // Check that there is no variable with the same of the
                    // second dimension.
                    char szExtraDim[NC_MAX_NAME + 1] = {};
                    NCDF_ERR(nc_inq_dimname(m_gid, anDimIds[1], szExtraDim));
                    int nUnused;
                    if (nc_inq_varid(m_gid, szExtraDim, &nUnused) == NC_NOERR)
                        return nullptr;
                }

                return netCDFVariable::Create(
                    m_poShared, m_poParent.lock(), m_gid, nVarId,
                    std::vector<std::shared_ptr<GDALDimension>>(), nullptr,
                    false);
            }
        }
    }

    // Otherwise explore the variables in this group to find one that has a
    // "coordinates" attribute that references this dimension. If so, let's
    // return the variable pointed by the value of "coordinates" as the indexing
    // variable. This assumes that there is no other variable that would use
    // another variable for the matching dimension of its "coordinates".
    netCDFGroup oGroup(m_poShared, m_gid);
    const auto arrayNames = oGroup.GetMDArrayNames(nullptr);
    std::shared_ptr<GDALMDArray> candidateIndexingVariable;
    for (const auto &arrayName : arrayNames)
    {
        const auto poArray = oGroup.OpenMDArray(arrayName, nullptr);
        const auto poArrayNC =
            std::dynamic_pointer_cast<netCDFVariable>(poArray);
        if (!poArrayNC)
            continue;

        const auto &apoArrayDims = poArray->GetDimensions();
        if (apoArrayDims.size() == 1)
        {
            const auto &poArrayDim = apoArrayDims[0];
            const auto poArrayDimNC =
                std::dynamic_pointer_cast<netCDFDimension>(poArrayDim);
            if (poArrayDimNC && poArrayDimNC->m_gid == m_gid &&
                poArrayDimNC->m_dimid == m_dimid)
            {
                // If the array doesn't have a coordinates variable, but is a 1D
                // array indexed by our dimension, then use it as the indexing
                // variable, provided it is the only such variable.
                if (!candidateIndexingVariable)
                {
                    candidateIndexingVariable = poArray;
                }
                else
                {
                    return nullptr;
                }
                continue;
            }
        }

        const auto poCoordinates = poArray->GetAttribute("coordinates");
        if (!(poCoordinates &&
              poCoordinates->GetDataType().GetClass() == GEDTC_STRING))
        {
            continue;
        }

        // Check that the arrays has as many dimensions as its coordinates
        // attribute
        const CPLStringList aosCoordinates(
            NCDFTokenizeCoordinatesAttribute(poCoordinates->ReadAsString()));
        if (apoArrayDims.size() != static_cast<size_t>(aosCoordinates.size()))
            continue;

        for (size_t i = 0; i < apoArrayDims.size(); ++i)
        {
            const auto &poArrayDim = apoArrayDims[i];
            const auto poArrayDimNC =
                std::dynamic_pointer_cast<netCDFDimension>(poArrayDim);

            // Check if the array is indexed by the current dimension
            if (!(poArrayDimNC && poArrayDimNC->m_gid == m_gid &&
                  poArrayDimNC->m_dimid == m_dimid))
            {
                continue;
            }

            // Caution: some datasets have their coordinates variables in the
            // same order than dimensions (i.e. from slowest varying to
            // fastest varying), while others have the coordinates variables
            // in the opposite order.
            // Assume same order by default, but if we find the first variable
            // to be of longitude/X type, then assume the opposite order.
            bool coordinatesInSameOrderThanDimensions = true;
            if (aosCoordinates.size() > 1)
            {
                int bFirstGroupId = -1;
                int nFirstVarId = -1;
                if (NCDFResolveVar(poArrayNC->GetGroupId(), aosCoordinates[0],
                                   &bFirstGroupId, &nVarId, false) == CE_None &&
                    (NCDFIsVarLongitude(bFirstGroupId, nFirstVarId,
                                        aosCoordinates[0]) ||
                     NCDFIsVarProjectionX(bFirstGroupId, nFirstVarId,
                                          aosCoordinates[0])))
                {
                    coordinatesInSameOrderThanDimensions = false;
                }
            }

            int nIndexingVarGroupId = -1;
            int nIndexingVarId = -1;
            const size_t nIdxCoordinate = coordinatesInSameOrderThanDimensions
                                              ? i
                                              : aosCoordinates.size() - 1 - i;
            if (NCDFResolveVar(
                    poArrayNC->GetGroupId(), aosCoordinates[nIdxCoordinate],
                    &nIndexingVarGroupId, &nIndexingVarId, false) == CE_None)
            {
                return netCDFVariable::Create(
                    m_poShared, m_poParent.lock(), nIndexingVarGroupId,
                    nIndexingVarId,
                    std::vector<std::shared_ptr<GDALDimension>>(), nullptr,
                    false);
            }
        }
    }

    return candidateIndexingVariable;
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool netCDFDimension::Rename(const std::string &osNewName)
{
    if (m_poShared->IsReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Rename() not supported on read-only file");
        return false;
    }
    if (osNewName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Empty name not supported");
        return false;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret = nc_rename_dim(m_gid, m_dimid, osNewName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                          netCDFVariable()                            */
/************************************************************************/

netCDFVariable::netCDFVariable(
    const std::shared_ptr<netCDFSharedResources> &poShared, int gid, int varid,
    const std::vector<std::shared_ptr<GDALDimension>> &dims,
    CSLConstList papszOptions)
    : GDALAbstractMDArray(NCDFGetGroupFullName(gid), retrieveName(gid, varid)),
      GDALPamMDArray(NCDFGetGroupFullName(gid), retrieveName(gid, varid),
                     poShared->GetPAM()),
      m_poShared(poShared), m_gid(gid), m_varid(varid), m_dims(dims)
{
    NCDF_ERR(nc_inq_varndims(m_gid, m_varid, &m_nDims));
    NCDF_ERR(nc_inq_vartype(m_gid, m_varid, &m_nVarType));
    if (m_nDims == 2 && m_nVarType == NC_CHAR)
    {
        int anDimIds[2] = {};
        NCDF_ERR(nc_inq_vardimid(m_gid, m_varid, &anDimIds[0]));

        // Check that there is no variable with the same of the
        // second dimension.
        char szExtraDim[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_dimname(m_gid, anDimIds[1], szExtraDim));
        int nUnused;
        if (nc_inq_varid(m_gid, szExtraDim, &nUnused) != NC_NOERR)
        {
            NCDF_ERR(nc_inq_dimlen(m_gid, anDimIds[1], &m_nTextLength));
        }
    }

    int nShuffle = 0;
    int nDeflate = 0;
    int nDeflateLevel = 0;
    if (nc_inq_var_deflate(m_gid, m_varid, &nShuffle, &nDeflate,
                           &nDeflateLevel) == NC_NOERR)
    {
        if (nDeflate)
        {
            m_aosStructuralInfo.SetNameValue("COMPRESS", "DEFLATE");
        }
    }
    auto unit = netCDFVariable::GetAttribute(CF_UNITS);
    if (unit && unit->GetDataType().GetClass() == GEDTC_STRING)
    {
        const char *pszVal = unit->ReadAsString();
        if (pszVal)
            m_osUnit = pszVal;
    }
    m_bWriteGDALTags = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_TAGS", "YES"));
}

/************************************************************************/
/*                          ~netCDFVariable()                           */
/************************************************************************/

netCDFVariable::~netCDFVariable()
{
    auto poParent = m_poParent.lock();
    if (poParent)
        poParent->UnRegisterArray(this);

    if (!m_poShared->IsReadOnly() && !m_dims.empty())
    {
        bool bNeedToWriteDummy = false;
        for (auto &poDim : m_dims)
        {
            auto netCDFDim = std::dynamic_pointer_cast<netCDFDimension>(poDim);
            CPLAssert(netCDFDim);
            if (netCDFDim->GetSize() > netCDFDim->GetActualSize())
            {
                bNeedToWriteDummy = true;
                break;
            }
        }
        if (bNeedToWriteDummy)
        {
            CPLDebug("netCDF", "Extending array %s to new dimension sizes",
                     GetName().c_str());
            m_bGetRawNoDataValueHasRun = false;
            m_bUseDefaultFillAsNoData = true;
            const void *pNoData = GetRawNoDataValue();
            std::vector<GByte> abyDummy(GetDataType().GetSize());
            if (pNoData == nullptr)
                pNoData = abyDummy.data();
            const auto nDimCount = m_dims.size();
            std::vector<GUInt64> arrayStartIdx(nDimCount);
            std::vector<size_t> count(nDimCount, 1);
            std::vector<GInt64> arrayStep(nDimCount, 0);
            std::vector<GPtrDiff_t> bufferStride(nDimCount, 0);
            for (size_t i = 0; i < nDimCount; ++i)
            {
                arrayStartIdx[i] = m_dims[i]->GetSize() - 1;
            }
            Write(arrayStartIdx.data(), count.data(), arrayStep.data(),
                  bufferStride.data(), GetDataType(), pNoData);
        }
    }
}

/************************************************************************/
/*                             GetDimensions()                          */
/************************************************************************/

const std::vector<std::shared_ptr<GDALDimension>> &
netCDFVariable::GetDimensions() const
{
    if (m_nDims == 0 || !m_dims.empty())
        return m_dims;
    CPLMutexHolderD(&hNCMutex);
    std::vector<int> anDimIds(m_nDims);
    NCDF_ERR(nc_inq_vardimid(m_gid, m_varid, &anDimIds[0]));
    if (m_nDims == 2 && m_nVarType == NC_CHAR && m_nTextLength > 0)
        anDimIds.resize(1);
    m_dims.reserve(m_nDims);
    for (const auto &dimid : anDimIds)
    {
        auto poCachedDim = m_poShared->GetCachedDimension(dimid);
        if (poCachedDim == nullptr)
        {
            const int groupDim =
                m_poShared->GetBelongingGroupOfDim(m_gid, dimid);
            poCachedDim =
                netCDFDimension::Create(m_poShared, m_poParent.lock(), groupDim,
                                        dimid, 0, std::string());
            m_poShared->CacheDimension(dimid, poCachedDim);
        }
        m_dims.emplace_back(poCachedDim);
    }
    return m_dims;
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

CSLConstList netCDFVariable::GetStructuralInfo() const
{
    return m_aosStructuralInfo.List();
}

/************************************************************************/
/*                          GetComplexDataType()                        */
/************************************************************************/

static GDALDataType GetComplexDataType(int gid, int nVarType)
{
    // First enquire and check that the number of fields is 2
    size_t nfields = 0;
    size_t compoundsize = 0;
    char szName[NC_MAX_NAME + 1] = {};
    if (nc_inq_compound(gid, nVarType, szName, &compoundsize, &nfields) !=
        NC_NOERR)
    {
        return GDT_Unknown;
    }

    if (nfields != 2 || !STARTS_WITH_CI(szName, "complex"))
    {
        return GDT_Unknown;
    }

    // Now check that that two types are the same in the struct.
    nc_type field_type1, field_type2;
    int field_dims1, field_dims2;
    if (nc_inq_compound_field(gid, nVarType, 0, nullptr, nullptr, &field_type1,
                              &field_dims1, nullptr) != NC_NOERR)
    {
        return GDT_Unknown;
    }

    if (nc_inq_compound_field(gid, nVarType, 0, nullptr, nullptr, &field_type2,
                              &field_dims2, nullptr) != NC_NOERR)
    {
        return GDT_Unknown;
    }

    if ((field_type1 != field_type2) || (field_dims1 != field_dims2) ||
        (field_dims1 != 0))
    {
        return GDT_Unknown;
    }

    if (field_type1 == NC_SHORT)
    {
        return GDT_CInt16;
    }
    else if (field_type1 == NC_INT)
    {
        return GDT_CInt32;
    }
    else if (field_type1 == NC_FLOAT)
    {
        return GDT_CFloat32;
    }
    else if (field_type1 == NC_DOUBLE)
    {
        return GDT_CFloat64;
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                       GetCompoundDataType()                          */
/************************************************************************/

static bool BuildDataType(int gid, int varid, int nVarType,
                          std::unique_ptr<GDALExtendedDataType> &dt,
                          bool &bPerfectDataTypeMatch);

static bool GetCompoundDataType(int gid, int nVarType,
                                std::unique_ptr<GDALExtendedDataType> &dt,
                                bool &bPerfectDataTypeMatch)
{
    size_t nfields = 0;
    size_t compoundsize = 0;
    char szName[NC_MAX_NAME + 1] = {};
    if (nc_inq_compound(gid, nVarType, szName, &compoundsize, &nfields) !=
        NC_NOERR)
    {
        return false;
    }
    bPerfectDataTypeMatch = true;
    std::vector<std::unique_ptr<GDALEDTComponent>> comps;
    for (size_t i = 0; i < nfields; i++)
    {
        nc_type field_type = 0;
        int field_dims = 0;
        size_t field_offset = 0;
        char field_name[NC_MAX_NAME + 1] = {};
        if (nc_inq_compound_field(gid, nVarType, static_cast<int>(i),
                                  field_name, &field_offset, &field_type,
                                  &field_dims, nullptr) != NC_NOERR)
        {
            return false;
        }
        if (field_dims != 0)
        {
            // We don't support that
            return false;
        }
        std::unique_ptr<GDALExtendedDataType> subDt;
        bool bSubPerfectDataTypeMatch = false;
        if (!BuildDataType(gid, -1, field_type, subDt,
                           bSubPerfectDataTypeMatch))
        {
            return false;
        }
        if (!bSubPerfectDataTypeMatch)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Non native GDAL type found in a component of a compound type");
            return false;
        }
        auto comp = std::unique_ptr<GDALEDTComponent>(new GDALEDTComponent(
            std::string(field_name), field_offset, *subDt));
        comps.emplace_back(std::move(comp));
    }
    dt.reset(new GDALExtendedDataType(
        GDALExtendedDataType::Create(szName, compoundsize, std::move(comps))));

    return dt->GetClass() == GEDTC_COMPOUND;
}

/************************************************************************/
/*                            BuildDataType()                           */
/************************************************************************/

static bool BuildDataType(int gid, int varid, int nVarType,
                          std::unique_ptr<GDALExtendedDataType> &dt,
                          bool &bPerfectDataTypeMatch)
{
    GDALDataType eDataType = GDT_Unknown;
    bPerfectDataTypeMatch = false;
    if (NCDFIsUserDefinedType(gid, nVarType))
    {
        nc_type nBaseType = NC_NAT;
        int eClass = 0;
        nc_inq_user_type(gid, nVarType, nullptr, nullptr, &nBaseType, nullptr,
                         &eClass);
        if (eClass == NC_COMPOUND)
        {
            eDataType = GetComplexDataType(gid, nVarType);
            if (eDataType != GDT_Unknown)
            {
                bPerfectDataTypeMatch = true;
                dt.reset(new GDALExtendedDataType(
                    GDALExtendedDataType::Create(eDataType)));
                return true;
            }
            else if (GetCompoundDataType(gid, nVarType, dt,
                                         bPerfectDataTypeMatch))
            {
                return true;
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported netCDF compound data type encountered.");
                return false;
            }
        }
        else if (eClass == NC_ENUM)
        {
            nVarType = nBaseType;
        }
        else if (eClass == NC_VLEN)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VLen data type not supported");
            return false;
        }
        else if (eClass == NC_OPAQUE)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Opaque data type not supported");
            return false;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported  netCDF data type encountered.");
            return false;
        }
    }

    if (nVarType == NC_STRING)
    {
        bPerfectDataTypeMatch = true;
        dt.reset(
            new GDALExtendedDataType(GDALExtendedDataType::CreateString()));
        return true;
    }
    else
    {
        if (nVarType == NC_BYTE)
        {
            char *pszTemp = nullptr;
            bool bSignedData = true;
            if (varid >= 0 &&
                NCDFGetAttr(gid, varid, "_Unsigned", &pszTemp) == CE_None)
            {
                if (EQUAL(pszTemp, "true"))
                    bSignedData = false;
                else if (EQUAL(pszTemp, "false"))
                    bSignedData = true;
                CPLFree(pszTemp);
            }
            if (!bSignedData)
            {
                eDataType = GDT_Byte;
                bPerfectDataTypeMatch = true;
            }
            else
            {
                eDataType = GDT_Int8;
                bPerfectDataTypeMatch = true;
            }
        }
        else if (nVarType == NC_CHAR)
        {
            // Not sure of this
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Byte;
        }
        else if (nVarType == NC_SHORT)
        {
            bPerfectDataTypeMatch = true;
            char *pszTemp = nullptr;
            bool bSignedData = true;
            if (varid >= 0 &&
                NCDFGetAttr(gid, varid, "_Unsigned", &pszTemp) == CE_None)
            {
                if (EQUAL(pszTemp, "true"))
                    bSignedData = false;
                else if (EQUAL(pszTemp, "false"))
                    bSignedData = true;
                CPLFree(pszTemp);
            }
            if (!bSignedData)
            {
                eDataType = GDT_UInt16;
            }
            else
            {
                eDataType = GDT_Int16;
            }
        }
        else if (nVarType == NC_INT)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Int32;
        }
        else if (nVarType == NC_FLOAT)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Float32;
        }
        else if (nVarType == NC_DOUBLE)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Float64;
        }
        else if (nVarType == NC_UBYTE)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Byte;
        }
        else if (nVarType == NC_USHORT)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_UInt16;
        }
        else if (nVarType == NC_UINT)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_UInt32;
        }
        else if (nVarType == NC_INT64)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Int64;
        }
        else if (nVarType == NC_UINT64)
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_UInt64;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported netCDF data type encountered.");
            return false;
        }
    }
    dt.reset(new GDALExtendedDataType(GDALExtendedDataType::Create(eDataType)));
    return true;
}

/************************************************************************/
/*                             GetDataType()                            */
/************************************************************************/

const GDALExtendedDataType &netCDFVariable::GetDataType() const
{
    if (m_dt)
        return *m_dt;
    CPLMutexHolderD(&hNCMutex);

    if (m_nDims == 2 && m_nVarType == NC_CHAR && m_nTextLength > 0)
    {
        m_bPerfectDataTypeMatch = true;
        m_dt.reset(new GDALExtendedDataType(
            GDALExtendedDataType::CreateString(m_nTextLength)));
    }
    else
    {
        m_dt.reset(new GDALExtendedDataType(
            GDALExtendedDataType::Create(GDT_Unknown)));

        BuildDataType(m_gid, m_varid, m_nVarType, m_dt,
                      m_bPerfectDataTypeMatch);
    }
    return *m_dt;
}

/************************************************************************/
/*                              SetUnit()                               */
/************************************************************************/

bool netCDFVariable::SetUnit(const std::string &osUnit)
{
    if (osUnit.empty())
    {
        nc_del_att(m_gid, m_varid, CF_UNITS);
        return true;
    }
    auto poUnits(GetAttribute(CF_UNITS));
    if (!poUnits)
    {
        poUnits = CreateAttribute(
            CF_UNITS, {}, GDALExtendedDataType::CreateString(), nullptr);
        if (!poUnits)
            return false;
    }
    return poUnits->Write(osUnit.c_str());
}

/************************************************************************/
/*                            GetSpatialRef()                           */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> netCDFVariable::GetSpatialRef() const
{
    if (m_bSRSRead)
        return m_poSRS;

    m_bSRSRead = true;
    netCDFDataset poDS;
    poDS.ReadAttributes(m_gid, m_varid);
    int iDimX = 0;
    int iDimY = 0;
    int iCount = 1;
    for (const auto &poDim : GetDimensions())
    {
        if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
            iDimX = iCount;
        else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
            iDimY = iCount;
        poDS.papszDimName.AddString(poDim->GetName().c_str());
        iCount++;
    }
    if ((iDimX == 0 || iDimY == 0) && GetDimensionCount() >= 2)
    {
        iDimX = static_cast<int>(GetDimensionCount());
        iDimY = iDimX - 1;
    }
    poDS.SetProjectionFromVar(m_gid, m_varid, true);
    auto poSRS = poDS.GetSpatialRef();
    if (poSRS)
    {
        m_poSRS.reset(poSRS->Clone());
        if (iDimX > 0 && iDimY > 0)
        {
            if (m_poSRS->GetDataAxisToSRSAxisMapping() ==
                std::vector<int>{2, 1})
                m_poSRS->SetDataAxisToSRSAxisMapping({iDimY, iDimX});
            else
                m_poSRS->SetDataAxisToSRSAxisMapping({iDimX, iDimY});
        }
    }

    return m_poSRS;
}

/************************************************************************/
/*                            SetSpatialRef()                           */
/************************************************************************/

static void WriteDimAttr(std::shared_ptr<GDALMDArray> &poVar,
                         const char *pszAttrName, const char *pszAttrValue)
{
    auto poAttr = poVar->GetAttribute(pszAttrName);
    if (poAttr)
    {
        const char *pszVal = poAttr->ReadAsString();
        if (pszVal && !EQUAL(pszVal, pszAttrValue))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Variable %s has a %s which is %s and not %s",
                     poVar->GetName().c_str(), pszAttrName, pszVal,
                     pszAttrValue);
        }
    }
    else
    {
        poAttr = poVar->CreateAttribute(
            pszAttrName, {}, GDALExtendedDataType::CreateString(), nullptr);
        if (poAttr)
            poAttr->Write(pszAttrValue);
    }
}

static void WriteDimAttrs(const std::shared_ptr<GDALDimension> &dim,
                          const char *pszStandardName, const char *pszLongName,
                          const char *pszUnits)
{
    auto poVar = dim->GetIndexingVariable();
    if (poVar)
    {
        WriteDimAttr(poVar, CF_STD_NAME, pszStandardName);
        WriteDimAttr(poVar, CF_LNG_NAME, pszLongName);
        WriteDimAttr(poVar, CF_UNITS, pszUnits);
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Dimension %s lacks a indexing variable",
                 dim->GetName().c_str());
    }
}

bool netCDFVariable::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    m_bSRSRead = false;
    m_poSRS.reset();

    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    if (poSRS == nullptr)
    {
        nc_del_att(m_gid, m_varid, CF_GRD_MAPPING);
        return true;
    }

    char *pszCFProjection = nullptr;
    int nSRSVarId =
        NCDFWriteSRSVariable(m_gid, poSRS, &pszCFProjection, m_bWriteGDALTags);
    if (nSRSVarId < 0 || pszCFProjection == nullptr)
        return false;

    NCDF_ERR(nc_put_att_text(m_gid, m_varid, CF_GRD_MAPPING,
                             strlen(pszCFProjection), pszCFProjection));
    CPLFree(pszCFProjection);

    auto apoDims = GetDimensions();
    if (poSRS->IsProjected())
    {
        bool bWriteX = false;
        bool bWriteY = false;
        const std::string osUnits = NCDFGetProjectedCFUnit(poSRS);
        for (const auto &poDim : apoDims)
        {
            const char *pszStandardName = nullptr;
            const char *pszLongName = nullptr;
            if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X ||
                EQUAL(poDim->GetName().c_str(), CF_PROJ_X_VAR_NAME))
            {
                pszStandardName = CF_PROJ_X_COORD;
                pszLongName = CF_PROJ_X_COORD_LONG_NAME;
                bWriteX = true;
            }
            else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y ||
                     EQUAL(poDim->GetName().c_str(), CF_PROJ_Y_VAR_NAME))
            {
                pszStandardName = CF_PROJ_Y_COORD;
                pszLongName = CF_PROJ_Y_COORD_LONG_NAME;
                bWriteY = true;
            }
            if (pszStandardName && pszLongName)
            {
                WriteDimAttrs(poDim, pszStandardName, pszLongName,
                              osUnits.c_str());
            }
        }
        if (!bWriteX && !bWriteY && apoDims.size() >= 2 &&
            apoDims[apoDims.size() - 2]->GetType().empty() &&
            apoDims[apoDims.size() - 1]->GetType().empty() &&
            apoDims[apoDims.size() - 2]->GetIndexingVariable() &&
            apoDims[apoDims.size() - 1]->GetIndexingVariable())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Dimensions of variable %s have no type declared. "
                     "Assuming the last one is X, and the preceding one Y",
                     GetName().c_str());
            WriteDimAttrs(apoDims[apoDims.size() - 1], CF_PROJ_X_COORD,
                          CF_PROJ_X_COORD_LONG_NAME, osUnits.c_str());
            WriteDimAttrs(apoDims[apoDims.size() - 2], CF_PROJ_Y_COORD,
                          CF_PROJ_Y_COORD_LONG_NAME, osUnits.c_str());
        }
    }
    else if (poSRS->IsGeographic())
    {
        bool bWriteX = false;
        bool bWriteY = false;
        for (const auto &poDim : apoDims)
        {
            const char *pszStandardName = nullptr;
            const char *pszLongName = nullptr;
            const char *pszUnits = "";
            if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X ||
                EQUAL(poDim->GetName().c_str(), CF_LONGITUDE_VAR_NAME))
            {
                pszStandardName = CF_LONGITUDE_STD_NAME;
                pszLongName = CF_LONGITUDE_LNG_NAME;
                pszUnits = CF_DEGREES_EAST;
                bWriteX = true;
            }
            else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y ||
                     EQUAL(poDim->GetName().c_str(), CF_LATITUDE_VAR_NAME))
            {
                pszStandardName = CF_LATITUDE_STD_NAME;
                pszLongName = CF_LATITUDE_LNG_NAME;
                pszUnits = CF_DEGREES_NORTH;
                bWriteY = true;
            }
            if (pszStandardName && pszLongName)
            {
                WriteDimAttrs(poDim, pszStandardName, pszLongName, pszUnits);
            }
        }
        if (!bWriteX && !bWriteY && apoDims.size() >= 2 &&
            apoDims[apoDims.size() - 2]->GetType().empty() &&
            apoDims[apoDims.size() - 1]->GetType().empty() &&
            apoDims[apoDims.size() - 2]->GetIndexingVariable() &&
            apoDims[apoDims.size() - 1]->GetIndexingVariable())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Dimensions of variable %s have no type declared. "
                     "Assuming the last one is longitude, "
                     "and the preceding one latitude",
                     GetName().c_str());
            WriteDimAttrs(apoDims[apoDims.size() - 1], CF_LONGITUDE_STD_NAME,
                          CF_LONGITUDE_LNG_NAME, CF_DEGREES_EAST);
            WriteDimAttrs(apoDims[apoDims.size() - 2], CF_LATITUDE_STD_NAME,
                          CF_LATITUDE_LNG_NAME, CF_DEGREES_NORTH);
        }
    }

    return true;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

bool netCDFVariable::SetStatistics(bool bApproxStats, double dfMin,
                                   double dfMax, double dfMean, double dfStdDev,
                                   GUInt64 nValidCount,
                                   CSLConstList papszOptions)
{
    if (!bApproxStats && !m_poShared->IsReadOnly() &&
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "UPDATE_METADATA", "NO")))
    {
        auto poAttr = GetAttribute("actual_range");
        if (!poAttr)
        {
            poAttr =
                CreateAttribute("actual_range", {2}, GetDataType(), nullptr);
        }
        if (poAttr)
        {
            std::vector<GUInt64> startIdx = {0};
            std::vector<size_t> count = {2};
            std::vector<double> values = {dfMin, dfMax};
            poAttr->Write(startIdx.data(), count.data(), nullptr, nullptr,
                          GDALExtendedDataType::Create(GDT_Float64),
                          values.data(), nullptr, 0);
        }
    }
    return GDALPamMDArray::SetStatistics(bApproxStats, dfMin, dfMax, dfMean,
                                         dfStdDev, nValidCount, papszOptions);
}

/************************************************************************/
/*                             GetNCTypeSize()                          */
/************************************************************************/

static size_t GetNCTypeSize(const GDALExtendedDataType &dt,
                            bool bPerfectDataTypeMatch, int nAttType)
{
    auto nElementSize = dt.GetSize();
    if (!bPerfectDataTypeMatch)
    {
        if (nAttType == NC_BYTE)
        {
            CPLAssert(dt.GetNumericDataType() == GDT_Int16);
            nElementSize = sizeof(signed char);
        }
        else if (nAttType == NC_INT64)
        {
            CPLAssert(dt.GetNumericDataType() == GDT_Float64);
            nElementSize = sizeof(GInt64);
        }
        else if (nAttType == NC_UINT64)
        {
            CPLAssert(dt.GetNumericDataType() == GDT_Float64);
            nElementSize = sizeof(GUInt64);
        }
        else
        {
            CPLAssert(false);
        }
    }
    return nElementSize;
}

/************************************************************************/
/*                   ConvertNCStringsToCPLStrings()                     */
/************************************************************************/

static void ConvertNCStringsToCPLStrings(GByte *pBuffer,
                                         const GDALExtendedDataType &dt)
{
    switch (dt.GetClass())
    {
        case GEDTC_STRING:
        {
            char *pszStr;
            // cppcheck-suppress pointerSize
            memcpy(&pszStr, pBuffer, sizeof(char *));
            if (pszStr)
            {
                char *pszNewStr = VSIStrdup(pszStr);
                nc_free_string(1, &pszStr);
                // cppcheck-suppress pointerSize
                memcpy(pBuffer, &pszNewStr, sizeof(char *));
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            break;
        }

        case GEDTC_COMPOUND:
        {
            const auto &comps = dt.GetComponents();
            for (const auto &comp : comps)
            {
                ConvertNCStringsToCPLStrings(pBuffer + comp->GetOffset(),
                                             comp->GetType());
            }
            break;
        }
    }
}

/************************************************************************/
/*                            FreeNCStrings()                           */
/************************************************************************/

static void FreeNCStrings(GByte *pBuffer, const GDALExtendedDataType &dt)
{
    switch (dt.GetClass())
    {
        case GEDTC_STRING:
        {
            char *pszStr;
            // cppcheck-suppress pointerSize
            memcpy(&pszStr, pBuffer, sizeof(char *));
            if (pszStr)
            {
                nc_free_string(1, &pszStr);
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            break;
        }

        case GEDTC_COMPOUND:
        {
            const auto &comps = dt.GetComponents();
            for (const auto &comp : comps)
            {
                FreeNCStrings(pBuffer + comp->GetOffset(), comp->GetType());
            }
            break;
        }
    }
}

/************************************************************************/
/*                          IReadWriteGeneric()                         */
/************************************************************************/

namespace
{
template <typename T> struct GetGByteType
{
};

template <> struct GetGByteType<void *>
{
    typedef GByte *type;
};

template <> struct GetGByteType<const void *>
{
    typedef const GByte *type;
};
}  // namespace

template <typename BufferType, typename NCGetPutVar1FuncType,
          typename ReadOrWriteOneElementType>
bool netCDFVariable::IReadWriteGeneric(
    const size_t *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    BufferType buffer, NCGetPutVar1FuncType NCGetPutVar1Func,
    ReadOrWriteOneElementType ReadOrWriteOneElement) const
{
    CPLAssert(m_nDims > 0);
    std::vector<size_t> array_idx(m_nDims);
    std::vector<size_t> stack_count_iters(m_nDims - 1);
    typedef typename GetGByteType<BufferType>::type GBytePtrType;
    std::vector<GBytePtrType> stack_ptr(m_nDims);
    std::vector<GPtrDiff_t> ptr_inc;
    ptr_inc.reserve(m_nDims);
    const auto &eArrayEDT = GetDataType();
    const bool bSameDT = m_bPerfectDataTypeMatch && eArrayEDT == bufferDataType;
    const auto nBufferDTSize = bufferDataType.GetSize();
    for (int i = 0; i < m_nDims; i++)
    {
        ptr_inc.push_back(bufferStride[i] * nBufferDTSize);
    }
    const auto nDimsMinus1 = m_nDims - 1;
    stack_ptr[0] = static_cast<GBytePtrType>(buffer);

    auto lambdaLastDim = [&](GBytePtrType ptr)
    {
        array_idx[nDimsMinus1] = arrayStartIdx[nDimsMinus1];
        size_t nIters = count[nDimsMinus1];
        while (true)
        {
            if (bSameDT)
            {
                int ret =
                    NCGetPutVar1Func(m_gid, m_varid, array_idx.data(), ptr);
                NCDF_ERR(ret);
                if (ret != NC_NOERR)
                    return false;
            }
            else
            {
                if (!(this->*ReadOrWriteOneElement)(eArrayEDT, bufferDataType,
                                                    array_idx.data(), ptr))
                    return false;
            }
            if ((--nIters) == 0)
                break;
            ptr += ptr_inc[nDimsMinus1];
            // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
            // thus automatic conversion from negative to big unsigned might
            // occur
            array_idx[nDimsMinus1] = CPLUnsanitizedAdd<size_t>(
                array_idx[nDimsMinus1],
                static_cast<GPtrDiff_t>(arrayStep[nDimsMinus1]));
        }
        return true;
    };

    if (m_nDims == 1)
    {
        return lambdaLastDim(stack_ptr[0]);
    }
    else if (m_nDims == 2)
    {
        auto nIters = count[0];
        array_idx[0] = arrayStartIdx[0];
        while (true)
        {
            if (!lambdaLastDim(stack_ptr[0]))
                return false;
            if ((--nIters) == 0)
                break;
            stack_ptr[0] += ptr_inc[0];
            // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
            // thus automatic conversion from negative to big unsigned might
            // occur
            array_idx[0] = CPLUnsanitizedAdd<size_t>(
                array_idx[0], static_cast<GPtrDiff_t>(arrayStep[0]));
        }
    }
    else if (m_nDims == 3)
    {
        stack_count_iters[0] = count[0];
        array_idx[0] = arrayStartIdx[0];
        while (true)
        {
            auto nIters = count[1];
            array_idx[1] = arrayStartIdx[1];
            stack_ptr[1] = stack_ptr[0];
            while (true)
            {
                if (!lambdaLastDim(stack_ptr[1]))
                    return false;
                if ((--nIters) == 0)
                    break;
                stack_ptr[1] += ptr_inc[1];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative,
                // and thus automatic conversion from negative to big unsigned
                // might occur
                array_idx[1] = CPLUnsanitizedAdd<size_t>(
                    array_idx[1], static_cast<GPtrDiff_t>(arrayStep[1]));
            }
            if ((--stack_count_iters[0]) == 0)
                break;
            stack_ptr[0] += ptr_inc[0];
            // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
            // thus automatic conversion from negative to big unsigned might
            // occur
            array_idx[0] = CPLUnsanitizedAdd<size_t>(
                array_idx[0], static_cast<GPtrDiff_t>(arrayStep[0]));
        }
    }
    else
    {
        // Implementation valid for nDims >= 3

        int dimIdx = 0;
        // Non-recursive implementation. Hence the gotos
        // It might be possible to rewrite this without gotos, but I find they
        // make it clearer to understand the recursive nature of the code
    lbl_start:
        if (dimIdx == nDimsMinus1 - 1)
        {
            array_idx[dimIdx] = arrayStartIdx[dimIdx];
            auto nIters = count[dimIdx];
            while (true)
            {
                if (!(lambdaLastDim(stack_ptr[dimIdx])))
                    return false;
                if ((--nIters) == 0)
                    break;
                stack_ptr[dimIdx] += ptr_inc[dimIdx];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative,
                // and thus automatic conversion from negative to big unsigned
                // might occur
                array_idx[dimIdx] = CPLUnsanitizedAdd<size_t>(
                    array_idx[dimIdx],
                    static_cast<GPtrDiff_t>(arrayStep[dimIdx]));
            }
            // If there was a test if( dimIdx > 0 ), that would be valid for
            // nDims == 2
            goto lbl_return_to_caller;
        }
        else
        {
            array_idx[dimIdx] = arrayStartIdx[dimIdx];
            stack_count_iters[dimIdx] = count[dimIdx];
            while (true)
            {
                // Simulate a recursive call to the next dimension
                // Implicitly save back count and ptr
                dimIdx++;
                stack_ptr[dimIdx] = stack_ptr[dimIdx - 1];
                goto lbl_start;
            lbl_return_to_caller:
                dimIdx--;
                if ((--stack_count_iters[dimIdx]) == 0)
                    break;
                stack_ptr[dimIdx] += ptr_inc[dimIdx];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative,
                // and thus automatic conversion from negative to big unsigned
                // might occur
                array_idx[dimIdx] = CPLUnsanitizedAdd<size_t>(
                    array_idx[dimIdx],
                    static_cast<GPtrDiff_t>(arrayStep[dimIdx]));
            }
            if (dimIdx > 0)
                goto lbl_return_to_caller;
        }
    }

    return true;
}

/************************************************************************/
/*                          CheckNumericDataType()                      */
/************************************************************************/

static bool CheckNumericDataType(const GDALExtendedDataType &dt)
{
    const auto klass = dt.GetClass();
    if (klass == GEDTC_NUMERIC)
        return dt.GetNumericDataType() != GDT_Unknown;
    if (klass == GEDTC_STRING)
        return false;
    CPLAssert(klass == GEDTC_COMPOUND);
    const auto &comps = dt.GetComponents();
    for (const auto &comp : comps)
    {
        if (!CheckNumericDataType(comp->GetType()))
            return false;
    }
    return true;
}

/************************************************************************/
/*                            IReadWrite()                              */
/************************************************************************/

template <typename BufferType, typename NCGetPutVar1FuncType,
          typename NCGetPutVaraFuncType, typename NCGetPutVarmFuncType,
          typename ReadOrWriteOneElementType>
bool netCDFVariable::IReadWrite(
    const bool bIsRead, const GUInt64 *arrayStartIdx, const size_t *count,
    const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
    const GDALExtendedDataType &bufferDataType, BufferType buffer,
    NCGetPutVar1FuncType NCGetPutVar1Func,
    NCGetPutVaraFuncType NCGetPutVaraFunc,
    NCGetPutVarmFuncType NCGetPutVarmFunc,
    ReadOrWriteOneElementType ReadOrWriteOneElement) const
{
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(false);

    const auto &eDT = GetDataType();
    std::vector<size_t> startp;
    startp.reserve(m_nDims);
    bool bUseSlowPath =
        !m_bPerfectDataTypeMatch &&
        !(bIsRead && bufferDataType.GetClass() == GEDTC_NUMERIC &&
          eDT.GetClass() == GEDTC_NUMERIC &&
          bufferDataType.GetSize() >= eDT.GetSize());
    for (int i = 0; i < m_nDims; i++)
    {
#if SIZEOF_VOIDP == 4
        if (arrayStartIdx[i] > std::numeric_limits<size_t>::max())
            return false;
#endif
        startp.push_back(static_cast<size_t>(arrayStartIdx[i]));

#if SIZEOF_VOIDP == 4
        if (arrayStep[i] < std::numeric_limits<ptrdiff_t>::min() ||
            arrayStep[i] > std::numeric_limits<ptrdiff_t>::max())
        {
            return false;
        }
#endif

        if (count[i] != 1 && arrayStep[i] <= 0)
            bUseSlowPath = true;  // netCDF rejects negative or NULL strides

        if (bufferStride[i] < 0)
            bUseSlowPath =
                true;  // and it seems to silently cast to size_t imapp
    }

    if (eDT.GetClass() == GEDTC_STRING &&
        bufferDataType.GetClass() == GEDTC_STRING && m_nVarType == NC_STRING)
    {
        if (m_nDims == 0)
        {
            return (this->*ReadOrWriteOneElement)(eDT, bufferDataType, nullptr,
                                                  buffer);
        }

        return IReadWriteGeneric(startp.data(), count, arrayStep, bufferStride,
                                 bufferDataType, buffer, NCGetPutVar1Func,
                                 ReadOrWriteOneElement);
    }

    if (!CheckNumericDataType(eDT))
        return false;
    if (!CheckNumericDataType(bufferDataType))
        return false;

    if (m_nDims == 0)
    {
        return (this->*ReadOrWriteOneElement)(eDT, bufferDataType, nullptr,
                                              buffer);
    }

    if (!bUseSlowPath &&
        ((GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()) ||
          bufferDataType.GetClass() == GEDTC_COMPOUND) &&
         bufferDataType == eDT))
    {
        // nc_get_varm() not supported for non-atomic types.
        ptrdiff_t nExpectedBufferStride = 1;
        for (int i = m_nDims; i != 0;)
        {
            --i;
            if (count[i] != 1 &&
                (arrayStep[i] != 1 || bufferStride[i] != nExpectedBufferStride))
            {
                bUseSlowPath = true;
                break;
            }
            nExpectedBufferStride *= count[i];
        }
        if (!bUseSlowPath)
        {
            int ret =
                NCGetPutVaraFunc(m_gid, m_varid, startp.data(), count, buffer);
            NCDF_ERR(ret);
            return ret == NC_NOERR;
        }
    }

    if (bUseSlowPath || bufferDataType.GetClass() == GEDTC_COMPOUND ||
        eDT.GetClass() == GEDTC_COMPOUND ||
        (!bIsRead &&
         bufferDataType.GetNumericDataType() != eDT.GetNumericDataType()) ||
        (bIsRead && bufferDataType.GetSize() < eDT.GetSize()))
    {
        return IReadWriteGeneric(startp.data(), count, arrayStep, bufferStride,
                                 bufferDataType, buffer, NCGetPutVar1Func,
                                 ReadOrWriteOneElement);
    }

    bUseSlowPath = false;
    ptrdiff_t nExpectedBufferStride = 1;
    for (int i = m_nDims; i != 0;)
    {
        --i;
        if (count[i] != 1 &&
            (arrayStep[i] != 1 || bufferStride[i] != nExpectedBufferStride))
        {
            bUseSlowPath = true;
            break;
        }
        nExpectedBufferStride *= count[i];
    }
    if (!bUseSlowPath)
    {
        // nc_get_varm() is terribly inefficient, so use nc_get_vara()
        // when possible.
        int ret =
            NCGetPutVaraFunc(m_gid, m_varid, startp.data(), count, buffer);
        if (ret != NC_NOERR)
        {
            NCDF_ERR(ret);
            return false;
        }
        if (bIsRead &&
            (!m_bPerfectDataTypeMatch ||
             bufferDataType.GetNumericDataType() != eDT.GetNumericDataType()))
        {
            // If the buffer data type is "larger" or of the same size as the
            // native data type, we can do a in-place conversion
            GByte *pabyBuffer =
                static_cast<GByte *>(const_cast<void *>(buffer));
            CPLAssert(bufferDataType.GetSize() >= eDT.GetSize());
            const auto nDTSize = eDT.GetSize();
            const auto nBufferDTSize = bufferDataType.GetSize();
            if (!m_bPerfectDataTypeMatch &&
                (m_nVarType == NC_CHAR || m_nVarType == NC_BYTE))
            {
                // native NC type translates into GDAL data type of larger size
                for (ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i)
                {
                    GByte abySrc[sizeof(
                        double)];  // 2 is enough here, but sizeof(double) make
                                   // MSVC happy
                    abySrc[0] = *(pabyBuffer + i);
                    ConvertNCToGDAL(&abySrc[0]);
                    GDALExtendedDataType::CopyValue(
                        &abySrc[0], eDT, pabyBuffer + i * nBufferDTSize,
                        bufferDataType);
                }
            }
            else if (!m_bPerfectDataTypeMatch)
            {
                // native NC type translates into GDAL data type of same size
                CPLAssert(m_nVarType == NC_INT64 || m_nVarType == NC_UINT64);
                for (ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i)
                {
                    ConvertNCToGDAL(pabyBuffer + i * nDTSize);
                    GDALExtendedDataType::CopyValue(
                        pabyBuffer + i * nDTSize, eDT,
                        pabyBuffer + i * nBufferDTSize, bufferDataType);
                }
            }
            else
            {
                for (ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i)
                {
                    GDALExtendedDataType::CopyValue(
                        pabyBuffer + i * nDTSize, eDT,
                        pabyBuffer + i * nBufferDTSize, bufferDataType);
                }
            }
        }
        return true;
    }
    else
    {
        if (bufferDataType.GetNumericDataType() != eDT.GetNumericDataType())
        {
            return IReadWriteGeneric(startp.data(), count, arrayStep,
                                     bufferStride, bufferDataType, buffer,
                                     NCGetPutVar1Func, ReadOrWriteOneElement);
        }
        std::vector<ptrdiff_t> stridep;
        stridep.reserve(m_nDims);
        std::vector<ptrdiff_t> imapp;
        imapp.reserve(m_nDims);
        for (int i = 0; i < m_nDims; i++)
        {
            stridep.push_back(
                static_cast<ptrdiff_t>(count[i] == 1 ? 1 : arrayStep[i]));
            imapp.push_back(static_cast<ptrdiff_t>(bufferStride[i]));
        }

        if (!m_poShared->GetImappIsInElements())
        {
            const size_t nMul =
                GetNCTypeSize(eDT, m_bPerfectDataTypeMatch, m_nVarType);
            for (int i = 0; i < m_nDims; ++i)
            {
                imapp[i] = static_cast<ptrdiff_t>(imapp[i] * nMul);
            }
        }
        int ret = NCGetPutVarmFunc(m_gid, m_varid, startp.data(), count,
                                   stridep.data(), imapp.data(), buffer);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }
}

/************************************************************************/
/*                          ConvertNCToGDAL()                           */
/************************************************************************/

void netCDFVariable::ConvertNCToGDAL(GByte *buffer) const
{
    if (!m_bPerfectDataTypeMatch)
    {
        if (m_nVarType == NC_CHAR || m_nVarType == NC_BYTE)
        {
            short s = reinterpret_cast<signed char *>(buffer)[0];
            memcpy(buffer, &s, sizeof(s));
        }
        else if (m_nVarType == NC_INT64)
        {
            double v =
                static_cast<double>(reinterpret_cast<GInt64 *>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
        else if (m_nVarType == NC_UINT64)
        {
            double v =
                static_cast<double>(reinterpret_cast<GUInt64 *>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
    }
}

/************************************************************************/
/*                           ReadOneElement()                           */
/************************************************************************/

bool netCDFVariable::ReadOneElement(const GDALExtendedDataType &src_datatype,
                                    const GDALExtendedDataType &bufferDataType,
                                    const size_t *array_idx,
                                    void *pDstBuffer) const
{
    if (src_datatype.GetClass() == GEDTC_STRING)
    {
        char *pszStr = nullptr;
        int ret = nc_get_var1_string(m_gid, m_varid, array_idx, &pszStr);
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return false;
        GDALExtendedDataType::CopyValue(&pszStr, src_datatype, pDstBuffer,
                                        bufferDataType);
        nc_free_string(1, &pszStr);
        return true;
    }

    std::vector<GByte> abySrc(std::max(
        src_datatype.GetSize(),
        GetNCTypeSize(src_datatype, m_bPerfectDataTypeMatch, m_nVarType)));

    int ret = nc_get_var1(m_gid, m_varid, array_idx, &abySrc[0]);
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    ConvertNCToGDAL(&abySrc[0]);

    GDALExtendedDataType::CopyValue(&abySrc[0], src_datatype, pDstBuffer,
                                    bufferDataType);
    return true;
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool netCDFVariable::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                           const GInt64 *arrayStep,
                           const GPtrDiff_t *bufferStride,
                           const GDALExtendedDataType &bufferDataType,
                           void *pDstBuffer) const
{
    if (m_nDims == 2 && m_nVarType == NC_CHAR && GetDimensions().size() == 1)
    {
        CPLMutexHolderD(&hNCMutex);
        m_poShared->SetDefineMode(false);

        if (bufferDataType.GetClass() != GEDTC_STRING)
            return false;
        GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
        size_t array_idx[2] = {static_cast<size_t>(arrayStartIdx[0]), 0};
        size_t array_count[2] = {1, m_nTextLength};
        std::string osTmp(m_nTextLength, 0);
        const char *pszTmp = osTmp.c_str();
        for (size_t i = 0; i < count[0]; i++)
        {
            int ret =
                nc_get_vara(m_gid, m_varid, array_idx, array_count, &osTmp[0]);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return false;
            // coverity[use_after_free]
            GDALExtendedDataType::CopyValue(&pszTmp, GetDataType(),
                                            pabyDstBuffer, GetDataType());
            array_idx[0] = static_cast<size_t>(array_idx[0] + arrayStep[0]);
            pabyDstBuffer += bufferStride[0] * sizeof(char *);
        }
        return true;
    }

    if (m_poCachedArray)
    {
        const auto nDims = GetDimensionCount();
        std::vector<GUInt64> modifiedArrayStartIdx(nDims);
        bool canUseCache = true;
        for (size_t i = 0; i < nDims; i++)
        {
            if (arrayStartIdx[i] >= m_cachedArrayStartIdx[i] &&
                arrayStartIdx[i] + (count[i] - 1) * arrayStep[i] <=
                    m_cachedArrayStartIdx[i] + m_cachedCount[i] - 1)
            {
                modifiedArrayStartIdx[i] =
                    arrayStartIdx[i] - m_cachedArrayStartIdx[i];
            }
            else
            {
                canUseCache = false;
                break;
            }
        }
        if (canUseCache)
        {
            return m_poCachedArray->Read(modifiedArrayStartIdx.data(), count,
                                         arrayStep, bufferStride,
                                         bufferDataType, pDstBuffer);
        }
    }

    if (IsTransposedRequest(count, bufferStride))
    {
        return ReadForTransposedRequest(arrayStartIdx, count, arrayStep,
                                        bufferStride, bufferDataType,
                                        pDstBuffer);
    }

    return IReadWrite(true, arrayStartIdx, count, arrayStep, bufferStride,
                      bufferDataType, pDstBuffer, nc_get_var1, nc_get_vara,
                      nc_get_varm, &netCDFVariable::ReadOneElement);
}

/************************************************************************/
/*                             IAdviseRead()                            */
/************************************************************************/

bool netCDFVariable::IAdviseRead(const GUInt64 *arrayStartIdx,
                                 const size_t *count,
                                 CSLConstList /* papszOptions */) const
{
    const auto nDims = GetDimensionCount();
    if (nDims == 0)
        return true;
    const auto &eDT = GetDataType();
    if (eDT.GetClass() != GEDTC_NUMERIC)
        return false;

    auto poMemDriver = static_cast<GDALDriver *>(GDALGetDriverByName("MEM"));
    if (poMemDriver == nullptr)
        return false;

    m_poCachedArray.reset();

    size_t nElts = 1;
    for (size_t i = 0; i < nDims; i++)
        nElts *= count[i];

    void *pData = VSI_MALLOC2_VERBOSE(nElts, eDT.GetSize());
    if (pData == nullptr)
        return false;

    if (!Read(arrayStartIdx, count, nullptr, nullptr, eDT, pData))
    {
        VSIFree(pData);
        return false;
    }

    auto poDS = poMemDriver->CreateMultiDimensional("", nullptr, nullptr);
    auto poGroup = poDS->GetRootGroup();
    delete poDS;

    std::vector<std::shared_ptr<GDALDimension>> apoMemDims;
    const auto &poDims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
    {
        apoMemDims.emplace_back(
            poGroup->CreateDimension(poDims[i]->GetName(), std::string(),
                                     std::string(), count[i], nullptr));
    }
    m_poCachedArray =
        poGroup->CreateMDArray(GetName(), apoMemDims, eDT, nullptr);
    m_poCachedArray->Write(std::vector<GUInt64>(nDims).data(), count, nullptr,
                           nullptr, eDT, pData);
    m_cachedArrayStartIdx.resize(nDims);
    memcpy(&m_cachedArrayStartIdx[0], arrayStartIdx, nDims * sizeof(GUInt64));
    m_cachedCount.resize(nDims);
    memcpy(&m_cachedCount[0], count, nDims * sizeof(size_t));
    VSIFree(pData);
    return true;
}

/************************************************************************/
/*                          ConvertGDALToNC()                           */
/************************************************************************/

void netCDFVariable::ConvertGDALToNC(GByte *buffer) const
{
    if (!m_bPerfectDataTypeMatch)
    {
        if (m_nVarType == NC_CHAR || m_nVarType == NC_BYTE)
        {
            const auto c =
                static_cast<signed char>(reinterpret_cast<short *>(buffer)[0]);
            memcpy(buffer, &c, sizeof(c));
        }
        else if (m_nVarType == NC_INT64)
        {
            const auto v =
                static_cast<GInt64>(reinterpret_cast<double *>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
        else if (m_nVarType == NC_UINT64)
        {
            const auto v =
                static_cast<GUInt64>(reinterpret_cast<double *>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
    }
}

/************************************************************************/
/*                          WriteOneElement()                           */
/************************************************************************/

bool netCDFVariable::WriteOneElement(const GDALExtendedDataType &dst_datatype,
                                     const GDALExtendedDataType &bufferDataType,
                                     const size_t *array_idx,
                                     const void *pSrcBuffer) const
{
    if (dst_datatype.GetClass() == GEDTC_STRING)
    {
        const char *pszStr = (static_cast<const char *const *>(pSrcBuffer))[0];
        int ret = nc_put_var1_string(m_gid, m_varid, array_idx, &pszStr);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }

    std::vector<GByte> abyTmp(dst_datatype.GetSize());
    GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &abyTmp[0],
                                    dst_datatype);

    ConvertGDALToNC(&abyTmp[0]);

    int ret = nc_put_var1(m_gid, m_varid, array_idx, &abyTmp[0]);
    NCDF_ERR(ret);
    return ret == NC_NOERR;
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

bool netCDFVariable::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            const void *pSrcBuffer)
{
    m_bHasWrittenData = true;

    m_poCachedArray.reset();

    if (m_nDims == 2 && m_nVarType == NC_CHAR && GetDimensions().size() == 1)
    {
        CPLMutexHolderD(&hNCMutex);
        m_poShared->SetDefineMode(false);

        if (bufferDataType.GetClass() != GEDTC_STRING)
            return false;
        const char *const *ppszSrcBuffer =
            static_cast<const char *const *>(pSrcBuffer);
        size_t array_idx[2] = {static_cast<size_t>(arrayStartIdx[0]), 0};
        size_t array_count[2] = {1, m_nTextLength};
        std::string osTmp(m_nTextLength, 0);
        for (size_t i = 0; i < count[0]; i++)
        {
            const char *pszStr = *ppszSrcBuffer;
            memset(&osTmp[0], 0, m_nTextLength);
            if (pszStr)
            {
                size_t nLen = strlen(pszStr);
                memcpy(&osTmp[0], pszStr, std::min(m_nTextLength, nLen));
            }
            int ret =
                nc_put_vara(m_gid, m_varid, array_idx, array_count, &osTmp[0]);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return false;
            array_idx[0] = static_cast<size_t>(array_idx[0] + arrayStep[0]);
            ppszSrcBuffer += bufferStride[0];
        }
        return true;
    }

    return IReadWrite(false, arrayStartIdx, count, arrayStep, bufferStride,
                      bufferDataType, pSrcBuffer, nc_put_var1, nc_put_vara,
                      nc_put_varm, &netCDFVariable::WriteOneElement);
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void *netCDFVariable::GetRawNoDataValue() const
{
    const auto &dt = GetDataType();
    if (dt.GetClass() != GEDTC_NUMERIC)
        return nullptr;

    if (m_bGetRawNoDataValueHasRun)
    {
        return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
    }

    m_bGetRawNoDataValueHasRun = true;

    const char *pszAttrName = _FillValue;
    auto poAttr = GetAttribute(pszAttrName);
    if (!poAttr)
    {
        pszAttrName = "missing_value";
        poAttr = GetAttribute(pszAttrName);
    }
    if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        auto oRawResult = poAttr->ReadAsRaw();
        if (oRawResult.data())
        {
            // Round-trip attribute value to target data type and back
            // to attribute data type to ensure there is no loss
            // Normally _FillValue data type should be the same
            // as the array one, but this is not always the case.
            // For example NASA GEDI L2B products have Float64
            // _FillValue for Float32 variables.
            m_abyNoData.resize(dt.GetSize());
            GDALExtendedDataType::CopyValue(oRawResult.data(),
                                            poAttr->GetDataType(),
                                            m_abyNoData.data(), dt);
            std::vector<GByte> abyTmp(poAttr->GetDataType().GetSize());
            GDALExtendedDataType::CopyValue(
                m_abyNoData.data(), dt, abyTmp.data(), poAttr->GetDataType());
            std::vector<GByte> abyOri;
            abyOri.assign(oRawResult.data(),
                          oRawResult.data() + oRawResult.size());
            if (abyOri == abyTmp)
                return m_abyNoData.data();
            m_abyNoData.clear();
            char *pszVal = nullptr;
            GDALExtendedDataType::CopyValue(
                oRawResult.data(), poAttr->GetDataType(), &pszVal,
                GDALExtendedDataType::CreateString());
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s attribute value (%s) is not in the range of the "
                     "variable data type",
                     pszAttrName, pszVal ? pszVal : "(null)");
            CPLFree(pszVal);
            return nullptr;
        }
    }
    else if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING)
    {
        const char *pszVal = poAttr->ReadAsString();
        if (pszVal)
        {
            // Round-trip attribute value to target data type and back
            // to attribute data type to ensure there is no loss
            m_abyNoData.resize(dt.GetSize());
            GDALExtendedDataType::CopyValue(&pszVal, poAttr->GetDataType(),
                                            m_abyNoData.data(), dt);
            char *pszTmpVal = nullptr;
            GDALExtendedDataType::CopyValue(m_abyNoData.data(), dt, &pszTmpVal,
                                            poAttr->GetDataType());
            if (pszTmpVal)
            {
                const bool bSame = strcmp(pszVal, pszTmpVal) == 0;
                CPLFree(pszTmpVal);
                if (bSame)
                    return m_abyNoData.data();
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s attribute value ('%s') is not in the range of the "
                         "variable data type",
                         pszAttrName, pszVal);
                m_abyNoData.clear();
                return nullptr;
            }
        }
    }

    if (m_bUseDefaultFillAsNoData && m_abyNoData.empty() &&
        (m_nVarType == NC_SHORT || m_nVarType == NC_USHORT ||
         m_nVarType == NC_INT || m_nVarType == NC_UINT ||
         m_nVarType == NC_FLOAT || m_nVarType == NC_DOUBLE))
    {
        bool bGotNoData = false;
        double dfNoData =
            NCDFGetDefaultNoDataValue(m_gid, m_varid, m_nVarType, bGotNoData);
        m_abyNoData.resize(dt.GetSize());
        GDALCopyWords(&dfNoData, GDT_Float64, 0, &m_abyNoData[0],
                      dt.GetNumericDataType(), 0, 1);
    }
    else if (m_bUseDefaultFillAsNoData && m_abyNoData.empty() &&
             m_nVarType == NC_INT64)
    {
        bool bGotNoData = false;
        const auto nNoData =
            NCDFGetDefaultNoDataValueAsInt64(m_gid, m_varid, bGotNoData);
        m_abyNoData.resize(dt.GetSize());
        memcpy(&m_abyNoData[0], &nNoData, sizeof(nNoData));
    }
    else if (m_bUseDefaultFillAsNoData && m_abyNoData.empty() &&
             m_nVarType == NC_UINT64)
    {
        bool bGotNoData = false;
        const auto nNoData =
            NCDFGetDefaultNoDataValueAsUInt64(m_gid, m_varid, bGotNoData);
        m_abyNoData.resize(dt.GetSize());
        memcpy(&m_abyNoData[0], &nNoData, sizeof(nNoData));
    }

    return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
}

/************************************************************************/
/*                          SetRawNoDataValue()                         */
/************************************************************************/

bool netCDFVariable::SetRawNoDataValue(const void *pNoData)
{
    GetDataType();
    if (m_nVarType == NC_STRING)
        return false;

    m_bGetRawNoDataValueHasRun = false;
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int ret;
    if (pNoData == nullptr)
    {
        m_abyNoData.clear();
        nc_type atttype = NC_NAT;
        size_t attlen = 0;
        if (nc_inq_att(m_gid, m_varid, _FillValue, &atttype, &attlen) ==
            NC_NOERR)
            ret = nc_del_att(m_gid, m_varid, _FillValue);
        else
            ret = NC_NOERR;
        if (nc_inq_att(m_gid, m_varid, "missing_value", &atttype, &attlen) ==
            NC_NOERR)
        {
            int ret2 = nc_del_att(m_gid, m_varid, "missing_value");
            if (ret2 != NC_NOERR)
                ret = ret2;
        }
    }
    else
    {
        const auto nSize = GetDataType().GetSize();
        m_abyNoData.resize(nSize);
        memcpy(&m_abyNoData[0], pNoData, nSize);

        std::vector<GByte> abyTmp(nSize);
        memcpy(&abyTmp[0], pNoData, nSize);
        ConvertGDALToNC(&abyTmp[0]);

        if (!m_bHasWrittenData)
        {
            ret = nc_def_var_fill(m_gid, m_varid, NC_FILL, &abyTmp[0]);
            NCDF_ERR(ret);
        }

        nc_type atttype = NC_NAT;
        size_t attlen = 0;
        if (nc_inq_att(m_gid, m_varid, "missing_value", &atttype, &attlen) ==
            NC_NOERR)
        {
            if (nc_inq_att(m_gid, m_varid, _FillValue, &atttype, &attlen) ==
                NC_NOERR)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Cannot change nodata when missing_value and "
                         "_FillValue both exist");
                return false;
            }
            ret = nc_put_att(m_gid, m_varid, "missing_value", m_nVarType, 1,
                             &abyTmp[0]);
        }
        else
        {
            ret = nc_put_att(m_gid, m_varid, _FillValue, m_nVarType, 1,
                             &abyTmp[0]);
        }
    }
    NCDF_ERR(ret);
    if (ret == NC_NOERR)
        m_bGetRawNoDataValueHasRun = true;
    return ret == NC_NOERR;
}

/************************************************************************/
/*                               SetScale()                             */
/************************************************************************/

bool netCDFVariable::SetScale(double dfScale, GDALDataType eStorageType)
{
    auto poAttr = GetAttribute(CF_SCALE_FACTOR);
    if (!poAttr)
    {
        poAttr = CreateAttribute(
            CF_SCALE_FACTOR, {},
            GDALExtendedDataType::Create(
                eStorageType == GDT_Unknown ? GDT_Float64 : eStorageType),
            nullptr);
    }
    if (!poAttr)
        return false;
    return poAttr->Write(dfScale);
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

bool netCDFVariable::SetOffset(double dfOffset, GDALDataType eStorageType)
{
    auto poAttr = GetAttribute(CF_ADD_OFFSET);
    if (!poAttr)
    {
        poAttr = CreateAttribute(
            CF_ADD_OFFSET, {},
            GDALExtendedDataType::Create(
                eStorageType == GDT_Unknown ? GDT_Float64 : eStorageType),
            nullptr);
    }
    if (!poAttr)
        return false;
    return poAttr->Write(dfOffset);
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

double netCDFVariable::GetScale(bool *pbHasScale,
                                GDALDataType *peStorageType) const
{
    auto poAttr = GetAttribute(CF_SCALE_FACTOR);
    if (!poAttr || poAttr->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        if (pbHasScale)
            *pbHasScale = false;
        return 1.0;
    }
    if (pbHasScale)
        *pbHasScale = true;
    if (peStorageType)
        *peStorageType = poAttr->GetDataType().GetNumericDataType();
    return poAttr->ReadAsDouble();
}

/************************************************************************/
/*                               GetOffset()                            */
/************************************************************************/

double netCDFVariable::GetOffset(bool *pbHasOffset,
                                 GDALDataType *peStorageType) const
{
    auto poAttr = GetAttribute(CF_ADD_OFFSET);
    if (!poAttr || poAttr->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        if (pbHasOffset)
            *pbHasOffset = false;
        return 0.0;
    }
    if (pbHasOffset)
        *pbHasOffset = true;
    if (peStorageType)
        *peStorageType = poAttr->GetDataType().GetNumericDataType();
    return poAttr->ReadAsDouble();
}

/************************************************************************/
/*                           GetBlockSize()                             */
/************************************************************************/

std::vector<GUInt64> netCDFVariable::GetBlockSize() const
{
    const auto nDimCount = GetDimensionCount();
    std::vector<GUInt64> res(nDimCount);
    if (res.empty())
        return res;
    int nStorageType = 0;
    // We add 1 to the dimension count, for 2D char variables that we
    // expose as a 1D variable.
    std::vector<size_t> anTemp(1 + nDimCount);
    CPLMutexHolderD(&hNCMutex);
    nc_inq_var_chunking(m_gid, m_varid, &nStorageType, &anTemp[0]);
    if (nStorageType == NC_CHUNKED)
    {
        for (size_t i = 0; i < res.size(); ++i)
            res[i] = anTemp[i];
    }
    return res;
}

/************************************************************************/
/*                         GetAttribute()                               */
/************************************************************************/

std::shared_ptr<GDALAttribute>
netCDFVariable::GetAttribute(const std::string &osName) const
{
    CPLMutexHolderD(&hNCMutex);
    int nAttId = -1;
    if (nc_inq_attid(m_gid, m_varid, osName.c_str(), &nAttId) != NC_NOERR)
        return nullptr;
    return netCDFAttribute::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFVariable>(m_pSelf.lock()),
        m_gid, m_varid, osName);
}

/************************************************************************/
/*                         GetAttributes()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
netCDFVariable::GetAttributes(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    std::vector<std::shared_ptr<GDALAttribute>> res;
    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(m_gid, m_varid, &nbAttr));
    res.reserve(nbAttr);
    const bool bShowAll =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    for (int i = 0; i < nbAttr; i++)
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(m_gid, m_varid, i, szAttrName));
        if (bShowAll || (!EQUAL(szAttrName, _FillValue) &&
                         !EQUAL(szAttrName, "missing_value") &&
                         !EQUAL(szAttrName, CF_UNITS) &&
                         !EQUAL(szAttrName, CF_SCALE_FACTOR) &&
                         !EQUAL(szAttrName, CF_ADD_OFFSET) &&
                         !EQUAL(szAttrName, CF_GRD_MAPPING) &&
                         !(EQUAL(szAttrName, "_Unsigned") &&
                           (m_nVarType == NC_BYTE || m_nVarType == NC_SHORT))))
        {
            res.emplace_back(netCDFAttribute::Create(
                m_poShared,
                std::dynamic_pointer_cast<netCDFVariable>(m_pSelf.lock()),
                m_gid, m_varid, szAttrName));
        }
    }
    return res;
}

/************************************************************************/
/*                          CreateAttribute()                           */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFVariable::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    return netCDFAttribute::Create(
        m_poShared, std::dynamic_pointer_cast<netCDFVariable>(m_pSelf.lock()),
        m_gid, m_varid, osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                         DeleteAttribute()                            */
/************************************************************************/

bool netCDFVariable::DeleteAttribute(const std::string &osName,
                                     CSLConstList /*papszOptions*/)
{
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret = nc_del_att(m_gid, m_varid, osName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    auto it = m_oMapAttributes.find(osName);
    if (it != m_oMapAttributes.end())
    {
        it->second->Deleted();
        m_oMapAttributes.erase(it);
    }

    return true;
}

/************************************************************************/
/*                      GetCoordinateVariables()                        */
/************************************************************************/

std::vector<std::shared_ptr<GDALMDArray>>
netCDFVariable::GetCoordinateVariables() const
{
    std::vector<std::shared_ptr<GDALMDArray>> ret;

    const auto poCoordinates = GetAttribute("coordinates");
    if (poCoordinates &&
        poCoordinates->GetDataType().GetClass() == GEDTC_STRING &&
        poCoordinates->GetDimensionCount() == 0)
    {
        const char *pszCoordinates = poCoordinates->ReadAsString();
        if (pszCoordinates)
        {
            const CPLStringList aosNames(
                NCDFTokenizeCoordinatesAttribute(pszCoordinates));
            CPLMutexHolderD(&hNCMutex);
            for (int i = 0; i < aosNames.size(); i++)
            {
                int nVarId = 0;
                if (nc_inq_varid(m_gid, aosNames[i], &nVarId) == NC_NOERR)
                {
                    ret.emplace_back(netCDFVariable::Create(
                        m_poShared, m_poParent.lock(), m_gid, nVarId,
                        std::vector<std::shared_ptr<GDALDimension>>(), nullptr,
                        false));
                }
                else
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Cannot find variable corresponding to coordinate %s",
                        aosNames[i]);
                }
            }
        }
    }

    // Special case for NASA EMIT datasets
    auto apoDims = GetDimensions();
    if ((apoDims.size() == 3 && apoDims[0]->GetName() == "downtrack" &&
         apoDims[1]->GetName() == "crosstrack" &&
         apoDims[2]->GetName() == "bands") ||
        (apoDims.size() == 2 && apoDims[0]->GetName() == "downtrack" &&
         apoDims[1]->GetName() == "crosstrack"))
    {
        auto poRootGroup = netCDFGroup::Create(m_poShared, nullptr, m_gid);
        if (poRootGroup)
        {
            auto poLocationGroup = poRootGroup->OpenGroup("location");
            if (poLocationGroup)
            {
                auto poLon = poLocationGroup->OpenMDArray("lon");
                auto poLat = poLocationGroup->OpenMDArray("lat");
                if (poLon && poLat)
                {
                    return {std::move(poLon), std::move(poLat)};
                }
            }
        }
    }

    return ret;
}

/************************************************************************/
/*                            Resize()                                  */
/************************************************************************/

bool netCDFVariable::Resize(const std::vector<GUInt64> &anNewDimSizes,
                            CSLConstList /* papszOptions */)
{
    if (!IsWritable())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Resize() not supported on read-only file");
        return false;
    }

    const auto nDimCount = GetDimensionCount();
    if (anNewDimSizes.size() != nDimCount)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Not expected number of values in anNewDimSizes.");
        return false;
    }

    auto &dims = GetDimensions();
    std::vector<size_t> anGrownDimIdx;
    std::map<GDALDimension *, GUInt64> oMapDimToSize;
    for (size_t i = 0; i < nDimCount; ++i)
    {
        auto oIter = oMapDimToSize.find(dims[i].get());
        if (oIter != oMapDimToSize.end() && oIter->second != anNewDimSizes[i])
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot resize a dimension referenced several times "
                     "to different sizes");
            return false;
        }
        if (anNewDimSizes[i] != dims[i]->GetSize())
        {
            if (anNewDimSizes[i] < dims[i]->GetSize())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Resize() does not support shrinking the array.");
                return false;
            }

            oMapDimToSize[dims[i].get()] = anNewDimSizes[i];
            anGrownDimIdx.push_back(i);
        }
        else
        {
            oMapDimToSize[dims[i].get()] = dims[i]->GetSize();
        }
    }

    if (!anGrownDimIdx.empty())
    {
        CPLMutexHolderD(&hNCMutex);
        // Query which netCDF dimensions have unlimited size
        int nUnlimitedDimIds = 0;
        nc_inq_unlimdims(m_gid, &nUnlimitedDimIds, nullptr);
        std::vector<int> anUnlimitedDimIds(nUnlimitedDimIds);
        nc_inq_unlimdims(m_gid, &nUnlimitedDimIds, anUnlimitedDimIds.data());
        std::set<int> oSetUnlimitedDimId;
        for (int idx : anUnlimitedDimIds)
            oSetUnlimitedDimId.insert(idx);

        // Check that dimensions that need to grow are of unlimited size
        for (size_t dimIdx : anGrownDimIdx)
        {
            auto netCDFDim =
                std::dynamic_pointer_cast<netCDFDimension>(dims[dimIdx]);
            if (!netCDFDim)
            {
                CPLAssert(false);
            }
            else if (oSetUnlimitedDimId.find(netCDFDim->GetId()) ==
                     oSetUnlimitedDimId.end())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Resize() cannot grow dimension %d (%s) "
                         "as it is not created as UNLIMITED.",
                         static_cast<int>(dimIdx),
                         netCDFDim->GetName().c_str());
                return false;
            }
        }
        for (size_t i = 0; i < nDimCount; ++i)
        {
            if (anNewDimSizes[i] > dims[i]->GetSize())
            {
                auto netCDFDim =
                    std::dynamic_pointer_cast<netCDFDimension>(dims[i]);
                if (!netCDFDim)
                {
                    CPLAssert(false);
                }
                else
                {
                    netCDFDim->SetSize(anNewDimSizes[i]);
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool netCDFVariable::Rename(const std::string &osNewName)
{
    if (m_poShared->IsReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Rename() not supported on read-only file");
        return false;
    }
    if (osNewName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Empty name not supported");
        return false;
    }

    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret = nc_rename_var(m_gid, m_varid, osNewName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                       NotifyChildrenOfRenaming()                     */
/************************************************************************/

void netCDFVariable::NotifyChildrenOfRenaming()
{
    for (const auto &iter : m_oMapAttributes)
        iter.second->ParentRenamed(m_osFullName);
}

/************************************************************************/
/*                    retrieveAttributeParentName()                     */
/************************************************************************/

static CPLString retrieveAttributeParentName(int gid, int varid)
{
    auto groupName(NCDFGetGroupFullName(gid));
    if (varid == NC_GLOBAL)
    {
        if (groupName == "/")
            return "/_GLOBAL_";
        return groupName + "/_GLOBAL_";
    }

    return groupName + "/" + netCDFVariable::retrieveName(gid, varid);
}

/************************************************************************/
/*                          netCDFAttribute()                           */
/************************************************************************/

netCDFAttribute::netCDFAttribute(
    const std::shared_ptr<netCDFSharedResources> &poShared, int gid, int varid,
    const std::string &name)
    : GDALAbstractMDArray(retrieveAttributeParentName(gid, varid), name),
      GDALAttribute(retrieveAttributeParentName(gid, varid), name),
      m_poShared(poShared), m_gid(gid), m_varid(varid)
{
    CPLMutexHolderD(&hNCMutex);
    size_t nLen = 0;
    NCDF_ERR(nc_inq_atttype(m_gid, m_varid, GetName().c_str(), &m_nAttType));
    NCDF_ERR(nc_inq_attlen(m_gid, m_varid, GetName().c_str(), &nLen));
    if (m_nAttType == NC_CHAR)
    {
        m_nTextLength = nLen;
    }
    else if (nLen > 1)
    {
        m_dims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), "length", std::string(), std::string(), nLen));
    }
}

/************************************************************************/
/*                          netCDFAttribute()                           */
/************************************************************************/

netCDFAttribute::netCDFAttribute(
    const std::shared_ptr<netCDFSharedResources> &poShared, int gid, int varid,
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
    : GDALAbstractMDArray(retrieveAttributeParentName(gid, varid), osName),
      GDALAttribute(retrieveAttributeParentName(gid, varid), osName),
      m_poShared(poShared), m_gid(gid), m_varid(varid)
{
    CPLMutexHolderD(&hNCMutex);
    m_bPerfectDataTypeMatch = true;
    m_nAttType = CreateOrGetType(gid, oDataType);
    m_dt.reset(new GDALExtendedDataType(oDataType));
    if (!anDimensions.empty())
    {
        m_dims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), "length", std::string(), std::string(),
            anDimensions[0]));
    }

    const char *pszType = CSLFetchNameValueDef(papszOptions, "NC_TYPE", "");
    if (oDataType.GetClass() == GEDTC_STRING && anDimensions.empty() &&
        (EQUAL(pszType, "") || EQUAL(pszType, "NC_CHAR")))
    {
        m_nAttType = NC_CHAR;
    }
    else if (oDataType.GetNumericDataType() == GDT_Byte &&
             EQUAL(CSLFetchNameValueDef(papszOptions, "NC_TYPE", ""),
                   "NC_BYTE"))
    {
        m_nAttType = NC_BYTE;
    }
    else if (oDataType.GetNumericDataType() == GDT_Int16 &&
             EQUAL(CSLFetchNameValueDef(papszOptions, "NC_TYPE", ""),
                   "NC_BYTE"))
    {
        m_bPerfectDataTypeMatch = false;
        m_nAttType = NC_BYTE;
    }
    else if (oDataType.GetNumericDataType() == GDT_Float64)
    {
        if (EQUAL(pszType, "NC_INT64"))
        {
            m_bPerfectDataTypeMatch = false;
            m_nAttType = NC_INT64;
        }
        else if (EQUAL(pszType, "NC_UINT64"))
        {
            m_bPerfectDataTypeMatch = false;
            m_nAttType = NC_UINT64;
        }
    }
}

/************************************************************************/
/*                         ~netCDFAttribute()                           */
/************************************************************************/

netCDFAttribute::~netCDFAttribute()
{
    if (m_bValid)
    {
        if (auto poParent = m_poParent.lock())
            poParent->UnRegisterAttribute(this);
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<netCDFAttribute>
netCDFAttribute::Create(const std::shared_ptr<netCDFSharedResources> &poShared,
                        const std::shared_ptr<netCDFAttributeHolder> &poParent,
                        int gid, int varid, const std::string &name)
{
    auto attr(std::shared_ptr<netCDFAttribute>(
        new netCDFAttribute(poShared, gid, varid, name)));
    attr->SetSelf(attr);
    attr->m_poParent = poParent;
    if (poParent)
        poParent->RegisterAttribute(attr.get());
    return attr;
}

std::shared_ptr<netCDFAttribute> netCDFAttribute::Create(
    const std::shared_ptr<netCDFSharedResources> &poShared,
    const std::shared_ptr<netCDFAttributeHolder> &poParent, int gid, int varid,
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (poShared->IsReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateAttribute() not supported on read-only file");
        return nullptr;
    }
    if (anDimensions.size() > 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 0 or 1-dimensional attribute are supported");
        return nullptr;
    }

    const char *apszOptions[2] = {nullptr, nullptr};
    if (!poShared->IsNC4() && oDataType.GetClass() == GEDTC_NUMERIC &&
        oDataType.GetNumericDataType() == GDT_Byte && !papszOptions)
    {
        // GDT_Byte would map to a NC_UBYTE datatype, which is not available in
        // NC3 datasets
        apszOptions[0] = "NC_TYPE=NC_BYTE";
        papszOptions = apszOptions;
    }

    auto attr(std::shared_ptr<netCDFAttribute>(new netCDFAttribute(
        poShared, gid, varid, osName, anDimensions, oDataType, papszOptions)));
    if (attr->m_nAttType == NC_NAT)
        return nullptr;
    attr->SetSelf(attr);
    attr->m_poParent = poParent;
    if (poParent)
        poParent->RegisterAttribute(attr.get());
    return attr;
}

/************************************************************************/
/*                             GetDataType()                            */
/************************************************************************/

const GDALExtendedDataType &netCDFAttribute::GetDataType() const
{
    if (m_dt)
        return *m_dt;
    CPLMutexHolderD(&hNCMutex);

    if (m_nAttType == NC_CHAR)
    {
        m_dt.reset(
            new GDALExtendedDataType(GDALExtendedDataType::CreateString()));
    }
    else
    {
        m_dt.reset(new GDALExtendedDataType(
            GDALExtendedDataType::Create(GDT_Unknown)));
        BuildDataType(m_gid, m_varid, m_nAttType, m_dt,
                      m_bPerfectDataTypeMatch);
    }

    return *m_dt;
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool netCDFAttribute::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            void *pDstBuffer) const
{
    if (!CheckValidAndErrorOutIfNot())
        return false;
    CPLMutexHolderD(&hNCMutex);

    if (m_nAttType == NC_STRING)
    {
        CPLAssert(GetDataType().GetClass() == GEDTC_STRING);
        std::vector<char *> apszStrings(
            static_cast<size_t>(GetTotalElementsCount()));
        int ret = nc_get_att_string(m_gid, m_varid, GetName().c_str(),
                                    &apszStrings[0]);
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return false;
        if (m_dims.empty())
        {
            const char *pszStr = apszStrings[0];
            GDALExtendedDataType::CopyValue(&pszStr, GetDataType(), pDstBuffer,
                                            bufferDataType);
        }
        else
        {
            GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
            for (size_t i = 0; i < count[0]; i++)
            {
                auto srcIdx =
                    static_cast<size_t>(arrayStartIdx[0] + arrayStep[0] * i);
                const char *pszStr = apszStrings[srcIdx];
                GDALExtendedDataType::CopyValue(&pszStr, GetDataType(),
                                                pabyDstBuffer, bufferDataType);
                pabyDstBuffer += sizeof(char *) * bufferStride[0];
            }
        }
        nc_free_string(apszStrings.size(), &apszStrings[0]);
        return true;
    }

    if (m_nAttType == NC_CHAR)
    {
        CPLAssert(GetDataType().GetClass() == GEDTC_STRING);
        CPLAssert(m_dims.empty());
        if (bufferDataType != GetDataType())
        {
            std::string osStr;
            osStr.resize(m_nTextLength);
            int ret =
                nc_get_att_text(m_gid, m_varid, GetName().c_str(), &osStr[0]);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return false;
            const char *pszStr = osStr.c_str();
            GDALExtendedDataType::CopyValue(&pszStr, GetDataType(), pDstBuffer,
                                            bufferDataType);
        }
        else
        {
            char *pszStr = static_cast<char *>(CPLCalloc(1, m_nTextLength + 1));
            int ret =
                nc_get_att_text(m_gid, m_varid, GetName().c_str(), pszStr);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
            {
                CPLFree(pszStr);
                return false;
            }
            *static_cast<char **>(pDstBuffer) = pszStr;
        }
        return true;
    }

    const auto &dt(GetDataType());
    if (dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown)
    {
        return false;
    }

    CPLAssert(dt.GetClass() != GEDTC_STRING);
    const bool bFastPath = ((m_dims.size() == 1 && arrayStartIdx[0] == 0 &&
                             count[0] == m_dims[0]->GetSize() &&
                             arrayStep[0] == 1 && bufferStride[0] == 1) ||
                            m_dims.empty()) &&
                           m_bPerfectDataTypeMatch && bufferDataType == dt &&
                           dt.GetSize() > 0;
    if (bFastPath)
    {
        int ret = nc_get_att(m_gid, m_varid, GetName().c_str(), pDstBuffer);
        NCDF_ERR(ret);
        if (ret == NC_NOERR)
        {
            ConvertNCStringsToCPLStrings(static_cast<GByte *>(pDstBuffer), dt);
        }
        return ret == NC_NOERR;
    }

    const auto nElementSize =
        GetNCTypeSize(dt, m_bPerfectDataTypeMatch, m_nAttType);
    if (nElementSize == 0)
        return false;
    const auto nOutputDTSize = bufferDataType.GetSize();
    std::vector<GByte> abyBuffer(static_cast<size_t>(GetTotalElementsCount()) *
                                 nElementSize);
    int ret = nc_get_att(m_gid, m_varid, GetName().c_str(), &abyBuffer[0]);
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    GByte *pabySrcBuffer =
        m_dims.empty()
            ? abyBuffer.data()
            : abyBuffer.data() +
                  static_cast<size_t>(arrayStartIdx[0]) * nElementSize;
    GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
    for (size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++)
    {
        GByte abyTmpBuffer[sizeof(double)];
        const GByte *pabySrcElement = pabySrcBuffer;
        if (!m_bPerfectDataTypeMatch)
        {
            if (m_nAttType == NC_BYTE)
            {
                short s =
                    reinterpret_cast<const signed char *>(pabySrcBuffer)[0];
                memcpy(abyTmpBuffer, &s, sizeof(s));
                pabySrcElement = abyTmpBuffer;
            }
            else if (m_nAttType == NC_INT64)
            {
                double v = static_cast<double>(
                    reinterpret_cast<const GInt64 *>(pabySrcBuffer)[0]);
                memcpy(abyTmpBuffer, &v, sizeof(v));
                pabySrcElement = abyTmpBuffer;
            }
            else if (m_nAttType == NC_UINT64)
            {
                double v = static_cast<double>(
                    reinterpret_cast<const GUInt64 *>(pabySrcBuffer)[0]);
                memcpy(abyTmpBuffer, &v, sizeof(v));
                pabySrcElement = abyTmpBuffer;
            }
            else
            {
                CPLAssert(false);
            }
        }
        GDALExtendedDataType::CopyValue(pabySrcElement, dt, pabyDstBuffer,
                                        bufferDataType);
        FreeNCStrings(pabySrcBuffer, dt);
        if (!m_dims.empty())
        {
            pabySrcBuffer +=
                static_cast<std::ptrdiff_t>(arrayStep[0] * nElementSize);
            pabyDstBuffer += nOutputDTSize * bufferStride[0];
        }
    }

    return true;
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

bool netCDFAttribute::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                             const GInt64 *arrayStep,
                             const GPtrDiff_t *bufferStride,
                             const GDALExtendedDataType &bufferDataType,
                             const void *pSrcBuffer)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;
    CPLMutexHolderD(&hNCMutex);

    if (m_dims.size() == 1 &&
        (arrayStartIdx[0] != 0 || count[0] != m_dims[0]->GetSize() ||
         arrayStep[0] != 1))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only contiguous writing of attribute values supported");
        return false;
    }

    m_poShared->SetDefineMode(true);

    const auto &dt(GetDataType());
    if (m_nAttType == NC_STRING)
    {
        CPLAssert(dt.GetClass() == GEDTC_STRING);
        if (m_dims.empty())
        {
            char *pszStr = nullptr;
            const char *pszStrConst;
            if (bufferDataType != dt)
            {
                GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType,
                                                &pszStr, dt);
                pszStrConst = pszStr;
            }
            else
            {
                memcpy(&pszStrConst, pSrcBuffer, sizeof(const char *));
            }
            int ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(), 1,
                                        &pszStrConst);
            CPLFree(pszStr);
            NCDF_ERR(ret);
            if (ret != NC_NOERR)
                return false;
            return true;
        }

        int ret;
        if (bufferDataType != dt)
        {
            std::vector<char *> apszStrings(count[0]);
            const auto nInputDTSize = bufferDataType.GetSize();
            const GByte *pabySrcBuffer = static_cast<const GByte *>(pSrcBuffer);
            for (size_t i = 0; i < count[0]; i++)
            {
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                                                &apszStrings[i], dt);
                pabySrcBuffer += nInputDTSize * bufferStride[0];
            }
            ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(), count[0],
                                    const_cast<const char **>(&apszStrings[0]));
            for (size_t i = 0; i < count[0]; i++)
            {
                CPLFree(apszStrings[i]);
            }
        }
        else
        {
            const char **ppszStr;
            memcpy(&ppszStr, &pSrcBuffer, sizeof(const char **));
            ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(), count[0],
                                    ppszStr);
        }
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return false;
        return true;
    }

    if (m_nAttType == NC_CHAR)
    {
        CPLAssert(dt.GetClass() == GEDTC_STRING);
        CPLAssert(m_dims.empty());
        char *pszStr = nullptr;
        const char *pszStrConst;
        if (bufferDataType != dt)
        {
            GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &pszStr,
                                            dt);
            pszStrConst = pszStr;
        }
        else
        {
            memcpy(&pszStrConst, pSrcBuffer, sizeof(const char *));
        }
        m_nTextLength = pszStrConst ? strlen(pszStrConst) : 0;
        int ret = nc_put_att_text(m_gid, m_varid, GetName().c_str(),
                                  m_nTextLength, pszStrConst);
        CPLFree(pszStr);
        NCDF_ERR(ret);
        if (ret != NC_NOERR)
            return false;
        return true;
    }

    if (dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown)
    {
        return false;
    }

    CPLAssert(dt.GetClass() != GEDTC_STRING);
    const bool bFastPath =
        ((m_dims.size() == 1 && bufferStride[0] == 1) || m_dims.empty()) &&
        m_bPerfectDataTypeMatch && bufferDataType == dt && dt.GetSize() > 0;
    if (bFastPath)
    {
        int ret = nc_put_att(m_gid, m_varid, GetName().c_str(), m_nAttType,
                             m_dims.empty() ? 1 : count[0], pSrcBuffer);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }

    const auto nElementSize =
        GetNCTypeSize(dt, m_bPerfectDataTypeMatch, m_nAttType);
    if (nElementSize == 0)
        return false;
    const auto nInputDTSize = bufferDataType.GetSize();
    std::vector<GByte> abyBuffer(static_cast<size_t>(GetTotalElementsCount()) *
                                 nElementSize);

    const GByte *pabySrcBuffer = static_cast<const GByte *>(pSrcBuffer);
    auto pabyDstBuffer = &abyBuffer[0];
    for (size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++)
    {
        if (!m_bPerfectDataTypeMatch)
        {
            if (m_nAttType == NC_BYTE)
            {
                short s;
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                                                &s, dt);
                signed char c = static_cast<signed char>(s);
                memcpy(pabyDstBuffer, &c, sizeof(c));
            }
            else if (m_nAttType == NC_INT64)
            {
                double d;
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                                                &d, dt);
                GInt64 v = static_cast<GInt64>(d);
                memcpy(pabyDstBuffer, &v, sizeof(v));
            }
            else if (m_nAttType == NC_UINT64)
            {
                double d;
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                                                &d, dt);
                GUInt64 v = static_cast<GUInt64>(d);
                memcpy(pabyDstBuffer, &v, sizeof(v));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else
        {
            GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                                            pabyDstBuffer, dt);
        }

        if (!m_dims.empty())
        {
            pabySrcBuffer += nInputDTSize * bufferStride[0];
            pabyDstBuffer += nElementSize;
        }
    }

    int ret = nc_put_att(m_gid, m_varid, GetName().c_str(), m_nAttType,
                         m_dims.empty() ? 1 : count[0], &abyBuffer[0]);
    NCDF_ERR(ret);
    return ret == NC_NOERR;
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool netCDFAttribute::Rename(const std::string &osNewName)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;
    if (m_poShared->IsReadOnly())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Rename() not supported on read-only file");
        return false;
    }
    if (osNewName.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Empty name not supported");
        return false;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);

    int ret =
        nc_rename_att(m_gid, m_varid, m_osName.c_str(), osNewName.c_str());
    NCDF_ERR(ret);
    if (ret != NC_NOERR)
        return false;

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                           OpenMultiDim()                             */
/************************************************************************/

GDALDataset *netCDFDataset::OpenMultiDim(GDALOpenInfo *poOpenInfo)
{

    CPLMutexHolderD(&hNCMutex);

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    netCDFDataset *poDS = new netCDFDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);

    std::string osFilename;

    // For example to open DAP datasets
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:"))
    {
        osFilename = poOpenInfo->pszFilename + strlen("NETCDF:");
        if (!osFilename.empty() && osFilename[0] == '"' &&
            osFilename.back() == '"')
        {
            osFilename = osFilename.substr(1, osFilename.size() - 2);
        }
    }
    else
    {
        osFilename = poOpenInfo->pszFilename;
        poDS->eFormat =
            netCDFIdentifyFormat(poOpenInfo, /* bCheckExt = */ true);
    }

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);

#ifdef ENABLE_NCDUMP
    bool bFileToDestroyAtClosing = false;
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (poOpenInfo->fpL != nullptr && STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") && strstr(pszHeader, "variables:"))
    {
        // By default create a temporary file that will be destroyed,
        // unless NETCDF_TMP_FILE is defined. Can be useful to see which
        // netCDF file has been generated from a potential fuzzed input.
        osFilename = CPLGetConfigOption("NETCDF_TMP_FILE", "");
        if (osFilename.empty())
        {
            bFileToDestroyAtClosing = true;
            osFilename = CPLGenerateTempFilename("netcdf_tmp");
        }
        if (!netCDFDatasetCreateTempFile(NCDF_FORMAT_NC4, osFilename.c_str(),
                                         poOpenInfo->fpL))
        {
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll
                                        // deadlock with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return nullptr;
        }
        poDS->eFormat = NCDF_FORMAT_NC4;
    }
#endif

    // Try opening the dataset.
#if defined(NCDF_DEBUG) && defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open_mem(%s)", osFilename.c_str());
#elif defined(NCDF_DEBUG) && !defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open(%s)", osFilename.c_str());
#endif
    int cdfid = -1;
    const int nMode =
        (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0 ? NC_WRITE : NC_NOWRITE;
    CPLString osFilenameForNCOpen(osFilename);
#ifdef _WIN32
    if (CPLTestBool(CPLGetConfigOption("GDAL_FILENAME_IS_UTF8", "YES")))
    {
        char *pszTemp = CPLRecode(osFilenameForNCOpen, CPL_ENC_UTF8, "CP_ACP");
        osFilenameForNCOpen = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int status2 = -1;

    auto poSharedResources(std::make_shared<netCDFSharedResources>(osFilename));
#ifdef ENABLE_NCDUMP
    poSharedResources->m_bFileToDestroyAtClosing = bFileToDestroyAtClosing;
#endif

    if (STARTS_WITH(osFilenameForNCOpen, "/vsimem/") &&
        poOpenInfo->eAccess == GA_ReadOnly)
    {
        vsi_l_offset nLength = 0;
        poDS->fpVSIMEM = VSIFOpenL(osFilenameForNCOpen, "rb");
        if (poDS->fpVSIMEM)
        {
            // We assume that the file will not be modified. If it is, then
            // pabyBuffer might become invalid.
            GByte *pabyBuffer =
                VSIGetMemFileBuffer(osFilenameForNCOpen, &nLength, false);
            if (pabyBuffer)
            {
                status2 = nc_open_mem(CPLGetFilename(osFilenameForNCOpen),
                                      nMode, static_cast<size_t>(nLength),
                                      pabyBuffer, &cdfid);
            }
        }
    }
    else
    {
#ifdef ENABLE_UFFD
        bool bVsiFile = !strncmp(osFilenameForNCOpen, "/vsi", strlen("/vsi"));
        bool bReadOnly = (poOpenInfo->eAccess == GA_ReadOnly);
        void *pVma = nullptr;
        uint64_t nVmaSize = 0;
        cpl_uffd_context *pCtx = nullptr;

        if (bVsiFile && bReadOnly && CPLIsUserFaultMappingSupported())
            pCtx = CPLCreateUserFaultMapping(osFilenameForNCOpen, &pVma,
                                             &nVmaSize);
        if (pCtx != nullptr && pVma != nullptr && nVmaSize > 0)
        {
            // netCDF code, at least for netCDF 4.7.0, is confused by filenames
            // like /vsicurl/http[s]://example.com/foo.nc, so just pass the
            // final part
            status2 = nc_open_mem(CPLGetFilename(osFilenameForNCOpen), nMode,
                                  static_cast<size_t>(nVmaSize), pVma, &cdfid);
        }
        else
            status2 = GDAL_nc_open(osFilenameForNCOpen, nMode, &cdfid);
        poSharedResources->m_pUffdCtx = pCtx;
#else
        status2 = GDAL_nc_open(osFilenameForNCOpen, nMode, &cdfid);
#endif
    }
    if (status2 != NC_NOERR)
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "error opening");
#endif
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }
#ifdef NCDF_DEBUG
    CPLDebug("GDAL_netCDF", "got cdfid=%d", cdfid);
#endif

#if defined(ENABLE_NCDUMP) && !defined(_WIN32)
    // Try to destroy the temporary file right now on Unix
    if (poSharedResources->m_bFileToDestroyAtClosing)
    {
        if (VSIUnlink(poSharedResources->m_osFilename) == 0)
        {
            poSharedResources->m_bFileToDestroyAtClosing = false;
        }
    }
#endif
    poSharedResources->m_bReadOnly = nMode == NC_NOWRITE;
    poSharedResources->m_bIsNC4 =
        poDS->eFormat == NCDF_FORMAT_NC4 || poDS->eFormat == NCDF_FORMAT_NC4C;
    poSharedResources->m_cdfid = cdfid;
    poSharedResources->m_fpVSIMEM = poDS->fpVSIMEM;
    poDS->fpVSIMEM = nullptr;

    // Is this a real netCDF file?
    int ndims;
    int ngatts;
    int nvars;
    int unlimdimid;
    int status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if (status != NC_NOERR)
    {
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }

    poDS->m_poRootGroup = netCDFGroup::Create(poSharedResources, cdfid);

    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                          GetRootGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> netCDFDataset::GetRootGroup() const
{
    return m_poRootGroup;
}

/************************************************************************/
/*                      CreateMultiDimensional()                        */
/************************************************************************/

GDALDataset *
netCDFDataset::CreateMultiDimensional(const char *pszFilename,
                                      CSLConstList /* papszRootGroupOptions */,
                                      CSLConstList papszOptions)
{
    CPLMutexHolderD(&hNCMutex);

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    netCDFDataset *poDS = new netCDFDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);
    poDS->eAccess = GA_Update;
    poDS->osFilename = pszFilename;

    // process options.
    poDS->papszCreationOptions = CSLDuplicate(papszOptions);
    if (CSLFetchNameValue(papszOptions, "FORMAT") == nullptr)
    {
        poDS->papszCreationOptions =
            CSLSetNameValue(poDS->papszCreationOptions, "FORMAT", "NC4");
    }
    poDS->ProcessCreationOptions();

    // Create the dataset.
    CPLString osFilenameForNCCreate(pszFilename);
#ifdef _WIN32
    if (CPLTestBool(CPLGetConfigOption("GDAL_FILENAME_IS_UTF8", "YES")))
    {
        char *pszTemp =
            CPLRecode(osFilenameForNCCreate, CPL_ENC_UTF8, "CP_ACP");
        osFilenameForNCCreate = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int cdfid = 0;
    int status = nc_create(osFilenameForNCCreate, poDS->nCreateMode, &cdfid);
    if (status != NC_NOERR)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create netCDF file %s (Error code %d): %s .",
                 pszFilename, status, nc_strerror(status));
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }

    auto poSharedResources(
        std::make_shared<netCDFSharedResources>(pszFilename));
    poSharedResources->m_cdfid = cdfid;
    poSharedResources->m_bReadOnly = false;
    poSharedResources->m_bDefineMode = true;
    poSharedResources->m_bIsNC4 =
        poDS->eFormat == NCDF_FORMAT_NC4 || poDS->eFormat == NCDF_FORMAT_NC4C;
    poDS->m_poRootGroup =
        netCDFGroup::Create(poSharedResources, nullptr, cdfid);
    const char *pszConventions = CSLFetchNameValueDef(
        papszOptions, "CONVENTIONS", NCDF_CONVENTIONS_CF_V1_6);
    if (!EQUAL(pszConventions, ""))
    {
        auto poAttr = poDS->m_poRootGroup->CreateAttribute(
            NCDF_CONVENTIONS, {}, GDALExtendedDataType::CreateString());
        if (poAttr)
            poAttr->Write(pszConventions);
    }

    return poDS;
}
