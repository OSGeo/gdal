 /******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_compressor.h"
#include "cpl_json.h"
#include "gdal_priv.h"

#include <algorithm>
#include <cassert>
#include <limits>

/************************************************************************/
/*                            ZarrDataset                               */
/************************************************************************/

class ZarrDataset final: public GDALDataset
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};

public:
    static int Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);

    std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }
};

/************************************************************************/
/*                             ZarrGroup                                */
/************************************************************************/

class ZarrGroup final: public GDALGroup
{
    std::map<CPLString, std::shared_ptr<GDALGroup>> m_oMapGroups{};
    std::map<CPLString, std::shared_ptr<GDALMDArray>> m_oMapMDArrays{};

public:
    ZarrGroup(const std::string& osParentName, const std::string& osName):
        GDALGroup(osParentName, osName) {}

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    void RegisterArray(const std::shared_ptr<GDALMDArray>& array);
};

/************************************************************************/
/*                              DtypeElt()                              */
/************************************************************************/

struct DtypeElt
{
    enum class NativeType
    {
        BOOLEAN,
        UNSIGNED_INT,
        SIGNED_INT,
        IEEEFP,
        COMPLEX_IEEEFP,
        STRING,
    };

    NativeType           nativeType = NativeType::BOOLEAN;
    size_t               nativeOffset = 0;
    size_t               nativeSize = 0;
    bool                 needByteSwapping = false;
    bool                 gdalTypeIsApproxOfNative = false;
    GDALExtendedDataType gdalType = GDALExtendedDataType::Create(GDT_Unknown);
    size_t               gdalOffset = 0;
    size_t               gdalSize = 0;
};

/************************************************************************/
/*                             ZarrArray                                */
/************************************************************************/

class ZarrArray final: public GDALMDArray
{
    const std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    const GDALExtendedDataType                        m_oType;
    const std::vector<DtypeElt>                       m_aoDtypeElts;
    const std::vector<GUInt64>                        m_anBlockSize;
    GByte                                            *m_pabyNoData = nullptr;
    std::string                                       m_osDimSeparator { "." };
    std::string                                       m_osFilename{};
    mutable std::vector<GByte>                        m_abyRawTileData{};
    mutable std::vector<GByte>                        m_abyDecodedTileData{};
    mutable std::vector<uint64_t>                     m_anCachedTiledIndices{};
    mutable bool                                      m_bCachedTiledValid = false;
    mutable bool                                      m_bCachedTiledEmpty = false;
    bool                                              m_bUseOptimizedCodePaths = true;
    bool                                              m_bFortranOrder = false;
    const CPLCompressor                              *m_psDecompressor = nullptr;
    mutable std::vector<GByte>                        m_abyTmpRawTileData{}; // used for Fortran order

    ZarrArray(const std::string& osParentName,
              const std::string& osName,
              const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
              const GDALExtendedDataType& oType,
              const std::vector<DtypeElt>& aoDtypeElts,
              const std::vector<GUInt64>& anBlockSize,
              bool bFortranOrder);

    bool LoadTileData(const std::vector<uint64_t>& tileIndices,
                      bool& bMissingTileOut) const;
    void BlockTranspose(const std::vector<GByte>& abySrc,
                        std::vector<GByte>& abyDst) const;

protected:
    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IsCacheable() const override { return false; }

public:
    ~ZarrArray() override;

    static std::shared_ptr<ZarrArray> Create(const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder);

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_osFilename; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_aoDims; }

    const GDALExtendedDataType& GetDataType() const override { return m_oType; }

    std::vector<GUInt64> GetBlockSize() const override { return m_anBlockSize; }

    const void* GetRawNoDataValue() const override { return m_pabyNoData; }

    void RegisterNoDataValue(const void*);

    void SetFilename(const std::string& osFilename ) { m_osFilename = osFilename; }

    void SetDimSeparator(const std::string& osDimSeparator) { m_osDimSeparator = osDimSeparator; }

    void SetDecompressor(const CPLCompressor* psComp) { m_psDecompressor = psComp; }
};


