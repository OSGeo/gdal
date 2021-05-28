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

#include "cpl_json.h"
#include "gdal_priv.h"

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

    ZarrArray(const std::string& osParentName,
              const std::string& osName,
              const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
              const GDALExtendedDataType& oType,
              const std::vector<DtypeElt>& aoDtypeElts,
              const std::vector<GUInt64>& anBlockSize);

    bool LoadTileData(const std::vector<uint64_t>& tileIndices,
                      std::vector<GByte>& abyTileData,
                      bool& bMissingTileOut) const;

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
                                             const std::vector<GUInt64>& anBlockSize);

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_osFilename; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_aoDims; }

    const GDALExtendedDataType& GetDataType() const override { return m_oType; }

    std::vector<GUInt64> GetBlockSize() const override { return m_anBlockSize; }

    const void* GetRawNoDataValue() const override { return m_pabyNoData; }

    void RegisterNoDataValue(const void*);

    void SetFilename(const std::string& osFilename ) { m_osFilename = osFilename; }

    void SetDimSeparator(const std::string& osDimSeparator) { m_osDimSeparator = osDimSeparator; }
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
                     const std::vector<GUInt64>& anBlockSize):
    GDALAbstractMDArray(osParentName, osName),
    GDALMDArray(osParentName, osName),
    m_aoDims(aoDims),
    m_oType(oType),
    m_aoDtypeElts(aoDtypeElts),
    m_anBlockSize(anBlockSize)
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
                                             const std::vector<GUInt64>& anBlockSize)
{
    auto arr = std::shared_ptr<ZarrArray>(
        new ZarrArray(osParentName, osName, aoDims, oType, aoDtypeElts,
                      anBlockSize));
    arr->SetSelf(arr);
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
/*                        ZarrArray::LoadTileData()                     */
/************************************************************************/

bool ZarrArray::LoadTileData(const std::vector<uint64_t>& tileIndices,
                             std::vector<GByte>& abyTileData,
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
    if( VSIFReadL(&abyTileData[0], 1, abyTileData.size(), fp) != abyTileData.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not read tile %s correctly",
                 osFilename.c_str());
        bRet = false;
    }
    VSIFCloseL(fp);
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

    // Reserve a buffer for tile content
    //const GDALDataType eDT = m_oType.GetNumericDataType();
    const auto nDTSize = m_oType.GetSize();
    std::vector<GByte> abyTileData;
    size_t nTileSize = m_oType.GetClass() == GEDTC_STRING ? m_oType.GetMaxStringLength() : nDTSize;
    for( const auto& nBlockSize: m_anBlockSize )
    {
        nTileSize *= static_cast<size_t>(nBlockSize);
    }
    try
    {
        abyTileData.resize( nTileSize );
    }
    catch( const std::bad_alloc& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    size_t dimIdx = 0;
    dstPtrStackOuterLoop[0] = static_cast<GByte*>(pDstBuffer);
lbl_next_depth:
    if( dimIdx == nDims )
    {
        size_t dimIdxSubLoop = 0;
        dstPtrStackInnerLoop[0] = dstPtrStackOuterLoop[nDims];
        bool bEmptyTile = false;
        if( !LoadTileData(tileIndices, abyTileData, bEmptyTile) )
            return false;

lbl_next_depth_inner_loop:
        if( dimIdxSubLoop == nDims )
        {
            void* dst_ptr = dstPtrStackInnerLoop[dimIdxSubLoop];
            if( bEmptyTile )
            {
                if( m_pabyNoData )
                {
                    GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                    dst_ptr, bufferDataType);
                }
                else
                {
                    memset(dst_ptr, 0, nBufferDTSize);
                }
            }
            else
            {
                size_t nOffset = 0;
                for( size_t i = 0; i < nDims; i++ )
                {
                    nOffset = static_cast<size_t>(nOffset * m_anBlockSize[i] +
                        (indicesInnerLoop[i] - tileIndices[i] * m_anBlockSize[i]));
                }
                GByte* src_ptr = &abyTileData[0] + nOffset * nSourceSize;
                if( m_oType.GetClass() == GEDTC_STRING )
                {
                    char* pDstStr = static_cast<char*>(CPLMalloc(nSourceSize + 1));
                    memcpy(pDstStr, src_ptr, nSourceSize);
                    pDstStr[nSourceSize] = 0;
                    char** pDstPtr = static_cast<char**>(dst_ptr);
                    memcpy(pDstPtr, &pDstStr, sizeof(char*));
                }
                else
                {
                    for( auto& elt: m_aoDtypeElts )
                    {
                        if( elt.needByteSwapping )
                        {
                            if( elt.nativeSize == 2 )
                                CPL_SWAP16PTR(src_ptr + elt.nativeOffset);
                            else if( elt.nativeSize == 4 )
                                CPL_SWAP32PTR(src_ptr + elt.nativeOffset);
                            else if( elt.nativeSize == 8 )
                            {
                                if( elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP )
                                {
                                    CPL_SWAP32PTR(src_ptr + elt.nativeOffset);
                                    CPL_SWAP32PTR(src_ptr + elt.nativeOffset + 4);
                                }
                                else
                                {
                                    CPL_SWAP64PTR(src_ptr + elt.nativeOffset);
                                }
                            }
                            else if( elt.nativeSize == 16 )
                            {
                                CPL_SWAP64PTR(src_ptr + elt.nativeOffset);
                                CPL_SWAP64PTR(src_ptr + elt.nativeOffset + 8);
                            }
                        }
                    }
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
            while(true)
            {
                dimIdxSubLoop ++;
                dstPtrStackInnerLoop[dimIdxSubLoop] = dstPtrStackInnerLoop[dimIdxSubLoop-1];
                goto lbl_next_depth_inner_loop;
lbl_return_to_caller_inner_loop:
                dimIdxSubLoop --;
                if( arrayStep[dimIdxSubLoop] == 0 )
                {
                    break;
                }
                const auto oldBlock = indicesOuterLoop[dimIdxSubLoop] / m_anBlockSize[dimIdxSubLoop];
                indicesInnerLoop[dimIdxSubLoop] += arrayStep[dimIdxSubLoop];
                if( (indicesInnerLoop[dimIdxSubLoop] /
                        m_anBlockSize[dimIdxSubLoop]) != oldBlock ||
                    indicesInnerLoop[dimIdxSubLoop] >
                        arrayStartIdx[dimIdxSubLoop] + (count[dimIdxSubLoop]-1) * arrayStep[dimIdxSubLoop] )
                {
                    break;
                }
                dstPtrStackInnerLoop[dimIdxSubLoop] +=
                    bufferStride[dimIdxSubLoop] * static_cast<GPtrDiff_t>(nBufferDTSize);
            }
        }
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

    // TODO: handle F
    const auto osOrder = oRoot["order"].ToString();
    if( osOrder != "C" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Only order=C supported");
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
    if( oCompressor.GetType() == CPLJSONObject::Type::Null )
    {
    }
    else
    {
        // TODO
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported compressor");
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
                                     aoDims, oType, aoDtypeElts, anBlockSize);
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
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
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if( GDALGetDriverByName( "Zarr" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

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
