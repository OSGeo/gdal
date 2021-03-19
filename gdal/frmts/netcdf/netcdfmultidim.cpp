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

#ifdef NETCDF_HAS_NC4

/************************************************************************/
/*                         netCDFSharedResources                        */
/************************************************************************/

class netCDFSharedResources
{
    friend class netCDFDataset;

    bool m_bImappIsInElements = true;
    bool m_bReadOnly = true;
    int m_cdfid = 0;
#ifdef ENABLE_NCDUMP
    bool m_bFileToDestroyAtClosing = false;
#endif
    CPLString m_osFilename{};
#ifdef ENABLE_UFFD
    cpl_uffd_context *m_pUffdCtx = nullptr;
#endif
    bool m_bDefineMode = false;
    std::map<int, int> m_oMapDimIdToGroupId{};
    bool m_bIsInIndexingVariable = false;

public:
    netCDFSharedResources();
    ~netCDFSharedResources();

    inline int GetCDFId() const { return m_cdfid; }
    inline bool IsReadOnly() const { return m_bReadOnly; }
    bool SetDefineMode(bool bNewDefineMode);
    int GetBelongingGroupOfDim(int startgid, int dimid);
    inline bool GetImappIsInElements() const { return m_bImappIsInElements; }

    void SetIsInGetIndexingVariable(bool b) { m_bIsInIndexingVariable = b; }
    bool GetIsInIndexingVariable() const { return m_bIsInIndexingVariable; }
};

/************************************************************************/
/*                       netCDFSharedResources()                        */
/************************************************************************/

netCDFSharedResources::netCDFSharedResources()
{
    // netcdf >= 4.4 uses imapp argument of nc_get/put_varm as a stride in
    // elements, whereas earlier versions use bytes.
    CPLStringList aosVersionNumbers(CSLTokenizeString2(nc_inq_libvers(), ".", 0));
    m_bImappIsInElements = false;
    if( aosVersionNumbers.size() >= 3 )
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
    if (oIter != m_oMapDimIdToGroupId.end() )
        return oIter->second;

    int gid = startgid;
    while( true )
    {
        int nbDims = 0;
        NCDF_ERR(nc_inq_ndims(gid, &nbDims));
        if( nbDims > 0 )
        {
            std::vector<int> dimids(nbDims);
            NCDF_ERR(nc_inq_dimids(gid, &nbDims, &dimids[0], FALSE)); 
            for( int i = 0; i < nbDims; i++ )
            {
                m_oMapDimIdToGroupId[dimid] = gid;
                if( dimids[i] == dimid )
                    return gid;
            }
        }
        int nParentGID = 0;
        if( nc_inq_grp_parent(gid, &nParentGID) != NC_NOERR )
            return startgid;
        gid = nParentGID;
    }
}


/************************************************************************/
/*                            SetDefineMode()                           */
/************************************************************************/

bool netCDFSharedResources::SetDefineMode( bool bNewDefineMode )
{
    // Do nothing if already in new define mode
    // or if dataset is in read-only mode.
    if( m_bDefineMode == bNewDefineMode || m_bReadOnly )
        return true;

    CPLDebug("GDAL_netCDF", "SetDefineMode(%d) old=%d",
             static_cast<int>(bNewDefineMode), static_cast<int>(m_bDefineMode));

    m_bDefineMode = bNewDefineMode;

    int status;
    if( m_bDefineMode )
        status = nc_redef(m_cdfid);
    else
        status = nc_enddef(m_cdfid);

    NCDF_ERR(status);
    return status == NC_NOERR;
}

/************************************************************************/
/*                           netCDFGroup                                */
/************************************************************************/

class netCDFGroup final: public GDALGroup
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    int m_gid = 0;
    CPLStringList m_aosStructuralInfo{};

    static std::string retrieveName(int gid)
    {
        CPLMutexHolderD(&hNCMutex);
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_grpname(gid, szName));
        return szName;
    }

public:
    netCDFGroup(const std::shared_ptr<netCDFSharedResources>& poShared, int gid);

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                            CSLConstList papszOptions) override;

    std::shared_ptr<GDALDimension> CreateDimension(const std::string& osName,
                                                    const std::string& osType,
                                                    const std::string& osDirection,
                                                    GUInt64 nSize,
                                                    CSLConstList papszOptions) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                               const GDALExtendedDataType& oDataType,
                                               CSLConstList papszOptions) override;

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions) override;

    CSLConstList GetStructuralInfo() const override;
};

/************************************************************************/
/*                   netCDFVirtualGroupBySameDimension                  */
/************************************************************************/

class netCDFVirtualGroupBySameDimension final: public GDALGroup
{
    // the real group to which we derived this virtual group from
    std::shared_ptr<netCDFGroup> m_poGroup;
    std::string m_osDimName{};

public:
    netCDFVirtualGroupBySameDimension(const std::shared_ptr<netCDFGroup>& poGroup,
                                      const std::string& osDimName);

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                         netCDFDimension                              */
/************************************************************************/

class netCDFDimension final: public GDALDimension
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    int m_gid = 0;
    int m_dimid = 0;

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
    netCDFDimension(const std::shared_ptr<netCDFSharedResources>& poShared, int cfid,
                    int dimid, size_t nForcedSize, const std::string& osType);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;

    int GetId() const { return m_dimid; }
};

/************************************************************************/
/*                         netCDFAttribute                              */
/************************************************************************/


class netCDFAttribute final: public GDALAttribute
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
    int m_gid = 0;
    int m_varid = 0;
    size_t m_nTextLength = 0;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    nc_type m_nAttType = NC_NAT;
    mutable std::unique_ptr<GDALExtendedDataType> m_dt;
    mutable bool m_bPerfectDataTypeMatch = false;

protected:
    netCDFAttribute(const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& name);

    netCDFAttribute(const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& osName,
                    const std::vector<GUInt64>& anDimensions,
                    const GDALExtendedDataType& oDataType,
                    CSLConstList papszOptions);

    bool IRead(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;

public:
    static std::shared_ptr<netCDFAttribute> Create(
                    const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& name);

    static std::shared_ptr<netCDFAttribute> Create(
                    const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& osName,
                    const std::vector<GUInt64>& anDimensions,
                    const GDALExtendedDataType& oDataType,
                    CSLConstList papszOptions);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override;
};

/************************************************************************/
/*                         netCDFVariable                               */
/************************************************************************/

class netCDFVariable final: public GDALMDArray
{
    std::shared_ptr<netCDFSharedResources> m_poShared;
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
    std::string m_osUnit{};
    CPLStringList m_aosStructuralInfo{};
    mutable bool m_bSRSRead = false;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};
    bool m_bWriteGDALTags = true;
    size_t m_nTextLength = 0;
    mutable std::vector<GUInt64> m_cachedArrayStartIdx{};
    mutable std::vector<size_t> m_cachedCount{};
    mutable std::shared_ptr<GDALMDArray> m_poCachedArray{};

    void ConvertNCToGDAL(GByte*) const;
    void ConvertGDALToNC(GByte*) const;

    bool ReadOneElement(const GDALExtendedDataType& src_datatype,
                        const GDALExtendedDataType& bufferDataType,
                        const size_t* array_idx,
                        void* pDstBuffer) const;

    bool WriteOneElement(const GDALExtendedDataType& dst_datatype,
                         const GDALExtendedDataType& bufferDataType,
                         const size_t* array_idx,
                         const void* pSrcBuffer) const;

    template< typename BufferType,
              typename NCGetPutVar1FuncType,
              typename ReadOrWriteOneElementType >
    bool IReadWriteGeneric(const size_t* arrayStartIdx,
                                  const size_t* count,
                                  const GInt64* arrayStep,
                                  const GPtrDiff_t* bufferStride,
                                  const GDALExtendedDataType& bufferDataType,
                                  BufferType buffer,
                                  NCGetPutVar1FuncType NCGetPutVar1Func,
                                  ReadOrWriteOneElementType ReadOrWriteOneElement) const;

    template< typename BufferType,
              typename NCGetPutVar1FuncType,
              typename NCGetPutVaraFuncType,
              typename NCGetPutVarmFuncType,
              typename ReadOrWriteOneElementType >
    bool IReadWrite(const bool bIsRead,
                    const GUInt64* arrayStartIdx,
                                    const size_t* count,
                                    const GInt64* arrayStep,
                                    const GPtrDiff_t* bufferStride,
                                    const GDALExtendedDataType& bufferDataType,
                                    BufferType buffer,
                                    NCGetPutVar1FuncType NCGetPutVar1Func,
                                    NCGetPutVaraFuncType NCGetPutVaraFunc,
                                    NCGetPutVarmFuncType NCGetPutVarmFunc,
                                    ReadOrWriteOneElementType ReadOrWriteOneElement) const;

protected:
    netCDFVariable(const std::shared_ptr<netCDFSharedResources>& poShared,
                   int gid, int varid,
                   const std::vector<std::shared_ptr<GDALDimension>>& dims,
                   CSLConstList papszOptions);

    bool IRead(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;

    bool IAdviseRead(const GUInt64* arrayStartIdx,
                     const size_t* count) const override;

public:
    static std::shared_ptr<netCDFVariable> Create(
                   const std::shared_ptr<netCDFSharedResources>& poShared,
                   int gid, int varid,
                   const std::vector<std::shared_ptr<GDALDimension>>& dims,
                   CSLConstList papszOptions,
                   bool bCreate)
    {
        auto var(std::shared_ptr<netCDFVariable>(new netCDFVariable(
            poShared, gid, varid, dims, papszOptions)));
        var->SetSelf(var);
        var->m_bHasWrittenData = !bCreate;
        return var;
    }

    bool IsWritable() const override { return !m_poShared->IsReadOnly(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions) override;

    const void* GetRawNoDataValue() const override;

    bool SetRawNoDataValue(const void*) override;

    std::vector<GUInt64> GetBlockSize() const override;

    CSLConstList GetStructuralInfo() const override;

    const std::string& GetUnit() const override { return m_osUnit; }

    bool SetUnit(const std::string& osUnit) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;

    bool SetSpatialRef(const OGRSpatialReference* poSRS) override;

    double GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const override;

    double GetScale(bool* pbHasScale, GDALDataType* peStorageType) const override;

    bool SetOffset(double dfOffset, GDALDataType eStorageType) override;

    bool SetScale(double dfScale, GDALDataType eStorageType) override;

    int GetGroupId() const { return m_gid; }
    int GetVarId() const { return m_varid; }

    static std::string retrieveName(int gid, int varid)
    {
        CPLMutexHolderD(&hNCMutex);
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_varname(gid, varid, szName));
        return szName;
    }
};

/************************************************************************/
/*                       ~netCDFSharedResources()                       */
/************************************************************************/

netCDFSharedResources::~netCDFSharedResources()
{
    CPLMutexHolderD(&hNCMutex);

    if( m_cdfid > 0 )
    {
#ifdef NCDF_DEBUG
        CPLDebug("GDAL_netCDF", "calling nc_close( %d)", m_cdfid);
#endif
        int status = nc_close(m_cdfid);
        NCDF_ERR(status);
    }

#ifdef ENABLE_UFFD
    if( m_pUffdCtx )
    {
        NETCDF_UFFD_UNMAP(m_pUffdCtx);
    }
#endif

#ifdef ENABLE_NCDUMP
    if( m_bFileToDestroyAtClosing )
        VSIUnlink( m_osFilename );
#endif
}

/************************************************************************/
/*                     NCDFGetParentGroupName()                         */
/************************************************************************/

static CPLString NCDFGetParentGroupName(int gid)
{
    int nParentGID = 0;
    if( nc_inq_grp_parent(gid, &nParentGID) != NC_NOERR )
        return std::string();
    return NCDFGetGroupFullName(nParentGID);
}

/************************************************************************/
/*                             netCDFGroup()                            */
/************************************************************************/

netCDFGroup::netCDFGroup(const std::shared_ptr<netCDFSharedResources>& poShared, int gid):
    GDALGroup(NCDFGetParentGroupName(gid), retrieveName(gid)),
    m_poShared(poShared),
    m_gid(gid)
{
    if( m_gid == m_poShared->GetCDFId() )
    {
        int nFormat = 0;
        nc_inq_format(m_gid, &nFormat);
        if( nFormat == NC_FORMAT_CLASSIC )
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "CLASSIC");
        }
#ifdef NC_FORMAT_64BIT_OFFSET
        else if( nFormat == NC_FORMAT_64BIT_OFFSET )
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "64BIT_OFFSET");
        }
#endif
#ifdef NC_FORMAT_CDF5
        else if( nFormat == NC_FORMAT_CDF5 )
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "CDF5");
        }
#endif
        else if( nFormat == NC_FORMAT_NETCDF4 )
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "NETCDF4");
        }
        else if( nFormat == NC_FORMAT_NETCDF4_CLASSIC )
        {
            m_aosStructuralInfo.SetNameValue("NC_FORMAT", "NETCDF4_CLASSIC");
        } 
    }
}

/************************************************************************/
/*                             CreateGroup()                            */
/************************************************************************/

std::shared_ptr<GDALGroup> netCDFGroup::CreateGroup(const std::string& osName,
                                                 CSLConstList /*papszOptions*/)
{
    if( osName.empty() )
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
    if( ret != NC_NOERR )
        return nullptr;
    return std::make_shared<netCDFGroup>(m_poShared, nSubGroupId);
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension> netCDFGroup::CreateDimension(const std::string& osName,
                                                            const std::string& osType,
                                                            const std::string&,
                                                            GUInt64 nSize,
                                                            CSLConstList papszOptions)
{
    const bool bUnlimited = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "UNLIMITED", "FALSE"));
    if( static_cast<size_t>(nSize) != nSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid size");
        return nullptr;
    }
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int nDimId = -1;
    NCDF_ERR(nc_def_dim(m_gid, osName.c_str(), static_cast<size_t>(bUnlimited ? 0 : nSize), &nDimId));
    if( nDimId < 0 )
        return nullptr;
    return std::make_shared<netCDFDimension>(
        m_poShared, m_gid, nDimId, static_cast<size_t>(nSize), osType);
}