/************************************************************************/
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> ZarrGroup::GetMDArrayNames(CSLConstList) const
{
    std::vector<std::string> names;
    for( const auto& iter: m_oMapMDArrays )
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                             OpenMDArray()                            */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrGroup::OpenMDArray(const std::string& osName,
                                                   CSLConstList) const
{
    auto oIter = m_oMapMDArrays.find(osName);
    if( oIter != m_oMapMDArrays.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                            RegisterArray()                           */
/************************************************************************/

void ZarrGroup::RegisterArray(const std::shared_ptr<GDALMDArray>& array)
{
    m_oMapMDArrays[array->GetName()] = array;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> ZarrGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> names;
    for( const auto& iter: m_oMapGroups )
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                              OpenGroup()                             */
/************************************************************************/

std::shared_ptr<GDALGroup> ZarrGroup::OpenGroup(const std::string& osName,
                                               CSLConstList) const
{
    auto oIter = m_oMapGroups.find(osName);
    if( oIter != m_oMapGroups.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                         ZarrArray::ZarrArray()                       */
/************************************************************************/

ZarrArray::ZarrArray(const std::string& osParentName,
                     const std::string& osName,
                     const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                     const GDALExtendedDataType& oType,
                     const std::vector<DtypeElt>& aoDtypeElts,
                     const std::vector<GUInt64>& anBlockSize,
                     bool bFortranOrder):
    GDALAbstractMDArray(osParentName, osName),
    GDALMDArray(osParentName, osName),
    m_aoDims(aoDims),
    m_oType(oType),
    m_aoDtypeElts(aoDtypeElts),
    m_anBlockSize(anBlockSize),
    m_bFortranOrder(bFortranOrder)
{
}

/************************************************************************/
/*                          ZarrArray::Create()                         */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrArray::Create(const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder)
{
    auto arr = std::shared_ptr<ZarrArray>(
        new ZarrArray(osParentName, osName, aoDims, oType, aoDtypeElts,
                      anBlockSize, bFortranOrder));
    arr->SetSelf(arr);

    // Reserve a buffer for tile content
    const size_t nSourceSize = aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize;
    size_t nTileSize = arr->m_oType.GetClass() == GEDTC_STRING ? arr->m_oType.GetMaxStringLength() : nSourceSize;
    for( const auto& nBlockSize: arr->m_anBlockSize )
    {
        nTileSize *= static_cast<size_t>(nBlockSize);
    }
    try
    {
        arr->m_abyRawTileData.resize( nTileSize );
        if( bFortranOrder )
            arr->m_abyTmpRawTileData.resize( nTileSize );
    }
    catch( const std::bad_alloc& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return nullptr;
    }

    bool bNeedDecodedBuffer = false;
    for( const auto& elt: arr->m_aoDtypeElts )
    {
        if( elt.needByteSwapping || elt.gdalTypeIsApproxOfNative )
        {
            bNeedDecodedBuffer = true;
            break;
        }
    }
    if( bNeedDecodedBuffer )
    {
        size_t nDecodedBufferSize = arr->m_oType.GetSize();
        for( const auto& nBlockSize: arr->m_anBlockSize )
        {
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        try
        {
            arr->m_abyDecodedTileData.resize( nDecodedBufferSize );
        }
        catch( const std::bad_alloc& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return nullptr;
        }
    }

    arr->m_bUseOptimizedCodePaths = CPLTestBool(
        CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES"));

    return arr;
}

/************************************************************************/
/*                              ~ZarrArray()                            */
/************************************************************************/

ZarrArray::~ZarrArray()
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }
    if( !m_abyDecodedTileData.empty() )
    {
        const size_t nDTSize = m_oType.GetSize();
        GByte* pDst = &m_abyDecodedTileData[0];
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        for( size_t i = 0; i < nValues; i++, pDst += nDTSize )
        {
            for( auto& elt: m_aoDtypeElts )
            {
                if( elt.nativeType == DtypeElt::NativeType::STRING )
                {
                    char* ptr;
                    char** pptr = reinterpret_cast<char**>(pDst + elt.gdalOffset);
                    memcpy(&ptr, pptr, sizeof(ptr));
                    VSIFree(ptr);
                }
            }
        }
    }
}

/************************************************************************/
/*                        RegisterNoDataValue()                         */
/************************************************************************/

void ZarrArray::RegisterNoDataValue(const void* pNoData)
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    if( pNoData == nullptr )
    {
        CPLFree(m_pabyNoData);
        m_pabyNoData = nullptr;
    }
    else
    {
        const auto nSize = m_oType.GetSize();
        if( m_pabyNoData == nullptr )
        {
            m_pabyNoData = static_cast<GByte*>(CPLMalloc(nSize));
        }
        memset(m_pabyNoData, 0, nSize);
        GDALExtendedDataType::CopyValue( pNoData, m_oType, m_pabyNoData, m_oType );
    }
}

/************************************************************************/
/*                      ZarrArray::BlockTranspose()                     */
/************************************************************************/

void ZarrArray::BlockTranspose(const std::vector<GByte>& abySrc,
                               std::vector<GByte>& abyDst) const
{
    // Perform transposition
    const size_t nDims = m_anBlockSize.size();
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;

    struct Stack
    {
        size_t       nIters = 0;
        const GByte* src_ptr = nullptr;
        GByte*       dst_ptr = nullptr;
        size_t       src_inc_offset = 0;
        size_t       dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims + 1);
    assert(!stack.empty()); // to make gcc 9.3 -O2 -Wnull-dereference happy

    stack[0].src_inc_offset = nSourceSize;
    for( size_t i = 1; i < nDims; ++i )
    {
        stack[i].src_inc_offset = stack[i-1].src_inc_offset *
                                static_cast<size_t>(m_anBlockSize[i-1]);
    }

    stack[nDims-1].dst_inc_offset = nSourceSize;
    for( size_t i = nDims - 1; i > 0; )
    {
        --i;
        stack[i].dst_inc_offset = stack[i+1].dst_inc_offset *
                                static_cast<size_t>(m_anBlockSize[i+1]);
    }

    stack[0].src_ptr = abySrc.data();
    stack[0].dst_ptr = &abyDst[0];

    size_t dimIdx = 0;
lbl_next_depth:
    if( dimIdx == nDims )
    {
        void* dst_ptr = stack[nDims].dst_ptr;
        const void* src_ptr = stack[nDims].src_ptr;
        if( nSourceSize == 1 )
            *stack[nDims].dst_ptr = *stack[nDims].src_ptr;
        else if( nSourceSize == 2 )
            *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr);
        else if( nSourceSize == 4 )
            *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr);
        else if( nSourceSize == 8 )
            *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr);
        else
            memcpy(dst_ptr, src_ptr, nSourceSize);
    }
    else
    {
        stack[dimIdx].nIters = static_cast<size_t>(m_anBlockSize[dimIdx]);
        while(true)
        {
            dimIdx ++;
            stack[dimIdx].src_ptr = stack[dimIdx-1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx-1].dst_ptr;
            goto lbl_next_depth;
lbl_return_to_caller:
            dimIdx --;
            if( (--stack[dimIdx].nIters) == 0 )
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;
}

/************************************************************************/
/*                        ZarrArray::LoadTileData()                     */
/************************************************************************/

bool ZarrArray::LoadTileData(const std::vector<uint64_t>& tileIndices,
                             bool& bMissingTileOut) const
{
    std::string osFilename;
    for( const auto index: tileIndices )
    {
        if( !osFilename.empty() )
            osFilename += m_osDimSeparator;
        osFilename += std::to_string(index);
    }
    osFilename = CPLFormFilename(
        CPLGetDirname(m_osFilename.c_str()), osFilename.c_str(), nullptr);

    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if( fp == nullptr )
    {
        // Missing files are OK and indicate nodata_value
        CPLDebugOnly("Zarr", "Tile %s missing (=nodata)", osFilename.c_str());
        bMissingTileOut = true;
        return true;
    }

    bMissingTileOut = false;
    bool bRet = true;
    if( m_psDecompressor == nullptr )
    {
        if( VSIFReadL(&m_abyRawTileData[0], 1, m_abyRawTileData.size(), fp) != m_abyRawTileData.size() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not read tile %s correctly",
                     osFilename.c_str());
            bRet = false;
        }
    }
    else
    {
        VSIFSeekL(fp, 0, SEEK_END);
        const auto nSize = VSIFTellL(fp);
        VSIFSeekL(fp, 0, SEEK_SET);
        if( nSize > static_cast<vsi_l_offset>(std::numeric_limits<int>::max()) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large tile %s",
                     osFilename.c_str());
            bRet = false;
        }
        else
        {
            std::vector<GByte> abyCompressedData;
            try
            {
                abyCompressedData.resize(static_cast<size_t>(nSize));

            }
            catch( const std::exception& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory for tile %s",
                         osFilename.c_str());
                bRet = false;
            }

            if( bRet &&
                VSIFReadL(&abyCompressedData[0], 1, abyCompressedData.size(),
                          fp) != abyCompressedData.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not read tile %s correctly",
                         osFilename.c_str());
                bRet = false;
            }
            else
            {
                void* out_buffer = &m_abyRawTileData[0];
                size_t out_size = m_abyRawTileData.size();
                if( !m_psDecompressor->pfnFunc(abyCompressedData.data(),
                                               abyCompressedData.size(),
                                               &out_buffer, &out_size,
                                               nullptr,
                                               m_psDecompressor->user_data ) ||
                    out_size != m_abyRawTileData.size() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Decompression of tile %s failed",
                             osFilename.c_str());
                    bRet = false;
                }
            }
        }
    }
    VSIFCloseL(fp);

    if( bRet && !bMissingTileOut && m_bFortranOrder )
    {
        BlockTranspose(m_abyRawTileData, m_abyTmpRawTileData);
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    if( bRet && !bMissingTileOut && !m_abyDecodedTileData.empty() )
    {
        const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                                   m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        const GByte* pSrc = m_abyRawTileData.data();
        GByte* pDst = &m_abyDecodedTileData[0];
        for( size_t i = 0; i < nValues; i++, pSrc += nSourceSize, pDst += nDTSize )
        {
            for( auto& elt: m_aoDtypeElts )
            {
                if( elt.needByteSwapping )
                {
                    if( elt.nativeSize == 2 )
                    {
                        *reinterpret_cast<uint16_t*>(pDst + elt.gdalOffset) =
                            CPL_SWAP16(*reinterpret_cast<const uint16_t*>(pSrc + elt.nativeOffset));
                    }
                    else if( elt.nativeSize == 4 )
                    {
                        *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                            CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.nativeOffset));
                    }
                    else if( elt.nativeSize == 8 )
                    {
                        if( elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP )
                        {
                            *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                                CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.nativeOffset));
                            *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset + 4) =
                                CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.nativeOffset + 4));
                        }
                        else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT )
                        {
                            CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                            *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                                CPL_SWAP64(*reinterpret_cast<const int64_t*>(pSrc + elt.nativeOffset)));
                        }
                        else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT )
                        {
                            CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                            *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                                CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.nativeOffset)));
                        }
                        else
                        {
                            *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                                CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.nativeOffset));
                        }
                    }
                    else if( elt.nativeSize == 16 )
                    {
                        *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                            CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.nativeOffset));
                        *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset + 8) =
                            CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.nativeOffset + 8));
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if( elt.gdalTypeIsApproxOfNative )
                {
                    if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                        elt.nativeSize == 1 )
                    {
                        CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Int16 );
                        int16_t intVal = *reinterpret_cast<const int8_t*>(pSrc + elt.nativeOffset);
                        memcpy(pDst + elt.gdalOffset, &intVal, sizeof(intVal));
                    }
                    else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                             elt.nativeSize == 8 )
                    {
                        CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                        int64_t intVal;
                        memcpy(&intVal, pSrc + elt.nativeOffset, sizeof(intVal));
                        double dblVal = static_cast<double>(intVal);
                        memcpy(pDst + elt.gdalOffset, &dblVal, sizeof(dblVal));
                    }
                    else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT &&
                             elt.nativeSize == 8 )
                    {
                        CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                        uint64_t intVal;
                        memcpy(&intVal, pSrc + elt.nativeOffset, sizeof(intVal));
                        double dblVal = static_cast<double>(intVal);
                        memcpy(pDst + elt.gdalOffset, &dblVal, sizeof(dblVal));
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if( elt.nativeType == DtypeElt::NativeType::STRING )
                {
                    char* ptr;
                    char** pDstPtr = reinterpret_cast<char**>(pDst + elt.gdalOffset);
                    memcpy(&ptr, pDstPtr, sizeof(ptr));
                    VSIFree(ptr);

                    char* pDstStr = static_cast<char*>(CPLMalloc(elt.nativeSize + 1));
                    memcpy(pDstStr, pSrc + elt.nativeOffset, elt.nativeSize);
                    pDstStr[elt.nativeSize] = 0;
                    memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
                }
                else
                {
                    CPLAssert( elt.nativeSize == elt.gdalSize );
                    memcpy(pDst + elt.gdalOffset,
                           pSrc + elt.nativeOffset,
                           elt.nativeSize);
                }
            }
        }
    }

    return bRet;
}