/************************************************************************/
/*                     CreateOrGetComplexDataType()                     */
/************************************************************************/

static int CreateOrGetComplexDataType(int gid, GDALDataType eDT)
{
    const char* pszName = "";
    int nSubTypeId = NC_NAT;
    switch(eDT)
    {
        case GDT_CInt16: pszName = "ComplexInt16"; nSubTypeId = NC_SHORT; break;
        case GDT_CInt32: pszName = "ComplexInt32"; nSubTypeId = NC_INT; break;
        case GDT_CFloat32: pszName = "ComplexFloat32"; nSubTypeId = NC_FLOAT; break;
        case GDT_CFloat64: pszName = "ComplexFloat64"; nSubTypeId = NC_DOUBLE; break;
        default: CPLAssert(false); break;
    }
    int nTypeId = NC_NAT;
    if( nc_inq_typeid(gid, pszName, &nTypeId) == NC_NOERR )
    {
        // We could check that the type definition is really the one we want
        return nTypeId;
    }
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    NCDF_ERR(nc_def_compound(gid, nDTSize, pszName, &nTypeId));
    if( nTypeId != NC_NAT )
    {
        NCDF_ERR(nc_insert_compound(gid, nTypeId, "real", 0, nSubTypeId));
        NCDF_ERR(nc_insert_compound(gid, nTypeId, "imag", nDTSize / 2, nSubTypeId));
    }
    return nTypeId;
}

/************************************************************************/
/*                    CreateOrGetCompoundDataType()                     */
/************************************************************************/

static int CreateOrGetType(int gid, const GDALExtendedDataType& oType);

static int CreateOrGetCompoundDataType(int gid, const GDALExtendedDataType& oType)
{
    int nTypeId = NC_NAT;
    if( nc_inq_typeid(gid, oType.GetName().c_str(), &nTypeId) == NC_NOERR )
    {
        // We could check that the type definition is really the one we want
        return nTypeId;
    }
    NCDF_ERR(nc_def_compound(gid, oType.GetSize(), oType.GetName().c_str(), &nTypeId));
    if( nTypeId != NC_NAT )
    {
        for( const auto& comp: oType.GetComponents() )
        {
            int nSubTypeId = CreateOrGetType(gid, comp->GetType());
            if( nSubTypeId == NC_NAT )
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

static int CreateOrGetType(int gid, const GDALExtendedDataType& oType)
{
    int nTypeId = NC_NAT;
    const auto typeClass = oType.GetClass();
    if( typeClass == GEDTC_NUMERIC )
    {
        switch( oType.GetNumericDataType() )
        {
            case GDT_Byte: nTypeId = NC_UBYTE; break;
            case GDT_UInt16: nTypeId = NC_USHORT; break;
            case GDT_Int16: nTypeId = NC_SHORT; break;
            case GDT_UInt32: nTypeId = NC_UINT; break;
            case GDT_Int32: nTypeId = NC_INT; break;
            case GDT_Float32: nTypeId = NC_FLOAT; break;
            case GDT_Float64: nTypeId = NC_DOUBLE; break;
            case GDT_CInt16:
            case GDT_CInt32:
            case GDT_CFloat32:
            case GDT_CFloat64:
                nTypeId = CreateOrGetComplexDataType(gid,
                                                     oType.GetNumericDataType());
                break;
            default:
                break;
        }
    }
    else if( typeClass == GEDTC_STRING )
    {
        nTypeId = NC_STRING;
    }
    else if( typeClass == GEDTC_COMPOUND )
    {
        nTypeId = CreateOrGetCompoundDataType(gid, oType);
    }
    return nTypeId;
}

/************************************************************************/
/*                            CreateMDArray()                           */
/************************************************************************/

std::shared_ptr<GDALMDArray> netCDFGroup::CreateMDArray(
    const std::string& osName,
    const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
    const GDALExtendedDataType& oType,
    CSLConstList papszOptions)
{
    if( osName.empty() )
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
    for(const auto& dim: aoDimensions )
    {
        int nDimId = -1;
        auto netCDFDim = std::dynamic_pointer_cast<netCDFDimension>(dim);
        if( netCDFDim )
        {
            nDimId = netCDFDim->GetId();
        }
        else
        {
            if( nc_inq_dimid(m_gid, dim->GetName().c_str(), &nDimId) == NC_NOERR )
            {
                netCDFDim = std::make_shared<netCDFDimension>(
                    m_poShared, m_gid, nDimId, 0, dim->GetType());
                if( netCDFDim->GetSize() != dim->GetSize() )
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
                netCDFDim = std::dynamic_pointer_cast<netCDFDimension>(
                    CreateDimension(dim->GetName(),
                                    dim->GetType(),
                                    dim->GetDirection(),
                                    dim->GetSize(), nullptr));
                if( !netCDFDim )
                    return nullptr;
                nDimId = netCDFDim->GetId();
            }
        }
        anDimIds.push_back(nDimId);
        dims.emplace_back(netCDFDim);
    }
    int nTypeId = CreateOrGetType(m_gid, oType);
    if( nTypeId == NC_NAT )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unhandled data type");
        return nullptr;
    }
    const char* pszType = CSLFetchNameValueDef(papszOptions, "NC_TYPE", "");
    if( (EQUAL(pszType, "") || EQUAL(pszType, "NC_CHAR")) &&
        dims.size() == 1 &&
        oType.GetClass() == GEDTC_STRING &&
        oType.GetMaxStringLength() > 0 )
    {
        nTypeId = NC_CHAR;
        auto dimLength = std::dynamic_pointer_cast<netCDFDimension>(
                    CreateDimension(aoDimensions[0]->GetName() + "_length",
                                    std::string(),
                                    std::string(),
                                    oType.GetMaxStringLength(), nullptr));
        if( !dimLength )
            return nullptr;
        anDimIds.push_back(dimLength->GetId());
    }
    else if( EQUAL(pszType, "NC_BYTE") )
        nTypeId = NC_BYTE;
    else if( EQUAL(pszType, "NC_INT64") )
        nTypeId = NC_INT64;
    else if( EQUAL(pszType, "NC_UINT64") )
        nTypeId = NC_UINT64;
    NCDF_ERR(nc_def_var(m_gid, osName.c_str(), nTypeId,
                        static_cast<int>(anDimIds.size()),
                        anDimIds.empty() ? nullptr : anDimIds.data(),
                        &nVarId));
    if( nVarId < 0 )
        return nullptr;

    const char* pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if( pszBlockSize &&
        /* ignore for now BLOCKSIZE for 1-dim string variables created as 2-dim */
        anDimIds.size() == aoDimensions.size() )
    {
        auto aszTokens(CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if( static_cast<size_t>(aszTokens.size()) != aoDimensions.size() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return nullptr;
        }
        if( !aoDimensions.empty() )
        {
            std::vector<size_t> anChunkSize(aoDimensions.size());
            for( size_t i = 0; i < anChunkSize.size(); ++i )
            {
                anChunkSize[i] = static_cast<size_t>(CPLAtoGIntBig(aszTokens[i]));
            }
            int ret = nc_def_var_chunking(m_gid, nVarId, NC_CHUNKED, &anChunkSize[0]);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return nullptr;
        }
    }

    const char* pszCompress = CSLFetchNameValue(papszOptions, "COMPRESS");
    if( pszCompress && EQUAL(pszCompress, "DEFLATE") )
    {
        int nZLevel = NCDF_DEFLATE_LEVEL;
        const char* pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
        if( pszZLevel != nullptr )
        {
            nZLevel = atoi(pszZLevel);
            if( !(nZLevel >= 1 && nZLevel <= 9) )
            {
                CPLError(CE_Warning, CPLE_IllegalArg,
                        "ZLEVEL=%s value not recognised, ignoring.", pszZLevel);
                nZLevel = NCDF_DEFLATE_LEVEL;
            }
        }
        int ret = nc_def_var_deflate(m_gid, nVarId,
                                     TRUE /* shuffle */,
                                     TRUE /* deflate on */,
                                     nZLevel);
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return nullptr;
    }

    const char* pszFilter = CSLFetchNameValue(papszOptions, "FILTER");
    if( pszFilter )
    {
#ifdef NC_EFILTER
        const auto aosTokens(CPLStringList(CSLTokenizeString2(pszFilter, ",", 0)));
        if( !aosTokens.empty() )
        {
            const unsigned nFilterId = static_cast<unsigned>(
                CPLAtoGIntBig(aosTokens[0]));
            std::vector<unsigned> anParams;
            for( int i = 1; i < aosTokens.size(); ++i )
            {
                anParams.push_back(
                    static_cast<unsigned>(CPLAtoGIntBig(aosTokens[i])));
            }
            int ret = nc_def_var_filter(m_gid, nVarId, nFilterId,
                                        anParams.size(), anParams.data());
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return nullptr;
        }
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "netCDF 4.6 or later needed for FILTER option");
        return nullptr;
#endif
    }

    const bool bChecksum = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CHECKSUM", "FALSE"));
    if( bChecksum )
    {
        int ret = nc_def_var_fletcher32(m_gid, nVarId, TRUE);
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return nullptr;
    }

    return netCDFVariable::Create(m_poShared, m_gid, nVarId, dims, papszOptions,
                                  true);
}

/************************************************************************/
/*                          CreateAttribute()                           */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFGroup::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions)
{
    return netCDFAttribute::Create(m_poShared, m_gid, NC_GLOBAL,
                                   osName, anDimensions, oDataType,
                                   papszOptions);
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> netCDFGroup::GetGroupNames(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nSubGroups = 0;
    NCDF_ERR(nc_inq_grps(m_gid, &nSubGroups, nullptr));
    if( nSubGroups == 0 )
    {
        if( EQUAL(CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""),
                  "SAME_DIMENSION") )
        {
            std::vector<std::string> names;
            std::set<std::string> oSetDimNames;
            for( const auto& osArrayName: GetMDArrayNames(nullptr) )
            {
                const auto poArray = OpenMDArray(osArrayName, nullptr);
                const auto apoDims = poArray->GetDimensions();
                if( apoDims.size() == 1 )
                {
                    const auto osDimName = apoDims[0]->GetName();
                    if( oSetDimNames.find(osDimName) == oSetDimNames.end() )
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
    for( const auto& subgid: anSubGroupdsIds )
    {
        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_grpname(subgid, szName));
        names.emplace_back(szName);
    }
    return names;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> netCDFGroup::OpenGroup(const std::string& osName,
                                                  CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nSubGroups = 0;
    // This is weird but nc_inq_grp_ncid() succeeds on a single group file.
    NCDF_ERR(nc_inq_grps(m_gid, &nSubGroups, nullptr));
    if( nSubGroups == 0 )
    {
        if( EQUAL(CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""),
            "SAME_DIMENSION") )
        {
            const auto oCandidateGroupNames = GetGroupNames(papszOptions);
            for( const auto& osCandidateGroupName: oCandidateGroupNames )
            {
                if( osCandidateGroupName == osName )
                {
                    auto poThisGroup = std::make_shared<netCDFGroup>(m_poShared, m_gid);
                    return std::make_shared<netCDFVirtualGroupBySameDimension>(
                        poThisGroup, osName);
                }
            }
        }
        return nullptr;
    }
    int nSubGroupId = 0;
    if( nc_inq_grp_ncid(m_gid, osName.c_str(), &nSubGroupId) != NC_NOERR ||
        nSubGroupId <= 0 )
        return nullptr;
    return std::make_shared<netCDFGroup>(m_poShared, nSubGroupId);
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> netCDFGroup::GetMDArrayNames(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    int nVars = 0;
    NCDF_ERR(nc_inq_nvars(m_gid, &nVars));
    if( nVars == 0 )
        return {};
    std::vector<int> anVarIds(nVars);
    NCDF_ERR(nc_inq_varids(m_gid, nullptr, &anVarIds[0]));
    std::vector<std::string> names;
    names.reserve(nVars);
    const bool bAll = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    const bool bZeroDim = bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_ZERO_DIM", "NO"));
    const bool bCoordinates = bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_COORDINATES", "YES"));
    const bool bBounds = bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_BOUNDS", "YES"));
    const bool bIndexing = bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_INDEXING", "YES"));
    const bool bTime = bAll || CPLTestBool(CSLFetchNameValueDef(papszOptions, "SHOW_TIME", "YES"));
    std::set<std::string> ignoreList;
    if( !bCoordinates || !bBounds )
    {
        for( const auto& varid: anVarIds )
        {
            char **papszTokens = nullptr;
            if( !bCoordinates )
            {
                char *pszTemp = nullptr;
                if( NCDFGetAttr(m_gid, varid, "coordinates", &pszTemp) == CE_None )
                    papszTokens = CSLTokenizeString2(pszTemp, " ", 0);
                CPLFree(pszTemp);
            }
            if( !bBounds )
            {
                char *pszTemp = nullptr;
                if( NCDFGetAttr(m_gid, varid, "bounds", &pszTemp) == CE_None &&
                    pszTemp != nullptr && !EQUAL(pszTemp, "") )
                    papszTokens = CSLAddString( papszTokens, pszTemp );
                CPLFree(pszTemp);
            }
            for( char** iter = papszTokens; iter && iter[0]; ++iter )
                ignoreList.insert(*iter);
            CSLDestroy(papszTokens);
        }
    }

    const bool bGroupBySameDimension =
        EQUAL(CSLFetchNameValueDef(papszOptions, "GROUP_BY", ""),
              "SAME_DIMENSION");
    for( const auto& varid: anVarIds )
    {
        int nVarDims = 0;
        NCDF_ERR(nc_inq_varndims(m_gid, varid, &nVarDims));
        if( nVarDims == 0 &&! bZeroDim )
        {
            continue;
        }
        if( nVarDims == 1 && bGroupBySameDimension )
        {
            continue;
        }

        char szName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_varname(m_gid, varid, szName));
        if( !bIndexing && nVarDims == 1 )
        {
            int nDimId = 0;
            NCDF_ERR(nc_inq_vardimid(m_gid, varid, &nDimId));
            char szDimName[NC_MAX_NAME+1] = {};
            NCDF_ERR(nc_inq_dimname(m_gid, nDimId, szDimName));
            if( strcmp(szDimName, szName) == 0 )
            {
                continue;
            }
        }

        if( !bTime )
        {
            char *pszTemp = nullptr;
            bool bSkip = false;
            if( NCDFGetAttr(m_gid, varid, "standard_name", &pszTemp) == CE_None )
            {
                bSkip = pszTemp && strcmp(pszTemp, "time") == 0;
            }
            CPLFree(pszTemp);
            if( bSkip )
            {
                continue;
            }
        }

        if( ignoreList.find(szName) == ignoreList.end() )
        {
            names.emplace_back(szName);
        }
    }
    return names;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> netCDFGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hNCMutex);
    int nVarId = 0;
    if( nc_inq_varid(m_gid, osName.c_str(), &nVarId) != NC_NOERR )
        return nullptr;
    return netCDFVariable::Create(m_poShared, m_gid, nVarId,
                                  std::vector<std::shared_ptr<GDALDimension>>(),
                                  nullptr, false);
}

/************************************************************************/
/*                         GetDimensions()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> netCDFGroup::GetDimensions(CSLConstList) const
{
    CPLMutexHolderD(&hNCMutex);
    int nbDims = 0;
    NCDF_ERR(nc_inq_ndims(m_gid, &nbDims));
    if( nbDims == 0 )
        return {};
    std::vector<int> dimids(nbDims);
    NCDF_ERR(nc_inq_dimids(m_gid, &nbDims, &dimids[0], FALSE)); 
    std::vector<std::shared_ptr<GDALDimension>> res;
    for( int i = 0; i < nbDims; i++ )
    {
        res.emplace_back(std::make_shared<netCDFDimension>(
            m_poShared, m_gid, dimids[i], 0, std::string()));
    }
    return res;
}

/************************************************************************/
/*                         GetAttribute()                               */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFGroup::GetAttribute(const std::string& osName) const
{
    CPLMutexHolderD(&hNCMutex);
    int nAttId = -1;
    if( nc_inq_attid(m_gid, NC_GLOBAL, osName.c_str(), &nAttId) != NC_NOERR )
        return nullptr;
    return netCDFAttribute::Create(m_poShared, m_gid, NC_GLOBAL, osName);
}

/************************************************************************/
/*                         GetAttributes()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> netCDFGroup::GetAttributes(CSLConstList) const
{
    CPLMutexHolderD(&hNCMutex);
    std::vector<std::shared_ptr<GDALAttribute>> res;
    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(m_gid, NC_GLOBAL, &nbAttr));
    res.reserve(nbAttr);
    for( int i = 0; i < nbAttr; i++ )
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(m_gid, NC_GLOBAL, i, szAttrName));
        if( !EQUAL(szAttrName, "_NCProperties") )
        {
            res.emplace_back(netCDFAttribute::Create(
                m_poShared, m_gid, NC_GLOBAL, szAttrName));
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
/*                   netCDFVirtualGroupBySameDimension()                */
/************************************************************************/

netCDFVirtualGroupBySameDimension::netCDFVirtualGroupBySameDimension(
                                    const std::shared_ptr<netCDFGroup>& poGroup,
                                    const std::string& osDimName):
    GDALGroup(poGroup->GetName(), osDimName),
    m_poGroup(poGroup),
    m_osDimName(osDimName)
{
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> netCDFVirtualGroupBySameDimension::GetMDArrayNames(CSLConstList) const
{
    const auto srcNames = m_poGroup->GetMDArrayNames(nullptr);
    std::vector<std::string> names;
    for( const auto& srcName: srcNames )
    {
        auto poArray = m_poGroup->OpenMDArray(srcName, nullptr);
        if( poArray )
        {
            const auto apoArrayDims = poArray->GetDimensions();
            if( apoArrayDims.size() == 1 &&
                apoArrayDims[0]->GetName() == m_osDimName )
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

std::shared_ptr<GDALMDArray> netCDFVirtualGroupBySameDimension::OpenMDArray(
                                            const std::string& osName,
                                            CSLConstList papszOptions) const
{
    return m_poGroup->OpenMDArray(osName, papszOptions);
}

/************************************************************************/
/*                           netCDFDimension()                          */
/************************************************************************/

netCDFDimension::netCDFDimension(
                const std::shared_ptr<netCDFSharedResources>& poShared, int cfid,
                int dimid, size_t nForcedSize, const std::string& osType):
    GDALDimension(NCDFGetGroupFullName(cfid),
                  retrieveName(cfid, dimid),
                  osType, // type
                  std::string(), // direction
                  nForcedSize ? nForcedSize : retrieveSize(cfid, dimid)),
    m_poShared(poShared),
    m_gid(cfid),
    m_dimid(dimid)
{
    if( m_osType.empty() && nForcedSize == 0 )
    {
        auto var = std::dynamic_pointer_cast<netCDFVariable>(
            GetIndexingVariable());
        if( var )
        {
            const auto gid = var->GetGroupId();
            const auto varid = var->GetVarId();
            const auto varname = var->GetName().c_str();
            if( NCDFIsVarLongitude(gid, varid, varname) ||
                NCDFIsVarProjectionX(gid, varid, varname) )
            {
                m_osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                auto attrPositive = var->GetAttribute(CF_UNITS);
                if( attrPositive )
                {
                    const auto val = attrPositive->ReadAsString();
                    if( val )
                    {
                        if( EQUAL(val, CF_DEGREES_EAST) )
                        {
                            m_osDirection = "EAST";
                        }
                    }
                }
            }
            else if( NCDFIsVarLatitude(gid, varid, varname) ||
                     NCDFIsVarProjectionY(gid, varid, varname) )
            {
                m_osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                auto attrPositive = var->GetAttribute(CF_UNITS);
                if( attrPositive )
                {
                    const auto val = attrPositive->ReadAsString();
                    if( val )
                    {
                        if( EQUAL(val, CF_DEGREES_NORTH) )
                        {
                            m_osDirection = "NORTH";
                        }
                    }
                }
            }
            else if( NCDFIsVarVerticalCoord(gid, varid, varname) )
            {
                m_osType = GDAL_DIM_TYPE_VERTICAL;
                auto attrPositive = var->GetAttribute("positive");
                if( attrPositive )
                {
                    const auto val = attrPositive->ReadAsString();
                    if( val )
                    {
                        if( EQUAL(val, "up") )
                        {
                            m_osDirection = "UP";
                        }
                        else if( EQUAL(val, "down") )
                        {
                            m_osDirection = "DOWN";
                        }
                    }
                }
            }
            else if( NCDFIsVarTimeCoord(gid, varid, varname) )
            {
                m_osType = GDAL_DIM_TYPE_TEMPORAL;
            }
        }
    }
}

/************************************************************************/
/*                         GetIndexingVariable()                        */
/************************************************************************/

namespace {
    struct SetIsInGetIndexingVariable
    {
        netCDFSharedResources* m_poShared;

        explicit SetIsInGetIndexingVariable(netCDFSharedResources* poSharedResources): m_poShared(poSharedResources)
        {
            m_poShared->SetIsInGetIndexingVariable(true);
        }

        ~SetIsInGetIndexingVariable()
        {
            m_poShared->SetIsInGetIndexingVariable(false);
        }
    };
}

std::shared_ptr<GDALMDArray> netCDFDimension::GetIndexingVariable() const
{
    if( m_poShared->GetIsInIndexingVariable() )
        return nullptr;

    SetIsInGetIndexingVariable setterIsInGetIndexingVariable(m_poShared.get());

    CPLMutexHolderD(&hNCMutex);

    // First try to find a variable in this group with the same name as the
    // dimension
    int nVarId = 0;
    if( nc_inq_varid(m_gid, GetName().c_str(), &nVarId) == NC_NOERR )
    {
        int nDims = 0;
        NCDF_ERR(nc_inq_varndims(m_gid, nVarId, &nDims));
        int nVarType = NC_NAT;
        NCDF_ERR(nc_inq_vartype(m_gid, nVarId, &nVarType));
        if( nDims == 1 || (nDims == 2 && nVarType == NC_CHAR) )
        {
            int anDimIds[2] = {};
            NCDF_ERR(nc_inq_vardimid(m_gid, nVarId, anDimIds));
            if( anDimIds[0] == m_dimid )
            {
                if( nDims == 2 )
                {
                    // Check that there is no variable with the same of the
                    // second dimension.
                    char szExtraDim[NC_MAX_NAME+1] = {};
                    NCDF_ERR(nc_inq_dimname(m_gid, anDimIds[1], szExtraDim));
                    int nUnused;
                    if( nc_inq_varid(m_gid, szExtraDim, &nUnused) == NC_NOERR )
                        return nullptr;
                }

                return netCDFVariable::Create(m_poShared, m_gid, nVarId,
                    std::vector<std::shared_ptr<GDALDimension>>(),
                    nullptr, false);
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
    for(const auto& arrayName: arrayNames)
    {
        const auto poArray = oGroup.OpenMDArray(arrayName, nullptr);
        if( poArray )
        {
            const auto poArrayNC = std::dynamic_pointer_cast<netCDFVariable>(poArray);
            const auto poCoordinates = poArray->GetAttribute("coordinates");
            if( poArrayNC && poCoordinates &&
                poCoordinates->GetDataType().GetClass() == GEDTC_STRING )
            {
                const CPLStringList aosCoordinates(
                    CSLTokenizeString2(poCoordinates->ReadAsString(), " ", 0));
                const auto apoArrayDims = poArray->GetDimensions();
                if( apoArrayDims.size() ==
                    static_cast<size_t>(aosCoordinates.size()) )
                {
                    for(size_t i = 0; i < apoArrayDims.size(); ++i)
                    {
                        const auto& poArrayDim =  apoArrayDims[i];
                        const auto poArrayDimNC = std::dynamic_pointer_cast<
                        netCDFDimension>(poArrayDim);
                        if( poArrayDimNC &&
                            poArrayDimNC->m_gid == m_gid &&
                            poArrayDimNC->m_dimid == m_dimid )
                        {
                            int nIndexingVarGroupId = -1;
                            int nIndexingVarId = -1;
                            if( NCDFResolveVar(poArrayNC->GetGroupId(),
                                               aosCoordinates[i],
                                               &nIndexingVarGroupId,
                                               &nIndexingVarId,
                                               false) == CE_None )
                            {
                                return netCDFVariable::Create(m_poShared,
                                    nIndexingVarGroupId, nIndexingVarId,
                                    std::vector<std::shared_ptr<GDALDimension>>(),
                                    nullptr, false);
                            }
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

/************************************************************************/
/*                          netCDFVariable()                            */
/************************************************************************/

netCDFVariable::netCDFVariable(const std::shared_ptr<netCDFSharedResources>& poShared,
                               int gid, int varid,
                               const std::vector<std::shared_ptr<GDALDimension>>& dims,
                               CSLConstList papszOptions):
    GDALAbstractMDArray(NCDFGetGroupFullName(gid), retrieveName(gid, varid)),
    GDALMDArray(NCDFGetGroupFullName(gid), retrieveName(gid, varid)),
    m_poShared(poShared),
    m_gid(gid),
    m_varid(varid),
    m_dims(dims)
{
    NCDF_ERR(nc_inq_varndims(m_gid, m_varid, &m_nDims));
    NCDF_ERR(nc_inq_vartype(m_gid, m_varid, &m_nVarType));
    if( m_nDims == 2 && m_nVarType == NC_CHAR )
    {
        int anDimIds[2] = {};
        NCDF_ERR(nc_inq_vardimid(m_gid, m_varid, &anDimIds[0]));

        // Check that there is no variable with the same of the
        // second dimension.
        char szExtraDim[NC_MAX_NAME+1] = {};
        NCDF_ERR(nc_inq_dimname(m_gid, anDimIds[1], szExtraDim));
        int nUnused;
        if( nc_inq_varid(m_gid, szExtraDim, &nUnused) != NC_NOERR )
        {
            NCDF_ERR(nc_inq_dimlen(m_gid, anDimIds[1], &m_nTextLength));
        }
    }

    int nShuffle = 0;
    int nDeflate = 0;
    int nDeflateLevel = 0;
    if( nc_inq_var_deflate(m_gid, m_varid, &nShuffle, &nDeflate, &nDeflateLevel) == NC_NOERR )
    {
        if( nDeflate )
        {
            m_aosStructuralInfo.SetNameValue("COMPRESS", "DEFLATE");
        }
    }
    auto unit = netCDFVariable::GetAttribute(CF_UNITS);
    if( unit )
    {
        const char* pszVal = unit->ReadAsString();
        if( pszVal )
            m_osUnit = pszVal;
    }
    m_bWriteGDALTags = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_GDAL_TAGS", "YES"));
}

/************************************************************************/
/*                             GetDimensions()                          */
/************************************************************************/

const std::vector<std::shared_ptr<GDALDimension>>& netCDFVariable::GetDimensions() const
{
    if( m_nDims == 0 || !m_dims.empty() )
        return m_dims;
    CPLMutexHolderD(&hNCMutex);
    std::vector<int> anDimIds(m_nDims);
    NCDF_ERR(nc_inq_vardimid(m_gid, m_varid, &anDimIds[0]));
    if( m_nDims == 2 && m_nVarType == NC_CHAR && m_nTextLength > 0 )
        anDimIds.resize(1);
    m_dims.reserve(m_nDims);
    for( const auto& dimid: anDimIds )
    {
        m_dims.emplace_back(std::make_shared<netCDFDimension>(
            m_poShared,
            m_poShared->GetBelongingGroupOfDim(m_gid, dimid),
            dimid, 0, std::string()));
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
    //First enquire and check that the number of fields is 2
    size_t nfields = 0;
    size_t compoundsize = 0;
    char szName[NC_MAX_NAME + 1] = {};
    if( nc_inq_compound(gid, nVarType, szName, &compoundsize, &nfields) != NC_NOERR)
    {
        return GDT_Unknown;
    }

    if (nfields != 2 || !STARTS_WITH_CI(szName, "complex"))
    {
        return GDT_Unknown;
    }

    //Now check that that two types are the same in the struct.
    nc_type field_type1, field_type2;
    int field_dims1, field_dims2;
    if ( nc_inq_compound_field(gid, nVarType, 0, nullptr, nullptr, &field_type1, &field_dims1, nullptr) != NC_NOERR)
    {
        return GDT_Unknown;
    }

    if ( nc_inq_compound_field(gid, nVarType, 0, nullptr, nullptr, &field_type2, &field_dims2, nullptr) != NC_NOERR)
    {
        return GDT_Unknown;
    }

    if ((field_type1 != field_type2) || (field_dims1 != field_dims2) || (field_dims1 != 0))
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
                          std::unique_ptr<GDALExtendedDataType>& dt,
                          bool& bPerfectDataTypeMatch);

static bool GetCompoundDataType(int gid, int nVarType,
                                std::unique_ptr<GDALExtendedDataType>& dt,
                                bool& bPerfectDataTypeMatch)
{
    size_t nfields = 0;
    size_t compoundsize = 0;
    char szName[NC_MAX_NAME + 1] = {};
    if( nc_inq_compound(gid, nVarType, szName, &compoundsize, &nfields) != NC_NOERR)
    {
        return false;
    }
    bPerfectDataTypeMatch = true;
    std::vector<std::unique_ptr<GDALEDTComponent>> comps;
    for( size_t i = 0; i < nfields; i++ )
    {
        nc_type field_type = 0;
        int field_dims = 0;
        size_t field_offset = 0;
        char field_name[NC_MAX_NAME + 1] = {};
        if ( nc_inq_compound_field(gid, nVarType, static_cast<int>(i),
                                   field_name, &field_offset,
                                   &field_type, &field_dims, nullptr) != NC_NOERR)
        {
            return false;
        }
        if( field_dims != 0 )
        {
            // We don't support that
            return false;
        }
        std::unique_ptr<GDALExtendedDataType> subDt;
        bool bSubPerfectDataTypeMatch = false;
        if( !BuildDataType(gid, -1, field_type, subDt, bSubPerfectDataTypeMatch) )
        {
            return false;
        }
        if( !bSubPerfectDataTypeMatch )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Non native GDAL type found in a component of a compound type");
            return false;
        }
        auto comp = std::unique_ptr<GDALEDTComponent>(
            new GDALEDTComponent(std::string(field_name), field_offset, *subDt));
        comps.emplace_back(std::move(comp));
    }
    dt.reset(new GDALExtendedDataType(GDALExtendedDataType::Create(
        szName, compoundsize, std::move(comps))));

    return dt->GetClass() == GEDTC_COMPOUND;
}

/************************************************************************/
/*                            BuildDataType()                           */
/************************************************************************/

static bool BuildDataType(int gid, int varid, int nVarType,
                          std::unique_ptr<GDALExtendedDataType>& dt,
                          bool& bPerfectDataTypeMatch)
{
    GDALDataType eDataType = GDT_Unknown;
    bPerfectDataTypeMatch = false;
    if (NCDFIsUserDefinedType(gid, nVarType))
    {
        nc_type nBaseType = NC_NAT;
        int eClass = 0;
        nc_inq_user_type(gid, nVarType, nullptr, nullptr,
                         &nBaseType, nullptr, &eClass);
        if( eClass == NC_COMPOUND )
        {
            eDataType = GetComplexDataType(gid, nVarType);
            if( eDataType != GDT_Unknown )
            {
                bPerfectDataTypeMatch = true;
                dt.reset(new GDALExtendedDataType(GDALExtendedDataType::Create(eDataType)));
                return true;
            }
            else if( GetCompoundDataType(gid, nVarType, dt, bPerfectDataTypeMatch) )
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
        else if( eClass == NC_ENUM )
        {
            nVarType = nBaseType;
        }
        else if( eClass == NC_VLEN )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VLen data type not supported");
            return false;
        }
        else if( eClass == NC_OPAQUE )
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

    if( nVarType == NC_STRING )
    {
        bPerfectDataTypeMatch = true;
        dt.reset(new GDALExtendedDataType(GDALExtendedDataType::CreateString()));
        return true;
    }
    else
    {
        if( nVarType == NC_BYTE )
        {
            char *pszTemp = nullptr;
            bool bSignedData = true;
            if( varid >= 0 && NCDFGetAttr(gid, varid, "_Unsigned", &pszTemp) == CE_None )
            {
                if( EQUAL(pszTemp, "true") )
                    bSignedData = false;
                else if( EQUAL(pszTemp, "false") )
                    bSignedData = true;
                CPLFree(pszTemp);
            }
            if( !bSignedData )
            {
                eDataType = GDT_Byte;
                bPerfectDataTypeMatch = true;
            }
            else
            {
                eDataType = GDT_Int16;
            }
        }
        else if( nVarType == NC_CHAR )
        {
            // Not sure of this
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Byte;
        }
        else if( nVarType == NC_SHORT )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Int16;
        }
        else if( nVarType == NC_INT )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Int32;
        }
        else if( nVarType == NC_FLOAT )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Float32;
        }
        else if( nVarType == NC_DOUBLE )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Float64;
        }
        else if( nVarType == NC_UBYTE )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_Byte;
        }
        else if( nVarType == NC_USHORT )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_UInt16;
        }
        else if( nVarType == NC_UINT )
        {
            bPerfectDataTypeMatch = true;
            eDataType = GDT_UInt32;
        }
        else if( nVarType == NC_INT64 )
            eDataType = GDT_Float64; // approximation
        else if( nVarType == NC_UINT64 )
            eDataType = GDT_Float64; // approximation
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
    if( m_dt )
        return *m_dt;
    CPLMutexHolderD(&hNCMutex);

    if( m_nDims == 2 && m_nVarType == NC_CHAR && m_nTextLength > 0 )
    {
        m_dt.reset(new GDALExtendedDataType(
            GDALExtendedDataType::CreateString(m_nTextLength)));
    }
    else
    {
        m_dt.reset(new GDALExtendedDataType(GDALExtendedDataType::Create(GDT_Unknown)));

        BuildDataType(m_gid, m_varid, m_nVarType, m_dt, m_bPerfectDataTypeMatch);
    }
    return *m_dt;
}

/************************************************************************/
/*                              SetUnit()                               */
/************************************************************************/

bool netCDFVariable::SetUnit(const std::string& osUnit)
{
    if( osUnit.empty() )
    {
        nc_del_att(m_gid, m_varid, CF_UNITS);
        return true;
    }
    auto poUnits(GetAttribute(CF_UNITS));
    if( !poUnits )
    {
        poUnits = CreateAttribute(CF_UNITS, {},
                                  GDALExtendedDataType::CreateString(),
                                  nullptr);
        if( !poUnits )
            return false;
    }
    return poUnits->Write(osUnit.c_str());
}

/************************************************************************/
/*                            GetSpatialRef()                           */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> netCDFVariable::GetSpatialRef() const
{
    if( m_bSRSRead )
        return m_poSRS;

    m_bSRSRead = true;
    netCDFDataset poDS;
    poDS.ReadAttributes(m_gid, m_varid);
    int iDimX = 0;
    int iDimY = 0;
    int iCount = 1;
    for( const auto& poDim: GetDimensions() )
    {
        if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X )
            iDimX = iCount;
        else if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y )
            iDimY = iCount;
        poDS.papszDimName.AddString(poDim->GetName().c_str());
        iCount ++;
    }
    if( (iDimX == 0 || iDimY == 0) && GetDimensionCount() >= 2 )
    {
        iDimX = static_cast<int>(GetDimensionCount());
        iDimY = iDimX - 1;
    }
    poDS.SetProjectionFromVar(m_gid, m_varid, true);
    auto poSRS = poDS.GetSpatialRef();
    if( poSRS )
    {
        m_poSRS.reset(poSRS->Clone());
        if( iDimX > 0 && iDimY > 0 )
        {
            if( m_poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{ 2, 1 } )
                m_poSRS->SetDataAxisToSRSAxisMapping({ iDimY, iDimX });
            else
                m_poSRS->SetDataAxisToSRSAxisMapping({ iDimX, iDimY });
        }
    }

    return m_poSRS;
}

/************************************************************************/
/*                            SetSpatialRef()                           */
/************************************************************************/

static void WriteDimAttr(std::shared_ptr<GDALMDArray> poVar,
                           const char* pszAttrName,
                           const char* pszAttrValue)
{
    auto poAttr = poVar->GetAttribute(pszAttrName);
    if( poAttr )
    {
        const char* pszVal = poAttr->ReadAsString();
        if( pszVal && !EQUAL(pszVal, pszAttrValue) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "Variable %s has a %s which is %s and not %s",
                        poVar->GetName().c_str(),
                        pszAttrName,
                        pszVal,
                        pszAttrValue);
        }
    }
    else
    {
        poAttr = poVar->CreateAttribute(
            pszAttrName, {},
            GDALExtendedDataType::CreateString(),
            nullptr);
        if( poAttr )
            poAttr->Write(pszAttrValue);
    }
}

static void WriteDimAttrs(std::shared_ptr<GDALDimension> dim,
                           const char* pszStandardName,
                           const char* pszLongName,
                           const char* pszUnits)
{
    auto poVar = dim->GetIndexingVariable();
    if( poVar )
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

bool netCDFVariable::SetSpatialRef(const OGRSpatialReference* poSRS)
{
    m_bSRSRead = false;
    m_poSRS.reset();

    if( poSRS == nullptr )
    {
        nc_del_att(m_gid, m_varid, CF_GRD_MAPPING);
        return true;
    }

    char *pszCFProjection = nullptr;
    int nSRSVarId = NCDFWriteSRSVariable(m_gid, poSRS, &pszCFProjection,
                                         m_bWriteGDALTags);
    if( nSRSVarId < 0 || pszCFProjection == nullptr )
        return false;

    NCDF_ERR(nc_put_att_text(m_gid, m_varid, CF_GRD_MAPPING,
                                strlen(pszCFProjection),
                                pszCFProjection));
    CPLFree(pszCFProjection);

    auto apoDims = GetDimensions();
    if( poSRS->IsProjected() )
    {
        bool bWriteX = false;
        bool bWriteY = false;
        const char *pszUnits = NCDFGetProjectedCFUnit(poSRS);
        for( const auto& poDim: apoDims )
        {
            const char* pszStandardName = nullptr;
            const char* pszLongName = nullptr;
            if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X ||
                EQUAL(poDim->GetName().c_str(), CF_PROJ_X_VAR_NAME) )
            {
                pszStandardName = CF_PROJ_X_COORD;
                pszLongName = CF_PROJ_X_COORD_LONG_NAME;
                bWriteX = true;
            }
            else if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y ||
                EQUAL(poDim->GetName().c_str(), CF_PROJ_Y_VAR_NAME) )
            {
                pszStandardName = CF_PROJ_Y_COORD;
                pszLongName = CF_PROJ_Y_COORD_LONG_NAME;
                bWriteY = true;
            }
            if( pszStandardName && pszLongName )
            {
                WriteDimAttrs(poDim, pszStandardName, pszLongName, pszUnits);
            }
        }
        if( !bWriteX && !bWriteY &&
            apoDims.size() >= 2 &&
            apoDims[apoDims.size()-2]->GetType().empty() &&
            apoDims[apoDims.size()-1]->GetType().empty() &&
            apoDims[apoDims.size()-2]->GetIndexingVariable() &&
            apoDims[apoDims.size()-1]->GetIndexingVariable() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Dimensions of variable %s have no type declared. "
                     "Assuming the last one is X, and the preceding one Y",
                     GetName().c_str());
            WriteDimAttrs(apoDims[apoDims.size()-1],
                          CF_PROJ_X_COORD, CF_PROJ_X_COORD_LONG_NAME, pszUnits);
            WriteDimAttrs(apoDims[apoDims.size()-2],
                          CF_PROJ_Y_COORD, CF_PROJ_Y_COORD_LONG_NAME, pszUnits);
        }
    }
    else if( poSRS->IsGeographic() )
    {
        bool bWriteX = false;
        bool bWriteY = false;
        for( const auto& poDim: apoDims )
        {
            const char* pszStandardName = nullptr;
            const char* pszLongName = nullptr;
            const char* pszUnits = "";
            if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X ||
                EQUAL(poDim->GetName().c_str(), CF_LONGITUDE_VAR_NAME) )
            {
                pszStandardName = CF_LONGITUDE_STD_NAME;
                pszLongName = CF_LONGITUDE_LNG_NAME;
                pszUnits = CF_DEGREES_EAST;
                bWriteX = true;
            }
            else if( poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y ||
                EQUAL(poDim->GetName().c_str(), CF_LATITUDE_VAR_NAME) )
            {
                pszStandardName = CF_LATITUDE_STD_NAME;
                pszLongName = CF_LATITUDE_LNG_NAME;
                pszUnits = CF_DEGREES_NORTH;
                bWriteY = true;
            }
            if( pszStandardName && pszLongName )
            {
                WriteDimAttrs(poDim, pszStandardName, pszLongName, pszUnits);
            }
        }
        if( !bWriteX && !bWriteY &&
            apoDims.size() >= 2 &&
            apoDims[apoDims.size()-2]->GetType().empty() &&
            apoDims[apoDims.size()-1]->GetType().empty() &&
            apoDims[apoDims.size()-2]->GetIndexingVariable() &&
            apoDims[apoDims.size()-1]->GetIndexingVariable() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Dimensions of variable %s have no type declared. "
                     "Assuming the last one is longitude, "
                     "and the preceding one latitude", GetName().c_str());
            WriteDimAttrs(apoDims[apoDims.size()-1],
                          CF_LONGITUDE_STD_NAME,
                          CF_LONGITUDE_LNG_NAME, CF_DEGREES_EAST);
            WriteDimAttrs(apoDims[apoDims.size()-2],
                          CF_LATITUDE_STD_NAME,
                          CF_LATITUDE_LNG_NAME, CF_DEGREES_NORTH);
        }
    }

    return true;
}

/************************************************************************/
/*                             GetNCTypeSize()                          */
/************************************************************************/

static size_t GetNCTypeSize(const GDALExtendedDataType& dt,
                            bool bPerfectDataTypeMatch,
                            int nAttType)
{
    auto nElementSize = dt.GetSize();
    if( !bPerfectDataTypeMatch )
    {
        if( nAttType == NC_BYTE )
        {
            CPLAssert( dt.GetNumericDataType() == GDT_Int16 );
            nElementSize = sizeof(signed char);
        }
        else if( nAttType == NC_INT64 )
        {
            CPLAssert( dt.GetNumericDataType() == GDT_Float64 );
            nElementSize = sizeof(GInt64);
        }
        else if( nAttType == NC_UINT64 )
        {
            CPLAssert( dt.GetNumericDataType() == GDT_Float64 );
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

static void ConvertNCStringsToCPLStrings(GByte* pBuffer,
                                         const GDALExtendedDataType& dt)
{
    switch( dt.GetClass() )
    {
        case GEDTC_STRING:
        {
            char* pszStr;
            // cppcheck-suppress pointerSize
            memcpy(&pszStr, pBuffer, sizeof(char*));
            if( pszStr )
            {
                char* pszNewStr = VSIStrdup(pszStr);
                nc_free_string(1, &pszStr);
                // cppcheck-suppress pointerSize
                memcpy(pBuffer, &pszNewStr, sizeof(char*));
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            break;
        }

        case GEDTC_COMPOUND:
        {
            const auto& comps = dt.GetComponents();
            for( const auto& comp: comps )
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

static void FreeNCStrings(GByte* pBuffer, const GDALExtendedDataType& dt)
{
    switch( dt.GetClass() )
    {
        case GEDTC_STRING:
        {
            char* pszStr;
            // cppcheck-suppress pointerSize
            memcpy(&pszStr, pBuffer, sizeof(char*));
            if( pszStr )
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
            const auto& comps = dt.GetComponents();
            for( const auto& comp: comps )
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

namespace {
template<typename T> struct GetGByteType {};
template<> struct GetGByteType<void*>
{
    typedef GByte* type;
};
template<> struct GetGByteType<const void*>
{
    typedef const GByte* type;
};
}

template< typename BufferType,
          typename NCGetPutVar1FuncType,
          typename ReadOrWriteOneElementType >
bool netCDFVariable::IReadWriteGeneric(const size_t* arrayStartIdx,
                                  const size_t* count,
                                  const GInt64* arrayStep,
                                  const GPtrDiff_t* bufferStride,
                                  const GDALExtendedDataType& bufferDataType,
                                  BufferType buffer,
                                  NCGetPutVar1FuncType NCGetPutVar1Func,
                                  ReadOrWriteOneElementType ReadOrWriteOneElement) const
{
    CPLAssert(m_nDims > 0);
    std::vector<size_t> array_idx(m_nDims);
    std::vector<size_t> stack_count_iters(m_nDims - 1);
    typedef typename GetGByteType<BufferType>::type GBytePtrType;
    std::vector<GBytePtrType> stack_ptr(m_nDims);
    std::vector<GPtrDiff_t> ptr_inc;
    ptr_inc.reserve(m_nDims);
    const auto& eArrayEDT = GetDataType();
    const bool bSameDT = m_bPerfectDataTypeMatch && eArrayEDT == bufferDataType;
    const auto nBufferDTSize = bufferDataType.GetSize();
    for( int i = 0; i < m_nDims; i++ )
    {
        ptr_inc.push_back(bufferStride[i] * nBufferDTSize);
    }
    const auto nDimsMinus1 = m_nDims - 1;
    stack_ptr[0] = static_cast<GBytePtrType>(buffer);

    auto lambdaLastDim = [&](GBytePtrType ptr)
    {
        array_idx[nDimsMinus1] = arrayStartIdx[nDimsMinus1];
        size_t nIters = count[nDimsMinus1];
        while(true)
        {
            if( bSameDT )
            {
                int ret = NCGetPutVar1Func(m_gid, m_varid, array_idx.data(), ptr);
                NCDF_ERR(ret);
                if( ret != NC_NOERR )
                    return false;
            }
            else
            {
                if( !(this->*ReadOrWriteOneElement)(
                                         eArrayEDT, bufferDataType,
                                         array_idx.data(), ptr) )
                    return false;
            }
            if( (--nIters) == 0 )
                break;
            ptr += ptr_inc[nDimsMinus1];
            // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
            // thus automatic conversion from negative to big unsigned might
            // occur
            array_idx[nDimsMinus1] =
                CPLUnsanitizedAdd<size_t>(
                    array_idx[nDimsMinus1],
                    static_cast<GPtrDiff_t>(arrayStep[nDimsMinus1]));
        }
        return true;
    };

    if( m_nDims == 1 )
    {
        return lambdaLastDim(stack_ptr[0]);
    }
    else if( m_nDims == 2)
    {
        auto nIters = count[0];
        array_idx[0] = arrayStartIdx[0];
        while(true)
        {
            if( !lambdaLastDim(stack_ptr[0]) )
                return false;
            if( (--nIters) == 0 )
                break;
            stack_ptr[0] += ptr_inc[0];
            // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
            // thus automatic conversion from negative to big unsigned might
            // occur
            array_idx[0] = CPLUnsanitizedAdd<size_t>(
                array_idx[0], static_cast<GPtrDiff_t>(arrayStep[0]));
        }
    }
    else if( m_nDims == 3)
    {
        stack_count_iters[0] = count[0];
        array_idx[0] = arrayStartIdx[0];
        while(true)
        {
            auto nIters = count[1];
            array_idx[1] = arrayStartIdx[1];
            stack_ptr[1] = stack_ptr[0];
            while(true)
            {
                if( !lambdaLastDim(stack_ptr[1]) )
                    return false;
                if( (--nIters) == 0 )
                    break;
                stack_ptr[1] += ptr_inc[1];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
                // thus automatic conversion from negative to big unsigned might
                // occur
                array_idx[1] = CPLUnsanitizedAdd<size_t>(
                    array_idx[1], static_cast<GPtrDiff_t>(arrayStep[1]));
            }
            if( (--stack_count_iters[0]) == 0 )
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
        if( dimIdx == nDimsMinus1 - 1 )
        {
            array_idx[dimIdx] = arrayStartIdx[dimIdx];
            auto nIters = count[dimIdx];
            while(true)
            {
                if( !(lambdaLastDim(stack_ptr[dimIdx])) )
                    return false;
                if( (--nIters) == 0 )
                    break;
                stack_ptr[dimIdx] += ptr_inc[dimIdx];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
                // thus automatic conversion from negative to big unsigned might
                // occur
                array_idx[dimIdx] = CPLUnsanitizedAdd<size_t>(
                    array_idx[dimIdx], static_cast<GPtrDiff_t>(arrayStep[dimIdx]));
            }
            // If there was a test if( dimIdx > 0 ), that would be valid for nDims == 2
            goto lbl_return_to_caller;
        }
        else
        {
            array_idx[dimIdx] = arrayStartIdx[dimIdx];
            stack_count_iters[dimIdx] = count[dimIdx];
            while(true)
            {
                // Simulate a recursive call to the next dimension
                // Implicitly save back count and ptr
                dimIdx ++;
                stack_ptr[dimIdx] = stack_ptr[dimIdx-1];
                goto lbl_start;
lbl_return_to_caller:
                dimIdx --;
                if( (--stack_count_iters[dimIdx]) == 0 )
                    break;
                stack_ptr[dimIdx] += ptr_inc[dimIdx];
                // CPLUnsanitizedAdd needed as arrayStep[] might be negative, and
                // thus automatic conversion from negative to big unsigned might
                // occur
                array_idx[dimIdx] = CPLUnsanitizedAdd<size_t>(
                    array_idx[dimIdx], static_cast<GPtrDiff_t>(arrayStep[dimIdx]));
            }
            if( dimIdx > 0 )
                goto lbl_return_to_caller;
        }
    }

    return true;
}

/************************************************************************/
/*                          CheckNumericDataType()                      */
/************************************************************************/

static bool CheckNumericDataType(const GDALExtendedDataType& dt)
{
    const auto klass = dt.GetClass();
    if( klass == GEDTC_NUMERIC )
        return dt.GetNumericDataType() != GDT_Unknown;
    if( klass == GEDTC_STRING )
        return false;
    CPLAssert( klass == GEDTC_COMPOUND );
    const auto& comps = dt.GetComponents();
    for( const auto& comp: comps )
    {
        if( !CheckNumericDataType(comp->GetType()) )
            return false;
    }
    return true;
}

/************************************************************************/
/*                            IReadWrite()                              */
/************************************************************************/

template< typename BufferType,
          typename NCGetPutVar1FuncType,
          typename NCGetPutVaraFuncType,
          typename NCGetPutVarmFuncType,
          typename ReadOrWriteOneElementType >
bool netCDFVariable::IReadWrite(const bool bIsRead,
                                const GUInt64* arrayStartIdx,
                                  const size_t* count,
                                  const GInt64* arrayStep,
                                  const GPtrDiff_t* bufferStride,
                                  const GDALExtendedDataType& bufferDataType,
                                  BufferType buffer,
                                  NCGetPutVar1FuncType NCGetPutVar1Func,
                                  NCGetPutVaraFuncType NCGetPutVaraFunc,
                                  NCGetPutVarmFuncType NCGetPutVarmFunc,
                                  ReadOrWriteOneElementType ReadOrWriteOneElement) const
{
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(false);

    const auto& eDT = GetDataType();
    std::vector<size_t> startp;
    startp.reserve(m_nDims);
    bool bUseSlowPath = !m_bPerfectDataTypeMatch &&
        !(bIsRead &&
          bufferDataType.GetClass() == GEDTC_NUMERIC &&
          eDT.GetClass() == GEDTC_NUMERIC &&
          bufferDataType.GetSize() >= eDT.GetSize());
    for( int i = 0; i < m_nDims; i++ )
    {
#if SIZEOF_VOIDP == 4
        if( arrayStartIdx[i] > std::numeric_limits<size_t>::max() )
            return false;
#endif
        startp.push_back(static_cast<size_t>(arrayStartIdx[i]));

#if SIZEOF_VOIDP == 4
        if( arrayStep[i] < std::numeric_limits<ptrdiff_t>::min() ||
            arrayStep[i] > std::numeric_limits<ptrdiff_t>::max() )
        {
            return false;
        }
#endif

        if( count[i] != 1 && arrayStep[i] <= 0 )
            bUseSlowPath = true; // netCDF rejects negative or NULL strides

        if( bufferStride[i] < 0 )
            bUseSlowPath = true; // and it seems to silently cast to size_t imapp
    }

    if( eDT.GetClass() == GEDTC_STRING &&
        bufferDataType.GetClass() == GEDTC_STRING &&
        m_nVarType == NC_STRING )
    {
        if( m_nDims == 0 )
        {
            return (this->*ReadOrWriteOneElement)(eDT, bufferDataType,
                                nullptr, buffer);
        }

        return IReadWriteGeneric(
                            startp.data(), count, arrayStep,
                            bufferStride, bufferDataType, buffer,
                            NCGetPutVar1Func, ReadOrWriteOneElement);
    }

    if( !CheckNumericDataType(eDT) )
        return false;
    if( !CheckNumericDataType(bufferDataType) )
        return false;

    if( m_nDims == 0 )
    {
        return (this->*ReadOrWriteOneElement)(eDT, bufferDataType,
                              nullptr, buffer);
    }

    if( !bUseSlowPath &&
        ((GDALDataTypeIsComplex(bufferDataType.GetNumericDataType()) ||
         bufferDataType.GetClass() == GEDTC_COMPOUND) &&
        bufferDataType == eDT) )
    {
        // nc_get_varm() not supported for non-atomic types.
        ptrdiff_t nExpectedBufferStride = 1;
        for( int i = m_nDims; i != 0; )
        {
            --i;
            if( count[i] != 1 &&
                (arrayStep[i] != 1 || bufferStride[i] != nExpectedBufferStride) )
            {
                bUseSlowPath = true;
                break;
            }
            nExpectedBufferStride *= count[i];
        }
        if( !bUseSlowPath )
        {
            int ret = NCGetPutVaraFunc(m_gid, m_varid, startp.data(), count, buffer);
            NCDF_ERR(ret);
            return ret == NC_NOERR;
        }
    }

    if( bUseSlowPath || 
        bufferDataType.GetClass() == GEDTC_COMPOUND ||
        eDT.GetClass() == GEDTC_COMPOUND ||
        (!bIsRead && bufferDataType.GetNumericDataType() != eDT.GetNumericDataType()) ||
        (bIsRead && bufferDataType.GetSize() < eDT.GetSize()) )
    {
        return IReadWriteGeneric(
                            startp.data(), count, arrayStep,
                            bufferStride, bufferDataType, buffer,
                            NCGetPutVar1Func, ReadOrWriteOneElement);
    }

    bUseSlowPath = false;
    ptrdiff_t nExpectedBufferStride = 1;
    for( int i = m_nDims; i != 0; )
    {
        --i;
        if( count[i] != 1 &&
            (arrayStep[i] != 1 || bufferStride[i] != nExpectedBufferStride) )
        {
            bUseSlowPath = true;
            break;
        }
        nExpectedBufferStride *= count[i];
    }
    if( !bUseSlowPath )
    {
        // nc_get_varm() is terribly inefficient, so use nc_get_vara()
        // when possible.
        int ret = NCGetPutVaraFunc(m_gid, m_varid, startp.data(), count, buffer);
        if( ret != NC_NOERR )
            return false;
        if( bIsRead &&
            (!m_bPerfectDataTypeMatch ||
             bufferDataType.GetNumericDataType() != eDT.GetNumericDataType()) )
        {
            // If the buffer data type is "larger" or of the same size as the
            // native data type, we can do a in-place conversion
            GByte* pabyBuffer = static_cast<GByte*>(const_cast<void*>(buffer));
            CPLAssert( bufferDataType.GetSize() >= eDT.GetSize() );
            const auto nDTSize = eDT.GetSize();
            const auto nBufferDTSize = bufferDataType.GetSize();
            if( !m_bPerfectDataTypeMatch && (m_nVarType == NC_CHAR || m_nVarType == NC_BYTE) )
            {
                // native NC type translates into GDAL data type of larger size
                for( ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i )
                {
                    GByte abySrc[2];
                    abySrc[0] = *(pabyBuffer + i);
                    ConvertNCToGDAL(&abySrc[0]);
                    GDALExtendedDataType::CopyValue(
                        &abySrc[0], eDT,
                        pabyBuffer + i * nBufferDTSize, bufferDataType);
                }
            }
            else if( !m_bPerfectDataTypeMatch )
            {
                // native NC type translates into GDAL data type of same size
                CPLAssert( m_nVarType == NC_INT64 || m_nVarType == NC_UINT64 );
                for( ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i )
                {
                    ConvertNCToGDAL(pabyBuffer + i * nDTSize);
                    GDALExtendedDataType::CopyValue(
                        pabyBuffer + i * nDTSize, eDT,
                        pabyBuffer + i * nBufferDTSize, bufferDataType);
                }
            }
            else
            {
                for( ptrdiff_t i = nExpectedBufferStride - 1; i >= 0; --i )
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
        if( bufferDataType.GetNumericDataType() != eDT.GetNumericDataType() )
        {
            return IReadWriteGeneric(
                                startp.data(), count, arrayStep,
                                bufferStride, bufferDataType, buffer,
                                NCGetPutVar1Func, ReadOrWriteOneElement);
        }
        std::vector<ptrdiff_t> stridep;
        stridep.reserve(m_nDims);
        std::vector<ptrdiff_t> imapp;
        imapp.reserve(m_nDims);
        for( int i = 0; i < m_nDims; i++ )
        {
            stridep.push_back(static_cast<ptrdiff_t>(count[i] == 1 ? 1 : arrayStep[i]));
            imapp.push_back(static_cast<ptrdiff_t>(bufferStride[i]));
        }

        if( !m_poShared->GetImappIsInElements() )
        {
            const size_t nMul = GetNCTypeSize(eDT,
                                            m_bPerfectDataTypeMatch, m_nVarType);
            for( int i = 0; i < m_nDims; ++i )
            {
                imapp[i] = static_cast<ptrdiff_t>(imapp[i] * nMul);
            }
        }
        int ret = NCGetPutVarmFunc(m_gid, m_varid, startp.data(), count,
                            stridep.data(), imapp.data(),
                            buffer);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }
}

/************************************************************************/
/*                          ConvertNCToGDAL()                           */
/************************************************************************/

void netCDFVariable::ConvertNCToGDAL(GByte* buffer) const
{
    if( !m_bPerfectDataTypeMatch )
    {
        if( m_nVarType == NC_CHAR || m_nVarType == NC_BYTE )
        {
            short s = reinterpret_cast<signed char*>(buffer)[0];
            memcpy(buffer, &s, sizeof(s));
        }
        else if( m_nVarType == NC_INT64 )
        {
            double v = static_cast<double>(
                reinterpret_cast<GInt64*>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
        else if( m_nVarType == NC_UINT64 )
        {
            double v = static_cast<double>(
                reinterpret_cast<GUInt64*>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
    }
}

/************************************************************************/
/*                           ReadOneElement()                           */
/************************************************************************/

bool netCDFVariable::ReadOneElement(const GDALExtendedDataType& src_datatype,
                                    const GDALExtendedDataType& bufferDataType,
                                    const size_t* array_idx,
                                    void* pDstBuffer) const
{
    if( src_datatype.GetClass() == GEDTC_STRING )
    {
        char* pszStr = nullptr;
        int ret = nc_get_var1_string(m_gid, m_varid, array_idx, &pszStr);
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return false;
        nc_free_string(1, &pszStr);
        GDALExtendedDataType::CopyValue(&pszStr, src_datatype, pDstBuffer, bufferDataType);
        return true;
    }

    std::vector<GByte> abySrc(std::max(src_datatype.GetSize(),
                                GetNCTypeSize(src_datatype,
                                    m_bPerfectDataTypeMatch,
                                    m_nVarType)));

    int ret = nc_get_var1(m_gid, m_varid, array_idx, &abySrc[0]);
    NCDF_ERR(ret);
    if( ret != NC_NOERR )
        return false;

    ConvertNCToGDAL(&abySrc[0]);

    GDALExtendedDataType::CopyValue(&abySrc[0], src_datatype, pDstBuffer, bufferDataType);
    return true;
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool netCDFVariable::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    if( m_nDims == 2 && m_nVarType == NC_CHAR && GetDimensions().size() == 1 )
    {
        CPLMutexHolderD(&hNCMutex);
        m_poShared->SetDefineMode(false);

        if( bufferDataType.GetClass() != GEDTC_STRING )
            return false;
        GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
        size_t array_idx[2] = { static_cast<size_t>(arrayStartIdx[0]), 0 };
        size_t array_count[2] = { 1, m_nTextLength };
        std::string osTmp(m_nTextLength, 0);
        const char* pszTmp = osTmp.c_str();
        for(size_t i = 0; i < count[0]; i++ )
        {
            int ret = nc_get_vara(m_gid, m_varid, array_idx, array_count, &osTmp[0]);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return false;
            // coverity[use_after_free]
            GDALExtendedDataType::CopyValue(&pszTmp, GetDataType(), pabyDstBuffer, GetDataType());
            array_idx[0] = static_cast<size_t>(array_idx[0] + arrayStep[0]);
            pabyDstBuffer += bufferStride[0] * sizeof(char*);
        }
        return true;
    }

    if( m_poCachedArray )
    {
        const auto nDims = GetDimensionCount();
        std::vector<GUInt64> modifiedArrayStartIdx(nDims);
        bool canUseCache = true;
        for( size_t i = 0; i < nDims; i++ )
        {
            if( arrayStartIdx[i] >= m_cachedArrayStartIdx[i] &&
                arrayStartIdx[i] + (count[i] - 1) * arrayStep[i] <=
                    m_cachedArrayStartIdx[i] + m_cachedCount[i] - 1 )
            {
                modifiedArrayStartIdx[i] = arrayStartIdx[i] - m_cachedArrayStartIdx[i];
            }
            else
            {
                canUseCache = false;
                break;
            }
        }
        if( canUseCache )
        {
            return m_poCachedArray->Read( modifiedArrayStartIdx.data(),
                                          count,
                                          arrayStep,
                                          bufferStride,
                                          bufferDataType,
                                          pDstBuffer );
        }
    }

    return IReadWrite
                (true,
                 arrayStartIdx, count, arrayStep, bufferStride,
                 bufferDataType, pDstBuffer,
                 nc_get_var1,
                 nc_get_vara,
                 nc_get_varm,
                 &netCDFVariable::ReadOneElement);
}

/************************************************************************/
/*                             IAdviseRead()                            */
/************************************************************************/

bool netCDFVariable::IAdviseRead(const GUInt64* arrayStartIdx,
                                 const size_t* count) const
{
    const auto nDims = GetDimensionCount();
    if( nDims == 0 )
        return true;
    const auto& eDT = GetDataType();
    if( eDT.GetClass() != GEDTC_NUMERIC )
        return false;

    auto poMemDriver = static_cast<GDALDriver*>(GDALGetDriverByName("MEM"));
    if( poMemDriver == nullptr )
        return false;

    m_poCachedArray.reset();

    size_t nElts = 1;
    for( size_t i = 0; i < nDims; i++ )
        nElts *= count[i];

    void* pData = VSI_MALLOC2_VERBOSE(nElts, eDT.GetSize());
    if( pData == nullptr )
        return false;

    if( !Read(arrayStartIdx, count, nullptr, nullptr, eDT, pData) )
    {
        VSIFree(pData);
        return false;
    }

    auto poDS = poMemDriver->CreateMultiDimensional("", nullptr, nullptr);
    auto poGroup = poDS->GetRootGroup();
    delete poDS;

    std::vector<std::shared_ptr<GDALDimension>> apoMemDims;
    const auto& poDims = GetDimensions();
    for( size_t i = 0; i < nDims; i++ )
    {
        apoMemDims.emplace_back(poGroup->CreateDimension( poDims[i]->GetName(),
                                                          std::string(),
                                                          std::string(),
                                                          count[i],
                                                          nullptr ) );
    }
    m_poCachedArray = poGroup->CreateMDArray(GetName(), apoMemDims, eDT, nullptr);
    m_poCachedArray->Write( std::vector<GUInt64>(nDims).data(),
                            count,
                            nullptr,
                            nullptr,
                            eDT,
                            pData );
    m_cachedArrayStartIdx.resize(nDims);
    memcpy( &m_cachedArrayStartIdx[0], arrayStartIdx, nDims * sizeof(GUInt64) );
    m_cachedCount.resize(nDims);
    memcpy( &m_cachedCount[0], count, nDims * sizeof(size_t) );
    VSIFree(pData);
    return true;
}

/************************************************************************/
/*                          ConvertGDALToNC()                           */
/************************************************************************/

void netCDFVariable::ConvertGDALToNC(GByte* buffer) const
{
    if( !m_bPerfectDataTypeMatch )
    {
        if( m_nVarType == NC_CHAR || m_nVarType == NC_BYTE )
        {
            const auto c = static_cast<signed char>(
                reinterpret_cast<short*>(buffer)[0]);
            memcpy(buffer, &c, sizeof(c));
        }
        else if( m_nVarType == NC_INT64 )
        {
            const auto v = static_cast<GInt64>(
                reinterpret_cast<double*>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
        else if( m_nVarType == NC_UINT64 )
        {
            const auto v = static_cast<GUInt64>(
                reinterpret_cast<double*>(buffer)[0]);
            memcpy(buffer, &v, sizeof(v));
        }
    }
}

/************************************************************************/
/*                          WriteOneElement()                           */
/************************************************************************/

bool netCDFVariable::WriteOneElement(const GDALExtendedDataType& dst_datatype,
                                    const GDALExtendedDataType& bufferDataType,
                                    const size_t* array_idx,
                                    const void* pSrcBuffer) const
{
    if( dst_datatype.GetClass() == GEDTC_STRING )
    {
        const char* pszStr = (static_cast<const char* const*>(pSrcBuffer))[0];
        int ret = nc_put_var1_string(m_gid, m_varid, array_idx, &pszStr);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }

    std::vector<GByte> abyTmp(dst_datatype.GetSize());
    GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &abyTmp[0], dst_datatype);

    ConvertGDALToNC(&abyTmp[0]);

    int ret = nc_put_var1(m_gid, m_varid, array_idx, &abyTmp[0]);
    NCDF_ERR(ret);
    return ret == NC_NOERR;
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

bool netCDFVariable::IWrite(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               const void* pSrcBuffer)
{
    m_bHasWrittenData = true;

    m_poCachedArray.reset();

    if( m_nDims == 2 && m_nVarType == NC_CHAR && GetDimensions().size() == 1 )
    {
        CPLMutexHolderD(&hNCMutex);
        m_poShared->SetDefineMode(false);

        if( bufferDataType.GetClass() != GEDTC_STRING )
            return false;
        const char* const* ppszSrcBuffer = static_cast<const char* const*>(pSrcBuffer);
        size_t array_idx[2] = { static_cast<size_t>(arrayStartIdx[0]), 0 };
        size_t array_count[2] = { 1, m_nTextLength };
        std::string osTmp(m_nTextLength, 0);
        for(size_t i = 0; i < count[0]; i++ )
        {
            const char* pszStr = *ppszSrcBuffer;
            memset(&osTmp[0], 0, m_nTextLength);
            if( pszStr )
            {
                size_t nLen = strlen(pszStr);
                memcpy(&osTmp[0], pszStr, std::min(m_nTextLength, nLen));
            }
            int ret = nc_put_vara(m_gid, m_varid, array_idx, array_count, &osTmp[0]);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return false;
            array_idx[0] = static_cast<size_t>(array_idx[0] + arrayStep[0]);
            ppszSrcBuffer += bufferStride[0];
        }
        return true;
    }

    return IReadWrite
                (false,
                 arrayStartIdx, count, arrayStep, bufferStride,
                 bufferDataType, pSrcBuffer,
                 nc_put_var1,
                 nc_put_vara,
                 nc_put_varm,
                 &netCDFVariable::WriteOneElement);
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void* netCDFVariable::GetRawNoDataValue() const
{
    const auto& dt = GetDataType();
    if( m_nVarType == NC_STRING )
        return nullptr;

    if( m_bGetRawNoDataValueHasRun )
    {
        return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
    }

    m_bGetRawNoDataValueHasRun = true;
    CPLMutexHolderD(&hNCMutex);
    std::vector<GByte> abyTmp(std::max(dt.GetSize(),
                                GetNCTypeSize(dt,
                                    m_bPerfectDataTypeMatch,
                                    m_nVarType)));
    int ret = nc_get_att(m_gid, m_varid, _FillValue, &abyTmp[0]);
    if( ret != NC_NOERR )
    {
        m_abyNoData.clear();
        return nullptr;
    }
    ConvertNCToGDAL(&abyTmp[0]);
    m_abyNoData.resize(dt.GetSize());
    memcpy(&m_abyNoData[0], &abyTmp[0], m_abyNoData.size());
    return m_abyNoData.data();
}

/************************************************************************/
/*                          SetRawNoDataValue()                         */
/************************************************************************/

bool netCDFVariable::SetRawNoDataValue(const void* pNoData)
{
    GetDataType();
    if( m_nVarType == NC_STRING )
        return false;

    m_bGetRawNoDataValueHasRun = false;
    CPLMutexHolderD(&hNCMutex);
    m_poShared->SetDefineMode(true);
    int ret;
    if( pNoData == nullptr )
    {
        m_abyNoData.clear();
        ret = nc_del_att(m_gid, m_varid, _FillValue);
    }
    else
    {
        const auto nSize = GetDataType().GetSize();
        m_abyNoData.resize(nSize);
        memcpy(&m_abyNoData[0], pNoData, nSize);

        std::vector<GByte> abyTmp(nSize);
        memcpy(&abyTmp[0], pNoData, nSize);
        ConvertGDALToNC(&abyTmp[0]);

        if( !m_bHasWrittenData )
        {
            ret = nc_def_var_fill(m_gid, m_varid, NC_FILL, &abyTmp[0]);
            NCDF_ERR(ret);
        }

        ret = nc_put_att(m_gid, m_varid, _FillValue, m_nVarType, 1, &abyTmp[0]);
    }
    NCDF_ERR(ret);
    if( ret == NC_NOERR )
        m_bGetRawNoDataValueHasRun = true;
    return ret == NC_NOERR;
}

/************************************************************************/
/*                               SetScale()                             */
/************************************************************************/

bool netCDFVariable::SetScale(double dfScale, GDALDataType eStorageType)
{
    auto poAttr = GetAttribute(CF_SCALE_FACTOR);
    if( !poAttr )
    {
        poAttr = CreateAttribute(CF_SCALE_FACTOR, {},
                                 GDALExtendedDataType::Create(
                                     eStorageType == GDT_Unknown ? GDT_Float64 : eStorageType),
                                 nullptr);
    }
    if( !poAttr )
        return false;
    return poAttr->Write(dfScale);
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

bool netCDFVariable::SetOffset(double dfOffset, GDALDataType eStorageType)
{
    auto poAttr = GetAttribute(CF_ADD_OFFSET);
    if( !poAttr )
    {
        poAttr = CreateAttribute(CF_ADD_OFFSET, {},
                                 GDALExtendedDataType::Create(
                                     eStorageType == GDT_Unknown ? GDT_Float64 : eStorageType),
                                 nullptr);
    }
    if( !poAttr )
        return false;
    return poAttr->Write(dfOffset);
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

double netCDFVariable::GetScale(bool* pbHasScale, GDALDataType* peStorageType) const
{
    auto poAttr = GetAttribute(CF_SCALE_FACTOR);
    if( !poAttr || poAttr->GetDataType().GetClass() != GEDTC_NUMERIC )
    {
        if( pbHasScale )
            *pbHasScale = false;
        return 1.0;
    }
    if( pbHasScale )
        *pbHasScale = true;
    if( peStorageType )
        *peStorageType = poAttr->GetDataType().GetNumericDataType();
    return poAttr->ReadAsDouble();
}

/************************************************************************/
/*                               GetOffset()                            */
/************************************************************************/

double netCDFVariable::GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const
{
    auto poAttr = GetAttribute(CF_ADD_OFFSET);
    if( !poAttr || poAttr->GetDataType().GetClass() != GEDTC_NUMERIC )
    {
        if( pbHasOffset )
            *pbHasOffset = false;
        return 0.0;
    }
    if( pbHasOffset )
        *pbHasOffset = true;
    if( peStorageType )
        *peStorageType = poAttr->GetDataType().GetNumericDataType();
    return poAttr->ReadAsDouble();
}

/************************************************************************/
/*                           GetBlockSize()                             */
/************************************************************************/

std::vector<GUInt64> netCDFVariable::GetBlockSize() const
{
    std::vector<GUInt64> res(GetDimensionCount());
    if( res.empty() )
        return res;
    int nStorageType = 0;
    std::vector<size_t> anTemp(GetDimensionCount());
    nc_inq_var_chunking(m_gid, m_varid, &nStorageType, &anTemp[0]);
    if( nStorageType == NC_CHUNKED )
    {
        for( size_t i = 0; i < res.size(); ++i )
            res[i] = anTemp[i];
    }
    return res;
}

/************************************************************************/
/*                         GetAttribute()                               */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFVariable::GetAttribute(const std::string& osName) const
{
    CPLMutexHolderD(&hNCMutex);
    int nAttId = -1;
    if( nc_inq_attid(m_gid, m_varid, osName.c_str(), &nAttId) != NC_NOERR )
        return nullptr;
    return netCDFAttribute::Create(m_poShared, m_gid, m_varid, osName);
}

/************************************************************************/
/*                         GetAttributes()                              */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> netCDFVariable::GetAttributes(CSLConstList papszOptions) const
{
    CPLMutexHolderD(&hNCMutex);
    std::vector<std::shared_ptr<GDALAttribute>> res;
    int nbAttr = 0;
    NCDF_ERR(nc_inq_varnatts(m_gid, m_varid, &nbAttr));
    res.reserve(nbAttr);
    const bool bShowAll = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "SHOW_ALL", "NO"));
    for( int i = 0; i < nbAttr; i++ )
    {
        char szAttrName[NC_MAX_NAME + 1];
        szAttrName[0] = 0;
        NCDF_ERR(nc_inq_attname(m_gid, m_varid, i, szAttrName));
        if( bShowAll ||
            (!EQUAL(szAttrName, _FillValue) &&
             !EQUAL(szAttrName, CF_UNITS) &&
             !EQUAL(szAttrName, CF_SCALE_FACTOR) &&
             !EQUAL(szAttrName, CF_ADD_OFFSET) &&
             !EQUAL(szAttrName, CF_GRD_MAPPING) &&
             !(EQUAL(szAttrName, "_Unsigned") && m_nVarType == NC_BYTE)) )
        {
            res.emplace_back(netCDFAttribute::Create(
                m_poShared, m_gid, m_varid, szAttrName));
        }
    }
    return res;
}

/************************************************************************/
/*                          CreateAttribute()                           */
/************************************************************************/

std::shared_ptr<GDALAttribute> netCDFVariable::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions)
{
    return netCDFAttribute::Create(m_poShared, m_gid, m_varid,
                                   osName, anDimensions, oDataType,
                                   papszOptions);
}

/************************************************************************/
/*                    retrieveAttributeParentName()                     */
/************************************************************************/

static CPLString retrieveAttributeParentName(int gid, int varid)
{
    auto groupName(NCDFGetGroupFullName(gid));
    if( varid == NC_GLOBAL )
    {
        if( groupName == "/" )
            return "/_GLOBAL_";
        return groupName + "/_GLOBAL_";
    }

    return groupName + "/" +
           netCDFVariable::retrieveName(gid, varid);
}

/************************************************************************/
/*                          netCDFAttribute()                           */
/************************************************************************/

netCDFAttribute::netCDFAttribute(
                    const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& name):
    GDALAbstractMDArray(retrieveAttributeParentName(gid, varid), name),
    GDALAttribute(retrieveAttributeParentName(gid, varid), name),
    m_poShared(poShared),
    m_gid(gid),
    m_varid(varid)
{
    CPLMutexHolderD(&hNCMutex);
    size_t nLen = 0;
    NCDF_ERR(nc_inq_atttype(m_gid, m_varid, GetName().c_str(), &m_nAttType));
    NCDF_ERR(nc_inq_attlen(m_gid, m_varid, GetName().c_str(), &nLen));
    if( m_nAttType == NC_CHAR )
    {
        m_nTextLength = nLen;
    }
    else if( nLen > 1 )
    {
        m_dims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), "length", std::string(), std::string(), nLen));
    }
}

/************************************************************************/
/*                          netCDFAttribute()                           */
/************************************************************************/

netCDFAttribute::netCDFAttribute(const std::shared_ptr<netCDFSharedResources>& poShared,
                   int gid, int varid, const std::string& osName,
                   const std::vector<GUInt64>& anDimensions,
                   const GDALExtendedDataType& oDataType,
                   CSLConstList papszOptions):
    GDALAbstractMDArray(retrieveAttributeParentName(gid, varid), osName),
    GDALAttribute(retrieveAttributeParentName(gid, varid), osName),
    m_poShared(poShared),
    m_gid(gid),
    m_varid(varid)
{
    CPLMutexHolderD(&hNCMutex);
    m_bPerfectDataTypeMatch = true;
    m_nAttType = CreateOrGetType(gid, oDataType);
    m_dt.reset(new GDALExtendedDataType(oDataType));
    if( !anDimensions.empty() )
    {
        m_dims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), "length", std::string(), std::string(), anDimensions[0]));
    }

    const char* pszType = CSLFetchNameValueDef(papszOptions, "NC_TYPE", "");
    if( oDataType.GetClass() == GEDTC_STRING && anDimensions.empty() &&
        (EQUAL(pszType, "") || EQUAL(pszType, "NC_CHAR")) )
    {
        m_nAttType = NC_CHAR;
    }
    else if( oDataType.GetNumericDataType() == GDT_Int16 &&
             EQUAL(CSLFetchNameValueDef(papszOptions, "NC_TYPE", ""), "NC_BYTE") )
    {
        m_bPerfectDataTypeMatch = false;
        m_nAttType = NC_BYTE;
    }
    else if( oDataType.GetNumericDataType() == GDT_Float64 )
    {
        if( EQUAL(pszType, "NC_INT64") )
        {
            m_bPerfectDataTypeMatch = false;
            m_nAttType = NC_INT64;
        }
        else if( EQUAL(pszType, "NC_UINT64") )
        {
            m_bPerfectDataTypeMatch = false;
            m_nAttType = NC_UINT64;
        }
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

std::shared_ptr<netCDFAttribute> netCDFAttribute::Create(
                    const std::shared_ptr<netCDFSharedResources>& poShared,
                    int gid, int varid, const std::string& name)
{
    auto attr(std::shared_ptr<netCDFAttribute>(
        new netCDFAttribute(poShared, gid, varid, name)));
    attr->SetSelf(attr);
    return attr;
}

std::shared_ptr<netCDFAttribute> netCDFAttribute::Create(
                   const std::shared_ptr<netCDFSharedResources>& poShared,
                   int gid, int varid, const std::string& osName,
                   const std::vector<GUInt64>& anDimensions,
                   const GDALExtendedDataType& oDataType,
                   CSLConstList papszOptions)
{
    if( poShared->IsReadOnly() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateAttribute() not supported on read-only file");
        return nullptr;
    }
    if( anDimensions.size() > 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 0 or 1-dimensional attribute are supported");
        return nullptr;
    }
    auto attr(std::shared_ptr<netCDFAttribute>(
        new netCDFAttribute(poShared, gid, varid, osName,
                            anDimensions, oDataType,
                            papszOptions)));
    if( attr->m_nAttType == NC_NAT )
        return nullptr;
    attr->SetSelf(attr);
    return attr;
}

/************************************************************************/
/*                             GetDataType()                            */
/************************************************************************/

const GDALExtendedDataType &netCDFAttribute::GetDataType() const
{
    if( m_dt )
        return *m_dt;
    CPLMutexHolderD(&hNCMutex);

    if( m_nAttType == NC_CHAR )
    {
        m_dt.reset(new GDALExtendedDataType(GDALExtendedDataType::CreateString()));
    }
    else
    {
        m_dt.reset(new GDALExtendedDataType(GDALExtendedDataType::Create(GDT_Unknown)));
        BuildDataType(m_gid, m_varid, m_nAttType, m_dt, m_bPerfectDataTypeMatch);
    }

    return *m_dt;
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool netCDFAttribute::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    CPLMutexHolderD(&hNCMutex);

    if( m_nAttType == NC_STRING )
    {
        CPLAssert( GetDataType().GetClass() == GEDTC_STRING );
        std::vector<char*> apszStrings(static_cast<size_t>(GetTotalElementsCount()));
        int ret = nc_get_att_string(m_gid, m_varid, GetName().c_str(), &apszStrings[0]);
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return false;
        if( m_dims.empty() )
        {
            const char* pszStr = apszStrings[0];
            GDALExtendedDataType::CopyValue(&pszStr, GetDataType(), pDstBuffer, bufferDataType);
        }
        else
        {
            GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
            for(size_t i = 0; i < count[0]; i++ )
            {
                auto srcIdx = static_cast<size_t>(arrayStartIdx[0] + arrayStep[0] * i);
                const char* pszStr = apszStrings[srcIdx];
                GDALExtendedDataType::CopyValue(&pszStr, GetDataType(),
                            pabyDstBuffer, bufferDataType);
                pabyDstBuffer += sizeof(char*) * bufferStride[0];
            }
        }
        nc_free_string(apszStrings.size(), &apszStrings[0]);
        return true;
    }

    if( m_nAttType == NC_CHAR )
    {
        CPLAssert( GetDataType().GetClass() == GEDTC_STRING );
        CPLAssert( m_dims.empty() );
        if( bufferDataType != GetDataType() )
        {
            std::string osStr;
            osStr.resize(m_nTextLength);
            int ret = nc_get_att_text(m_gid, m_varid, GetName().c_str(), &osStr[0]);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return false;
            const char* pszStr = osStr.c_str();
            GDALExtendedDataType::CopyValue(&pszStr, GetDataType(), pDstBuffer, bufferDataType);
        }
        else
        {
            char* pszStr = static_cast<char*>(CPLCalloc(1, m_nTextLength + 1));
            int ret = nc_get_att_text(m_gid, m_varid, GetName().c_str(), pszStr);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
            {
                CPLFree(pszStr);
                return false;
            }
            *static_cast<char**>(pDstBuffer) = pszStr;
        }
        return true;
    }

    const auto dt(GetDataType());
    if( dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown )
    {
        return false;
    }

    CPLAssert( dt.GetClass() != GEDTC_STRING );
    const bool bFastPath =
        ((m_dims.size() == 1 &&
          arrayStartIdx[0] == 0 &&
          count[0] == m_dims[0]->GetSize() &&
          arrayStep[0] == 1 &&
          bufferStride[0] == 1 ) ||
         m_dims.empty() ) &&
        m_bPerfectDataTypeMatch &&
        bufferDataType == dt &&
        dt.GetSize() > 0;
    if( bFastPath )
    {
        int ret = nc_get_att(m_gid, m_varid, GetName().c_str(), pDstBuffer);
        NCDF_ERR(ret);
        if( ret == NC_NOERR )
        {
            ConvertNCStringsToCPLStrings(static_cast<GByte*>(pDstBuffer), dt);
        }
        return ret == NC_NOERR;
    }

    const auto nElementSize = GetNCTypeSize(dt,
                                            m_bPerfectDataTypeMatch,
                                            m_nAttType);
    if( nElementSize == 0 )
        return false;
    const auto nOutputDTSize = bufferDataType.GetSize();
    std::vector<GByte> abyBuffer(
        static_cast<size_t>(GetTotalElementsCount()) * nElementSize);
    int ret = nc_get_att(m_gid, m_varid, GetName().c_str(), &abyBuffer[0]);
    NCDF_ERR(ret);
    if( ret != NC_NOERR )
        return false;

    GByte* pabySrcBuffer = m_dims.empty() ? abyBuffer.data() :
        abyBuffer.data() +
            static_cast<size_t>(arrayStartIdx[0]) * nElementSize;
    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    for(size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++ )
    {
        GByte abyTmpBuffer[sizeof(double)];
        const GByte* pabySrcElement = pabySrcBuffer;
        if( !m_bPerfectDataTypeMatch )
        {
            if( m_nAttType == NC_BYTE )
            {
                short s = reinterpret_cast<const signed char*>(pabySrcBuffer)[0];
                memcpy(abyTmpBuffer, &s, sizeof(s));
                pabySrcElement = abyTmpBuffer;
            }
            else if( m_nAttType == NC_INT64 )
            {
                double v = static_cast<double>(
                    reinterpret_cast<const GInt64*>(pabySrcBuffer)[0]);
                memcpy(abyTmpBuffer, &v, sizeof(v));
                pabySrcElement = abyTmpBuffer;
            }
            else if( m_nAttType == NC_UINT64 )
            {
                double v = static_cast<double>(
                    reinterpret_cast<const GUInt64*>(pabySrcBuffer)[0]);
                memcpy(abyTmpBuffer, &v, sizeof(v));
                pabySrcElement = abyTmpBuffer;
            }
            else
            {
                CPLAssert(false);
            }
        }
        GDALExtendedDataType::CopyValue(pabySrcElement, dt,
                    pabyDstBuffer, bufferDataType);
        FreeNCStrings(pabySrcBuffer, dt);
        if( !m_dims.empty() )
        {
            pabySrcBuffer += static_cast<std::ptrdiff_t>(arrayStep[0] * nElementSize);
            pabyDstBuffer += nOutputDTSize * bufferStride[0];
        }
    }

    return true;
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

bool netCDFAttribute::IWrite(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               const void* pSrcBuffer)
{
    CPLMutexHolderD(&hNCMutex);

    if( m_dims.size() == 1 &&
        (arrayStartIdx[0] != 0 || 
         count[0] != m_dims[0]->GetSize() || arrayStep[0] != 1) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Only contiguous writing of attribute values supported");
        return false;
    }

    m_poShared->SetDefineMode(true);

    if( m_nAttType == NC_STRING )
    {
        CPLAssert( GetDataType().GetClass() == GEDTC_STRING );
        if( m_dims.empty() )
        {
            char* pszStr = nullptr;
            const char* pszStrConst;
            if( bufferDataType != GetDataType() )
            {
                GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &pszStr, GetDataType());
                pszStrConst = pszStr;
            }
            else
            {
                memcpy(&pszStrConst, pSrcBuffer, sizeof(const char*));
            }
            int ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(),
                                        1, &pszStrConst);
            CPLFree(pszStr);
            NCDF_ERR(ret);
            if( ret != NC_NOERR )
                return false;
            return true;
        }

        int ret;
        if( bufferDataType != GetDataType() )
        {
            std::vector<char*> apszStrings(count[0]);
            const char** ppszStr;
            memcpy(&ppszStr, &pSrcBuffer, sizeof(const char**));
            for( size_t i = 0; i < count[0]; i++ )
            {
                GDALExtendedDataType::CopyValue(&ppszStr[i], bufferDataType, &apszStrings[i], GetDataType());
            }
            ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(),
                                        count[0],
                                        const_cast<const char**>(&apszStrings[0]));
            for( size_t i = 0; i < count[0]; i++ )
            {
                CPLFree(apszStrings[i]);
            }
        }
        else
        {
            const char** ppszStr;
            memcpy(&ppszStr, &pSrcBuffer, sizeof(const char**));
            ret = nc_put_att_string(m_gid, m_varid, GetName().c_str(),
                                    count[0],
                                    ppszStr);
        }
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return false;
        return true;
    }

    if( m_nAttType == NC_CHAR )
    {
        CPLAssert( GetDataType().GetClass() == GEDTC_STRING );
        CPLAssert( m_dims.empty() );
        char* pszStr = nullptr;
        const char* pszStrConst;
        if( bufferDataType != GetDataType() )
        {
            GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &pszStr, GetDataType());
            pszStrConst = pszStr;
        }
        else
        {
            memcpy(&pszStrConst, pSrcBuffer, sizeof(const char*));
        }
        m_nTextLength = pszStrConst ? strlen(pszStrConst) : 0;
        int ret = nc_put_att_text(m_gid, m_varid, GetName().c_str(),
                                  m_nTextLength, pszStrConst);
        CPLFree(pszStr);
        NCDF_ERR(ret);
        if( ret != NC_NOERR )
            return false;
        return true;
    }

    const auto dt(GetDataType());
    if( dt.GetClass() == GEDTC_NUMERIC &&
        dt.GetNumericDataType() == GDT_Unknown )
    {
        return false;
    }

    CPLAssert( dt.GetClass() != GEDTC_STRING );
    const bool bFastPath =
        ((m_dims.size() == 1 &&
          bufferStride[0] == 1 ) ||
         m_dims.empty() ) &&
        m_bPerfectDataTypeMatch &&
        bufferDataType == dt &&
        dt.GetSize() > 0;
    if( bFastPath )
    {
        int ret = nc_put_att(m_gid, m_varid, GetName().c_str(),
                             m_nAttType,
                             m_dims.empty() ? 1 : count[0],
                             pSrcBuffer);
        NCDF_ERR(ret);
        return ret == NC_NOERR;
    }

    const auto nElementSize = GetNCTypeSize(dt,
                                            m_bPerfectDataTypeMatch,
                                            m_nAttType);
    if( nElementSize == 0 )
        return false;
    const auto nInputDTSize = bufferDataType.GetSize();
    std::vector<GByte> abyBuffer(
        static_cast<size_t>(GetTotalElementsCount()) * nElementSize);

    const GByte* pabySrcBuffer = static_cast<const GByte*>(pSrcBuffer);
    auto pabyDstBuffer = &abyBuffer[0];
    for(size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); i++ )
    {
        if( !m_bPerfectDataTypeMatch )
        {
            if( m_nAttType == NC_BYTE )
            {
                short s;
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                            &s, dt);
                signed char c = static_cast<signed char>(s);
                memcpy(pabyDstBuffer, &c, sizeof(c));
            }
            else if( m_nAttType == NC_INT64 )
            {
                double d;
                GDALExtendedDataType::CopyValue(pabySrcBuffer, bufferDataType,
                            &d, dt);
                GInt64 v = static_cast<GInt64>(d);
                memcpy(pabyDstBuffer, &v, sizeof(v));
            }
            else if( m_nAttType == NC_UINT64 )
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

        if( !m_dims.empty() )
        {
            pabySrcBuffer += nInputDTSize * bufferStride[0];
            pabyDstBuffer += nElementSize;
        }
    }

    int ret = nc_put_att(m_gid, m_varid, GetName().c_str(),
                            m_nAttType,
                            m_dims.empty() ? 1 : count[0],
                            &abyBuffer[0]);
    NCDF_ERR(ret);
    return ret == NC_NOERR;
}

/************************************************************************/
/*                           OpenMultiDim()                             */
/************************************************************************/

GDALDataset *netCDFDataset::OpenMultiDim( GDALOpenInfo *poOpenInfo )
{

    CPLMutexHolderD(&hNCMutex);

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with
                                // GDALDataset own mutex.
    netCDFDataset *poDS = new netCDFDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);

    auto poSharedResources(std::make_shared<netCDFSharedResources>());

    // For example to open DAP datasets
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:") )
    {
        poSharedResources->m_osFilename = poOpenInfo->pszFilename + strlen("NETCDF:");
        if( !poSharedResources->m_osFilename.empty() &&
            poSharedResources->m_osFilename[0] == '"' &&
            poSharedResources->m_osFilename.back() == '"' )
        {
            poSharedResources->m_osFilename = poSharedResources->m_osFilename.
                substr(1, poSharedResources->m_osFilename.size() - 2);
        }
    }
    else
        poSharedResources->m_osFilename = poOpenInfo->pszFilename;

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);

#ifdef ENABLE_NCDUMP
    const char* pszHeader =
                reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( poOpenInfo->fpL != nullptr &&
        STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") &&
        strstr(pszHeader, "variables:") )
    {
        // By default create a temporary file that will be destroyed,
        // unless NETCDF_TMP_FILE is defined. Can be useful to see which
        // netCDF file has been generated from a potential fuzzed input.
        poSharedResources->m_osFilename = CPLGetConfigOption("NETCDF_TMP_FILE", "");
        if( poSharedResources->m_osFilename.empty() )
        {
            poSharedResources->m_bFileToDestroyAtClosing = true;
            poSharedResources->m_osFilename = CPLGenerateTempFilename("netcdf_tmp");
        }
        if( !netCDFDatasetCreateTempFile( NCDF_FORMAT_NC4,
                                          poSharedResources->m_osFilename,
                                          poOpenInfo->fpL ) )
        {
            CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                        // with GDALDataset own mutex.
            delete poDS;
            CPLAcquireMutex(hNCMutex, 1000.0);
            return nullptr;
        }
        poDS->eFormat = NCDF_FORMAT_NC4;
    }
#endif

    // Try opening the dataset.
#if defined(NCDF_DEBUG) && defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open_mem(%s)", poSharedResources->m_osFilename.c_str());
#elseif defined(NCDF_DEBUG) && !defined(ENABLE_UFFD)
    CPLDebug("GDAL_netCDF", "calling nc_open(%s)", poSharedResources->m_osFilename.c_str());
#endif
    int cdfid = -1;
    const int nMode = (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0 ? NC_WRITE : NC_NOWRITE;
    CPLString osFilenameForNCOpen(poSharedResources->m_osFilename);
#ifdef WIN32
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCOpen, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCOpen = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int status2;

#ifdef ENABLE_UFFD
    bool bVsiFile = !strncmp(osFilenameForNCOpen, "/vsi", strlen("/vsi"));
    bool bReadOnly = (poOpenInfo->eAccess == GA_ReadOnly);
    void * pVma = nullptr;
    uint64_t nVmaSize = 0;
    cpl_uffd_context * pCtx = nullptr;

    if ( bVsiFile && bReadOnly && CPLIsUserFaultMappingSupported() )
      pCtx = CPLCreateUserFaultMapping(osFilenameForNCOpen, &pVma, &nVmaSize);
    if (pCtx != nullptr && pVma != nullptr && nVmaSize > 0)
      status2 = nc_open_mem(osFilenameForNCOpen, nMode, static_cast<size_t>(nVmaSize), pVma, &cdfid);
    else
      status2 = nc_open(osFilenameForNCOpen, nMode, &cdfid);
    poSharedResources->m_pUffdCtx = pCtx;
#else
    status2 = nc_open(osFilenameForNCOpen, nMode, &cdfid);
#endif
    if( status2 != NC_NOERR )
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

#if defined(ENABLE_NCDUMP) && !defined(WIN32)
    // Try to destroy the temporary file right now on Unix
    if( poSharedResources->m_bFileToDestroyAtClosing )
    {
        if( VSIUnlink( poSharedResources->m_osFilename ) == 0 )
        {
            poSharedResources->m_bFileToDestroyAtClosing = false;
        }
    }
#endif
    poSharedResources->m_bReadOnly = nMode == NC_NOWRITE;
    poSharedResources->m_cdfid = cdfid;

    // Is this a real netCDF file?
    int ndims;
    int ngatts;
    int nvars;
    int unlimdimid;
    int status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if( status != NC_NOERR )
    {
        CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock
                                    // with GDALDataset own mutex.
        delete poDS;
        CPLAcquireMutex(hNCMutex, 1000.0);
        return nullptr;
    }

    poDS->m_poRootGroup.reset(new netCDFGroup(poSharedResources, cdfid));

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

GDALDataset* netCDFDataset::CreateMultiDimensional( const char * pszFilename,
                                                    CSLConstList /* papszRootGroupOptions */,
                                                    CSLConstList papszOptions )
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
    if( CSLFetchNameValue(papszOptions, "FORMAT") == nullptr )
    {
        poDS->papszCreationOptions = CSLSetNameValue(
            poDS->papszCreationOptions, "FORMAT", "NC4");
    }
    poDS->ProcessCreationOptions();

    // Create the dataset.
    CPLString osFilenameForNCCreate(pszFilename);
#ifdef WIN32
    if( CPLTestBool(CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
    {
        char* pszTemp = CPLRecode( osFilenameForNCCreate, CPL_ENC_UTF8, "CP_ACP" );
        osFilenameForNCCreate = pszTemp;
        CPLFree(pszTemp);
    }
#endif
    int cdfid = 0;
    int status = nc_create(osFilenameForNCCreate, poDS->nCreateMode, &cdfid);
    if( status != NC_NOERR )
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

    auto poSharedResources(std::make_shared<netCDFSharedResources>());
    poSharedResources->m_cdfid = cdfid;
    poSharedResources->m_bReadOnly = false;
    poSharedResources->m_bDefineMode = true;
    poDS->m_poRootGroup.reset(new netCDFGroup(poSharedResources, cdfid));
    const char* pszConventions = CSLFetchNameValueDef(
        papszOptions, "CONVENTIONS", NCDF_CONVENTIONS_CF_V1_6);
    if( !EQUAL(pszConventions, "") )
    {
        auto poAttr = poDS->m_poRootGroup->CreateAttribute(
            NCDF_CONVENTIONS, {}, GDALExtendedDataType::CreateString());
        if( poAttr )
            poAttr->Write(pszConventions);
    }

    return poDS;
}

#endif // NETCDF_HAS_NC4