/************************************************************************/
/*                           ZarrArray::IRead()                         */
/************************************************************************/

bool ZarrArray::IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const
{

    // Need to be kept in top-level scope
    std::vector<GUInt64> arrayStartIdxMod;
    std::vector<GInt64> arrayStepMod;
    std::vector<GPtrDiff_t> bufferStrideMod;

    const size_t nDims = m_aoDims.size();
    bool negativeStep = false;
    for( size_t i = 0; i < nDims; ++i )
    {
        if( arrayStep[i] < 0 )
        {
            negativeStep = true;
            break;
        }
    }

    //const auto eBufferDT = bufferDataType.GetNumericDataType();
    const auto nBufferDTSize = static_cast<int>(bufferDataType.GetSize());

    // Make sure that arrayStep[i] are positive for sake of simplicity
    if( negativeStep )
    {
        arrayStartIdxMod.resize(nDims);
        arrayStepMod.resize(nDims);
        bufferStrideMod.resize(nDims);
        for( size_t i = 0; i < nDims; ++i )
        {
            if( arrayStep[i] < 0 )
            {
                arrayStartIdxMod[i] = arrayStartIdx[i] - (count[i] - 1) * (-arrayStep[i]);
                arrayStepMod[i] = -arrayStep[i];
                bufferStrideMod[i] = -bufferStride[i];
                pDstBuffer = static_cast<GByte*>(pDstBuffer) +
                    bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize * (count[i] - 1));
            }
            else
            {
                arrayStartIdxMod[i] = arrayStartIdx[i];
                arrayStepMod[i] = arrayStep[i];
                bufferStrideMod[i] = bufferStride[i];
            }
        }
        arrayStartIdx = arrayStartIdxMod.data();
        arrayStep = arrayStepMod.data();
        bufferStride = bufferStrideMod.data();
    }

    std::vector<uint64_t> indicesOuterLoop(nDims);
    std::vector<GByte*> dstPtrStackOuterLoop(nDims + 1);

    std::vector<uint64_t> indicesInnerLoop(nDims);
    std::vector<GByte*> dstPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> dstBufferStrideBytes;
    for( size_t i = 0; i < nDims; ++i )
    {
        dstBufferStrideBytes.push_back(
            bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize));
    }

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT =
        m_oType.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    std::vector<GByte> abyTargetNoData;
    bool bNoDataIsZero = false;

    size_t dimIdx = 0;
    dstPtrStackOuterLoop[0] = static_cast<GByte*>(pDstBuffer);
lbl_next_depth:
    if( dimIdx == nDims )
    {
        size_t dimIdxSubLoop = 0;
        dstPtrStackInnerLoop[0] = dstPtrStackOuterLoop[nDims];
        bool bEmptyTile = false;
        if( tileIndices == m_anCachedTiledIndices )
        {
            if( !m_bCachedTiledValid )
                return false;
            bEmptyTile = m_bCachedTiledEmpty;
        }
        else
        {
            m_anCachedTiledIndices = tileIndices;
            m_bCachedTiledValid = LoadTileData(tileIndices, bEmptyTile);
            if( !m_bCachedTiledValid )
            {
                return false;
            }
            m_bCachedTiledEmpty = bEmptyTile;
        }

        const GByte* pabySrcTile = m_abyDecodedTileData.empty() ?
                        m_abyRawTileData.data(): m_abyDecodedTileData.data();
        const size_t nSrcDTSize = m_abyDecodedTileData.empty() ? nSourceSize : nDTSize;

        for( size_t i = 0; i < nDims; ++i )
        {
            countInnerLoopInit[i] = 1;
            if( arrayStep[i] != 0 )
            {
                const auto nextBlockIdx = std::min(
                    (1 + indicesOuterLoop[i] / m_anBlockSize[i]) * m_anBlockSize[i],
                    arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(
                        (nextBlockIdx - indicesOuterLoop[i] + arrayStep[i] - 1) / arrayStep[i]);
            }
        }

        if( bEmptyTile && bBothAreNumericDT && abyTargetNoData.empty() )
        {
            abyTargetNoData.resize(nBufferDTSize);
            if( m_pabyNoData )
            {
                GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                &abyTargetNoData[0], bufferDataType);
                bNoDataIsZero = true;
                for( size_t i = 0; i < abyTargetNoData.size(); ++i )
                {
                    if( abyTargetNoData[i] != 0 )
                        bNoDataIsZero = false;
                }
            }
            else
            {
                bNoDataIsZero = true;
                GByte zero = 0;
                GDALCopyWords(&zero, GDT_Byte, 0,
                              &abyTargetNoData[0],
                              bufferDataType.GetNumericDataType(), 0,
                              1);
            }
        }

lbl_next_depth_inner_loop:
        if( dimIdxSubLoop == nDims - 1 )
        {
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            void* dst_ptr = dstPtrStackInnerLoop[dimIdxSubLoop];

            if( m_bUseOptimizedCodePaths &&
                bEmptyTile && bBothAreNumericDT && bNoDataIsZero &&
                nBufferDTSize == dstBufferStrideBytes[dimIdxSubLoop] )
            {
                memset(dst_ptr, 0, nBufferDTSize * countInnerLoopInit[dimIdxSubLoop]);
                goto end_inner_loop;
            }
            else if( m_bUseOptimizedCodePaths &&
                bEmptyTile && !abyTargetNoData.empty() && bBothAreNumericDT &&
                     dstBufferStrideBytes[dimIdxSubLoop] < std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( abyTargetNoData.data(),
                                 bufferDataType.GetNumericDataType(),
                                 0,
                                 dst_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );
                goto end_inner_loop;
            }
            else if( bEmptyTile )
            {
                for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                        ++i,
                        dst_ptr = static_cast<uint8_t*>(dst_ptr) + dstBufferStrideBytes[dimIdxSubLoop] )
                {
                    if( bNoDataIsZero )
                    {
                        if( nBufferDTSize == 1 )
                        {
                            *static_cast<uint8_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 2 )
                        {
                            *static_cast<uint16_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 4 )
                        {
                            *static_cast<uint32_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 8 )
                        {
                            *static_cast<uint64_t*>(dst_ptr) = 0;
                        }
                        else if( nBufferDTSize == 16 )
                        {
                            static_cast<uint64_t*>(dst_ptr)[0] = 0;
                            static_cast<uint64_t*>(dst_ptr)[1] = 0;
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                    }
                    else if( m_pabyNoData )
                    {
                        if( bBothAreNumericDT )
                        {
                            const void* src_ptr_v = abyTargetNoData.data();
                            if( nBufferDTSize == 1 )
                                *static_cast<uint8_t*>(dst_ptr) = *static_cast<const uint8_t*>(src_ptr_v);
                            else if( nBufferDTSize == 2 )
                                *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr_v);
                            else if( nBufferDTSize == 4 )
                                *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr_v);
                            else if( nBufferDTSize == 8 )
                                *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr_v);
                            else if( nBufferDTSize == 16 )
                            {
                                static_cast<uint64_t*>(dst_ptr)[0] = static_cast<const uint64_t*>(src_ptr_v)[0];
                                static_cast<uint64_t*>(dst_ptr)[1] = static_cast<const uint64_t*>(src_ptr_v)[1];
                            }
                            else
                            {
                                CPLAssert(false);
                            }
                        }
                        else
                        {
                            GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                            dst_ptr, bufferDataType);
                        }
                    }
                    else
                    {
                        memset(dst_ptr, 0, nBufferDTSize);
                    }
                }

                goto end_inner_loop;
            }

            size_t nOffset = 0;
            for( size_t i = 0; i < nDims; i++ )
            {
                nOffset = static_cast<size_t>(nOffset * m_anBlockSize[i] +
                    (indicesInnerLoop[i] - tileIndices[i] * m_anBlockSize[i]));
            }
            const GByte* src_ptr = pabySrcTile + nOffset * nSrcDTSize;

            if( m_bUseOptimizedCodePaths && bBothAreNumericDT &&
                arrayStep[dimIdxSubLoop] <= static_cast<GIntBig>(std::numeric_limits<int>::max() / nDTSize) &&
                dstBufferStrideBytes[dimIdxSubLoop] <= std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( src_ptr,
                                 m_oType.GetNumericDataType(),
                                 static_cast<int>(arrayStep[dimIdxSubLoop] * nDTSize),
                                 dst_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );

                goto end_inner_loop;
            }

            for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                    ++i,
                    src_ptr += arrayStep[dimIdxSubLoop] * nSrcDTSize,
                    dst_ptr = static_cast<uint8_t*>(dst_ptr) + dstBufferStrideBytes[dimIdxSubLoop] )
            {
                if( bSameNumericDT )
                {
                    const void* src_ptr_v = src_ptr;
                    if( nSameDTSize == 1 )
                        *static_cast<uint8_t*>(dst_ptr) = *static_cast<const uint8_t*>(src_ptr_v);
                    else if( nSameDTSize == 2 )
                    {
                        *static_cast<uint16_t*>(dst_ptr) = *static_cast<const uint16_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 4 )
                    {
                        *static_cast<uint32_t*>(dst_ptr) = *static_cast<const uint32_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 8 )
                    {
                        *static_cast<uint64_t*>(dst_ptr) = *static_cast<const uint64_t*>(src_ptr_v);
                    }
                    else if( nSameDTSize == 16 )
                    {
                        static_cast<uint64_t*>(dst_ptr)[0] = static_cast<const uint64_t*>(src_ptr_v)[0];
                        static_cast<uint64_t*>(dst_ptr)[1] = static_cast<const uint64_t*>(src_ptr_v)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if( m_oType.GetClass() == GEDTC_STRING )
                {
                    char* pDstStr = static_cast<char*>(CPLMalloc(nSourceSize + 1));
                    memcpy(pDstStr, src_ptr, nSourceSize);
                    pDstStr[nSourceSize] = 0;
                    char** pDstPtr = static_cast<char**>(dst_ptr);
                    memcpy(pDstPtr, &pDstStr, sizeof(char*));
                }
                else
                {
                    GDALExtendedDataType::CopyValue(src_ptr, m_oType,
                                                    dst_ptr, bufferDataType);
                }
            }
        }
        else
        {
            // This level of loop loops over individual samples, within a
            // block
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            countInnerLoop[dimIdxSubLoop] = countInnerLoopInit[dimIdxSubLoop];
            while(true)
            {
                dimIdxSubLoop ++;
                dstPtrStackInnerLoop[dimIdxSubLoop] = dstPtrStackInnerLoop[dimIdxSubLoop-1];
                goto lbl_next_depth_inner_loop;
lbl_return_to_caller_inner_loop:
                dimIdxSubLoop --;
                -- countInnerLoop[dimIdxSubLoop];
                if( countInnerLoop[dimIdxSubLoop] == 0 )
                {
                    break;
                }
                indicesInnerLoop[dimIdxSubLoop] += arrayStep[dimIdxSubLoop];
                dstPtrStackInnerLoop[dimIdxSubLoop] += dstBufferStrideBytes[dimIdxSubLoop];
            }
        }
end_inner_loop:
        if( dimIdxSubLoop > 0 )
            goto lbl_return_to_caller_inner_loop;
    }
    else
    {
        // This level of loop loops over blocks
        indicesOuterLoop[dimIdx] = arrayStartIdx[dimIdx];
        tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        while(true)
        {
            dimIdx ++;
            dstPtrStackOuterLoop[dimIdx] = dstPtrStackOuterLoop[dimIdx - 1];
            goto lbl_next_depth;
lbl_return_to_caller:
            dimIdx --;
            if( count[dimIdx] == 1 || arrayStep[dimIdx] == 0 )
                break;

            size_t nIncr;
            if( static_cast<GUInt64>(arrayStep[dimIdx]) < m_anBlockSize[dimIdx] )
            {
                // Compute index at next block boundary
                auto newIdx = indicesOuterLoop[dimIdx] +
                    (m_anBlockSize[dimIdx] - (indicesOuterLoop[dimIdx] % m_anBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>(
                    (newIdx - indicesOuterLoop[dimIdx] + arrayStep[dimIdx] - 1) / arrayStep[dimIdx]);
            }
            else
            {
                nIncr = 1;
            }
            indicesOuterLoop[dimIdx] += nIncr * arrayStep[dimIdx];
            if( indicesOuterLoop[dimIdx] > arrayStartIdx[dimIdx] + (count[dimIdx]-1) * arrayStep[dimIdx] )
                break;
            dstPtrStackOuterLoop[dimIdx] += bufferStride[dimIdx] * static_cast<GPtrDiff_t>(nIncr * nBufferDTSize);
            tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ZarrDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( !poOpenInfo->bIsDirectory )
    {
        return FALSE;
    }

    // TODO: groups, consolitated metadata
    CPLString osMDFilename = CPLFormFilename( poOpenInfo->pszFilename,
                                              ".zarray", nullptr );

    VSIStatBufL sStat;
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                             ParseDtype()                             */
/************************************************************************/

static GDALExtendedDataType ParseDtype(const CPLJSONObject& obj,
                                       std::vector<DtypeElt>& elts)
{
    const auto AlignOffsetOn = [](size_t offset, size_t alignment)
    {
        return offset + (alignment - (offset % alignment)) % alignment;
    };

    do
    {
        if( obj.GetType() == CPLJSONObject::Type::String )
        {
            const auto str = obj.ToString();
            if( str.size() < 3 )
                break;
            const char chEndianness = str[0];

            DtypeElt elt;
            if( chEndianness == '<' )
                elt.needByteSwapping = (CPL_IS_LSB == 0);
            else if( chEndianness == '>' )
                elt.needByteSwapping = (CPL_IS_LSB != 0);

            const char chType = str[1];
            const int nBytes = atoi(str.c_str() + 2);
            GDALDataType eDT;
            if( !elts.empty() )
            {
                elt.nativeOffset = elts.back().nativeOffset + elts.back().nativeSize;
                elt.gdalOffset = elts.back().gdalOffset + elts.back().gdalSize;
            }
            elt.nativeSize = nBytes;
            if( chType == 'b' && nBytes == 1 ) // boolean
            {
                elt.nativeType = DtypeElt::NativeType::BOOLEAN;
                eDT = GDT_Byte;
            }
            else if( chType == 'u' && nBytes == 1 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_Byte;
            }
            else if( chType == 'i' && nBytes == 1 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Int16;
            }
            else if( chType == 'i' && nBytes == 2 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int16;
            }
            else if( chType == 'i' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int32;
            }
            else if( chType == 'i' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float64;
            }
            else if( chType == 'u' && nBytes == 2 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt16;
            }
            else if( chType == 'u' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt32;
            }
            else if( chType == 'u' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float64;
            }
            else if( chType == 'f' && nBytes == 4 )
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float32;
            }
            else if( chType == 'f' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float64;
            }
            else if( chType == 'c' && nBytes == 8 )
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat32;
            }
            else if( chType == 'c' && nBytes == 16 )
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat64;
            }
            else if( chType == 'S' )
            {
                elt.nativeType = DtypeElt::NativeType::STRING;
                elt.gdalType = GDALExtendedDataType::CreateString(nBytes);
                elt.gdalSize = elt.gdalType.GetSize();
                elts.emplace_back(elt);
                return GDALExtendedDataType::CreateString(nBytes);
            }
            else
                break;
            elt.gdalType = GDALExtendedDataType::Create(eDT);
            elt.gdalSize = elt.gdalType.GetSize();
            elt.gdalOffset = AlignOffsetOn(elt.gdalOffset, elt.gdalSize);
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
        else if( obj.GetType() == CPLJSONObject::Type::Array )
        {
            bool error = false;
            const auto oArray = obj.ToArray();
            std::vector<std::unique_ptr<GDALEDTComponent>> comps;
            size_t offset = 0;
            for( const auto& oElt: oArray )
            {
                const auto oEltArray = oElt.ToArray();
                if( !oEltArray.IsValid() || oEltArray.Size() != 2 ||
                    oEltArray[0].GetType() != CPLJSONObject::Type::String )
                {
                    error = true;
                    break;
                }
                GDALExtendedDataType subDT = ParseDtype(oEltArray[1], elts);
                if( subDT.GetClass() == GEDTC_NUMERIC &&
                    subDT.GetNumericDataType() == GDT_Unknown )
                {
                    error = true;
                    break;
                }
                if( subDT.GetClass() == GEDTC_STRING )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "String in compound type not supported");
                    return GDALExtendedDataType::Create(GDT_Unknown);
                }
                const std::string osName = oEltArray[0].ToString();
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osName, offset, subDT)));
                offset += subDT.GetSize();
                if( subDT.GetClass() != GEDTC_STRING )
                {
                    // Add padding for alignment
                    offset = AlignOffsetOn(offset, subDT.GetSize());
                }
            }
            if( error )
                break;
            size_t nTotalSize = offset;
            if( !elts.empty() && elts[0].gdalType.GetClass() != GEDTC_STRING )
            {
                nTotalSize = AlignOffsetOn(nTotalSize, elts[0].gdalSize);
            }
            return GDALExtendedDataType::Create(obj.ToString(),
                                                nTotalSize,
                                                std::move(comps));
        }
    }
    while(false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for dtype");
    return GDALExtendedDataType::Create(GDT_Unknown);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* ZarrDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) ||
        (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) == 0 )
    {
        return nullptr;
    }

    CPLJSONDocument oDoc;
    const std::string osZarrayFilename(
                CPLFormFilename(poOpenInfo->pszFilename, ".zarray", nullptr));
    if( !oDoc.Load(osZarrayFilename) )
        return nullptr;
    const auto oRoot = oDoc.GetRoot();

    const auto osFormat = oRoot["zarr_format"].ToString();
    if( osFormat != "2" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Only zarr_format=2 supported");
        return nullptr;
    }

    bool bFortranOrder = false;
    const auto osOrder = oRoot["order"].ToString();
    if( osOrder == "C" )
    {
        // ok
    }
    else if( osOrder == "F" )
    {
        bFortranOrder = true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for order");
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if( !oShape.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    const auto oChunks = oRoot["chunks"].ToArray();
    if( !oChunks.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "chunks missing or not an array");
        return nullptr;
    }

    if( oShape.Size() != oChunks.Size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }
    if( oShape.Size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "empty shape array not supported");
        return nullptr;
    }

    std::vector<std::shared_ptr<GDALDimension>> aoDims;
    for( int i = 0; i < oShape.Size(); ++i )
    {
        const auto nSize = static_cast<GUInt64>(oShape[i].ToLong());
        if( nSize == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for shape");
            return nullptr;
        }
        aoDims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), CPLSPrintf("dim%d", i),
            std::string(), std::string(), nSize));
    }

    const auto oDtype = oRoot["dtype"];
    if( !oDtype.IsValid() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "dtype missing");
        return nullptr;
    }
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtype(oDtype, aoDtypeElts);
    if( oType.GetClass() == GEDTC_NUMERIC && oType.GetNumericDataType() == GDT_Unknown )
        return nullptr;

    if( oType.GetClass() == GEDTC_COMPOUND )
    {
        bool bFoundSignedInt1Byte = false;
        for( const auto& elt: aoDtypeElts )
        {
            if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                elt.nativeSize == 1 )
            {
                bFoundSignedInt1Byte = true;
                break;
            }
        }
        if( bFoundSignedInt1Byte )
        {
            // Would complicate IRead() implementation
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compound data type with signed integer of "
                     "1 byte not supported");
            return nullptr;
        }
    }

    std::vector<GUInt64> anBlockSize;
    size_t nBlockSize = oType.GetSize();
    for( const auto& item: oChunks )
    {
        const auto nSize = static_cast<GUInt64>(item.ToLong());
        if( nSize == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for chunks");
            return nullptr;
        }
        if( nBlockSize > std::numeric_limits<size_t>::max() / nSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large chunks");
            return nullptr;
        }
        nBlockSize *= static_cast<size_t>(nSize);
        anBlockSize.emplace_back(nSize);
    }

    std::string osDimSeparator = oRoot["dimension_separator"].ToString();
    if( osDimSeparator.empty() )
        osDimSeparator = ".";

    const auto oFillValue = oRoot["fill_value"];
    if( !oFillValue.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "fill_value missing");
        return nullptr;
    }
    const auto eFillValueType = oFillValue.GetType();
    bool bHasNoData = false;
    std::vector<GByte> abyNoData;
    std::string osNoData;
    if( eFillValueType == CPLJSONObject::Type::Null )
    {
        // Nothing to do
    }
    else if( eFillValueType == CPLJSONObject::Type::String )
    {
        const auto osFillValue = oFillValue.ToString();
        if( oType.GetClass() == GEDTC_NUMERIC )
        {
            double dfNoDataValue;
            if( osFillValue == "NaN" )
            {
                dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
            }
            else if( osFillValue == "Infinity" )
            {
                dfNoDataValue = std::numeric_limits<double>::infinity();
            }
            else if( osFillValue == "-Infinity" )
            {
                dfNoDataValue = -std::numeric_limits<double>::infinity();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            if( oType.GetNumericDataType() == GDT_Float32 )
            {
                bHasNoData = true;
                const float fNoDataValue = static_cast<float>(dfNoDataValue);
                abyNoData.resize(sizeof(fNoDataValue));
                memcpy( &abyNoData[0], &fNoDataValue, sizeof(fNoDataValue) );
            }
            else if( oType.GetNumericDataType() == GDT_Float64 )
            {
                bHasNoData = true;
                abyNoData.resize(sizeof(dfNoDataValue));
                memcpy( &abyNoData[0], &dfNoDataValue, sizeof(dfNoDataValue) );
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
        }
        else if( oType.GetClass() == GEDTC_STRING )
        {
            bHasNoData = true;
            osNoData = osFillValue;
            int nBytes = CPLBase64DecodeInPlace(reinterpret_cast<GByte*>(&osNoData[0]));
            osNoData.resize(nBytes);
        }
        else
        {
            // TODO compound types. Base64 encoding
        }
    }
    else if( eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double )
    {
        if( oType.GetClass() == GEDTC_NUMERIC )
        {
            bHasNoData = true;
            const double dfNoDataValue = oFillValue.ToDouble();
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                          &abyNoData[0], oType.GetNumericDataType(), 0,
                          1);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
        return nullptr;
    }

    const auto oCompressor = oRoot["compressor"];
    if( !oCompressor.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "compressor missing");
        return nullptr;
    }
    const CPLCompressor* psDecompressor = nullptr;
    if( oCompressor.GetType() == CPLJSONObject::Type::Null )
    {
        // nothing to do
    }
    else if( oCompressor.GetType() == CPLJSONObject::Type::Object )
    {
        const auto osCompressorId = oCompressor["id"].ToString();
        if( osCompressorId.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing compressor id");
            return nullptr;
        }
        psDecompressor = CPLGetDecompressor( osCompressorId.c_str() );
        if( psDecompressor == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Decompressor %s not handled",
                     osCompressorId.c_str());
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
        return nullptr;
    }

    const auto oFilters = oRoot["filters"];
    if( !oFilters.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "filters missing");
        return nullptr;
    }
    if( oFilters.GetType() == CPLJSONObject::Type::Null )
    {
    }
    else
    {
        // TODO
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported filters");
        return nullptr;
    }

    // TODO
    if( oType.GetClass() == GEDTC_COMPOUND )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Compound data type not yet supported");
        return nullptr;
    }

    auto poDS = std::unique_ptr<ZarrDataset>(new ZarrDataset());
    auto poRG = std::make_shared<ZarrGroup>(std::string(), "/");
    poDS->m_poRootGroup = poRG;
    auto poArray = ZarrArray::Create(poRG->GetFullName(),
                                     CPLGetBasename(poOpenInfo->pszFilename),
                                     aoDims, oType, aoDtypeElts, anBlockSize,
                                     bFortranOrder);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
    poArray->SetDecompressor(psDecompressor);
    if( bHasNoData )
    {
        if( oType.GetClass() == GEDTC_STRING )
        {
            const char* pszNoData = osNoData.data();
            poArray->RegisterNoDataValue(&pszNoData);
        }
        else
        {
            poArray->RegisterNoDataValue(abyNoData.data());
        }
    }
    poRG->RegisterArray(poArray);
    return poDS.release();
}


/************************************************************************/
/*                           ZarrDriver()                               */
/************************************************************************/

class ZarrDriver final: public GDALDriver
{
public:
    const char* GetMetadataItem(const char* pszName, const char* pszDomain) override;
};

const char* ZarrDriver::GetMetadataItem(const char* pszName, const char* pszDomain)
{
    if( (pszDomain == nullptr || pszDomain[0] == '\0') &&
        EQUAL(pszName, "COMPRESSORS") )
    {
        // A bit of a hack. Normally GetMetadata() should also return it,
        // but as this is only used for tests, just make GetMetadataItem()
        // handle it
        std::string osCompressors;
        char** decompressors = CPLGetDecompressors();
        for( auto iter = decompressors; iter && *iter; ++iter )
        {
            if( !osCompressors.empty() )
                osCompressors += ',';
            osCompressors += *iter;
        }
        CSLDestroy(decompressors);
        return CPLSPrintf("%s", osCompressors.c_str());
    }
    return GDALDriver::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if( GDALGetDriverByName( "Zarr" ) != nullptr )
        return;

    GDALDriver *poDriver = new ZarrDriver();

    poDriver->SetDescription( "Zarr" );
    // poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Zarr" );
    //poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
    //                           "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = ZarrDataset::Identify;
    poDriver->pfnOpen = ZarrDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
