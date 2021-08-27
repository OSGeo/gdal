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

#include "cpl_vsi_virtual.h"
#include "zarr.h"
#include "gdal_thread_pool.h"

#include "tif_float.h"

#include "netcdf_cf_constants.h" // for CF_UNITS, etc

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <set>

#define ZARR_DEBUG_KEY "ZARR"

/************************************************************************/
/*                         ZarrArray::ZarrArray()                       */
/************************************************************************/

ZarrArray::ZarrArray(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                     const std::string& osParentName,
                     const std::string& osName,
                     const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                     const GDALExtendedDataType& oType,
                     const std::vector<DtypeElt>& aoDtypeElts,
                     const std::vector<GUInt64>& anBlockSize,
                     bool bFortranOrder):
    GDALAbstractMDArray(osParentName, osName),
    GDALPamMDArray(osParentName, osName, poSharedResource->GetPAM()),
    m_poSharedResource(poSharedResource),
    m_aoDims(aoDims),
    m_oType(oType),
    m_aoDtypeElts(aoDtypeElts),
    m_anBlockSize(anBlockSize),
    m_bFortranOrder(bFortranOrder),
    m_oAttrGroup(osParentName)
{
    m_oCompressorJSonV2.Deinit();
    m_oCompressorJSonV3.Deinit();

    // Compute individual tile size
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;
    m_nTileSize = m_oType.GetClass() == GEDTC_STRING ?
                                m_oType.GetMaxStringLength() : nSourceSize;
    for( const auto& nBlockSize: m_anBlockSize )
    {
        m_nTileSize *= static_cast<size_t>(nBlockSize);
    }
}

/************************************************************************/
/*                          ZarrArray::Create()                         */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrArray::Create(const std::shared_ptr<ZarrSharedResource>& poSharedResource,
                                             const std::string& osParentName,
                                             const std::string& osName,
                                             const std::vector<std::shared_ptr<GDALDimension>>& aoDims,
                                             const GDALExtendedDataType& oType,
                                             const std::vector<DtypeElt>& aoDtypeElts,
                                             const std::vector<GUInt64>& anBlockSize,
                                             bool bFortranOrder)
{
    uint64_t nTotalTileCount = 1;
    for( size_t i = 0; i < aoDims.size(); ++i )
    {
        uint64_t nTileThisDim = (aoDims[i]->GetSize() / anBlockSize[i]) +
                    (((aoDims[i]->GetSize() % anBlockSize[i]) != 0) ? 1 : 0);
        if( nTotalTileCount > std::numeric_limits<uint64_t>::max() / nTileThisDim )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Array %s has more than 2^64 tiles. This is not supported.",
                     osName.c_str());
            return nullptr;
        }
        nTotalTileCount *= nTileThisDim;
    }

    auto arr = std::shared_ptr<ZarrArray>(
        new ZarrArray(poSharedResource,
                      osParentName, osName, aoDims, oType, aoDtypeElts,
                      anBlockSize, bFortranOrder));
    arr->SetSelf(arr);

    arr->m_nTotalTileCount = nTotalTileCount;
    arr->m_bUseOptimizedCodePaths = CPLTestBool(
        CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES"));

    return arr;
}

/************************************************************************/
/*                              ~ZarrArray()                            */
/************************************************************************/

ZarrArray::~ZarrArray()
{
    Flush();

    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }

    DeallocateDecodedTileData();
}

/************************************************************************/
/*                                Flush()                               */
/************************************************************************/

void ZarrArray::Flush()
{
    FlushDirtyTile();
    bool bSerializeV3 = false;

    if( m_bDefinitionModified  )
    {
        if( m_nVersion == 2 )
        {
            SerializeV2();
        }
        else
        {
            bSerializeV3 = true;
        }
        m_bDefinitionModified = false;
    }

    CPLJSONArray j_ARRAY_DIMENSIONS;
    if( !m_aoDims.empty() )
    {
        for( const auto& poDim: m_aoDims )
        {
            if( dynamic_cast<const ZarrArray*>(poDim->GetIndexingVariable().get()) != nullptr )
            {
                j_ARRAY_DIMENSIONS.Add( poDim->GetName() );
            }
            else
            {
                j_ARRAY_DIMENSIONS = CPLJSONArray();
                break;
            }
        }
    }

    CPLJSONObject oAttrs;
    if( m_oAttrGroup.IsModified() ||
        (m_bNew && j_ARRAY_DIMENSIONS.Size() != 0) ||
        m_bUnitModified ||
        m_bOffsetModified ||
        m_bScaleModified ||
        m_bSRSModified )
    {
        m_bNew = false;
        m_bSRSModified = false;
        m_oAttrGroup.UnsetModified();

        oAttrs = m_oAttrGroup.Serialize();

        if( j_ARRAY_DIMENSIONS.Size() != 0 )
        {
            oAttrs.Delete("_ARRAY_DIMENSIONS");
            oAttrs.Add("_ARRAY_DIMENSIONS", j_ARRAY_DIMENSIONS);
        }

        if( m_poSRS )
        {
            CPLJSONObject oCRS;
            const char* const apszOptions[] = { "FORMAT=WKT2_2019", nullptr };
            char* pszWKT = nullptr;
            if( m_poSRS->exportToWkt(&pszWKT, apszOptions) == OGRERR_NONE )
            {
                oCRS.Add("wkt", pszWKT);
            }
            CPLFree(pszWKT);

            {
                CPLErrorHandlerPusher quietError(CPLQuietErrorHandler);
                CPLErrorStateBackuper errorStateBackuper;
                char* projjson = nullptr;
                if( m_poSRS->exportToPROJJSON(&projjson, nullptr) == OGRERR_NONE &&
                    projjson != nullptr )
                {
                    CPLJSONDocument oDocProjJSON;
                    if( oDocProjJSON.LoadMemory(std::string(projjson)) )
                    {
                        oCRS.Add("projjson", oDocProjJSON.GetRoot());
                    }
                }
                CPLFree(projjson);
            }

            const char* pszAuthorityCode = m_poSRS->GetAuthorityCode(nullptr);
            const char* pszAuthorityName = m_poSRS->GetAuthorityName(nullptr);
            if( pszAuthorityCode && pszAuthorityName &&
                EQUAL(pszAuthorityName, "EPSG") )
            {
                oCRS.Add("url",
                         std::string("http://www.opengis.net/def/crs/EPSG/0/") +
                             pszAuthorityCode);
            }

            oAttrs.Add("crs", oCRS);
        }

        if( m_osUnit.empty() )
        {
            if( m_bUnitModified )
                oAttrs.Delete(CF_UNITS);
        }
        else
        {
            oAttrs.Set(CF_UNITS, m_osUnit);
        }
        m_bUnitModified = false;

        if( !m_bHasOffset )
        {
            oAttrs.Delete(CF_ADD_OFFSET);
        }
        else
        {
            oAttrs.Set(CF_ADD_OFFSET, m_dfOffset);
        }
        m_bOffsetModified = false;

        if( !m_bHasScale )
        {
            oAttrs.Delete(CF_SCALE_FACTOR);
        }
        else
        {
            oAttrs.Set(CF_SCALE_FACTOR, m_dfScale);
        }
        m_bScaleModified = false;

        if( m_nVersion == 2 )
        {
            CPLJSONDocument oDoc;
            oDoc.SetRoot(oAttrs);
            const std::string osAttrFilename = CPLFormFilename(
                CPLGetDirname(m_osFilename.c_str()), ".zattrs", nullptr);
            oDoc.Save(osAttrFilename);
            m_poSharedResource->SetZMetadataItem(osAttrFilename, oAttrs);
        }
        else
        {
            bSerializeV3 = true;
        }
    }

    if( bSerializeV3 )
    {
        SerializeV3(oAttrs);
    }
}

/************************************************************************/
/*                      DeallocateDecodedTileData()                     */
/************************************************************************/

void ZarrArray::DeallocateDecodedTileData()
{
    if( !m_abyDecodedTileData.empty() )
    {
        const size_t nDTSize = m_oType.GetSize();
        GByte* pDst = &m_abyDecodedTileData[0];
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        for( auto& elt: m_aoDtypeElts )
        {
            if( elt.nativeType == DtypeElt::NativeType::STRING )
            {
                for( size_t i = 0; i < nValues; i++, pDst += nDTSize )
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
/*                             EncodeElt()                              */
/************************************************************************/

/* Encode from GDAL raw type to Zarr native type */
static void EncodeElt(const std::vector<DtypeElt>& elts,
                      const GByte* pSrc,
                      GByte* pDst)
{
    for( const auto& elt: elts )
    {
        if( elt.needByteSwapping )
        {
            if( elt.nativeSize == 2 )
            {
                if( elt.gdalTypeIsApproxOfNative )
                {
                    CPLAssert( elt.nativeType == DtypeElt::NativeType::IEEEFP );
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float32 );
                    const uint32_t uint32Val =
                        *reinterpret_cast<const uint32_t*>(pSrc + elt.gdalOffset);
                    bool bHasWarned = false;
                    uint16_t uint16Val = CPL_SWAP16(FloatToHalf(uint32Val, bHasWarned));
                    memcpy(pDst + elt.nativeOffset, &uint16Val, sizeof(uint16Val));
                }
                else
                {
                    const uint16_t val =
                        CPL_SWAP16(*reinterpret_cast<const uint16_t*>(pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
            }
            else if( elt.nativeSize == 4 )
            {
                const uint32_t val =
                    CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.gdalOffset));
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
            }
            else if( elt.nativeSize == 8 )
            {
                if( elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP )
                {
                    uint32_t val =
                        CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                    val =
                        CPL_SWAP32(*reinterpret_cast<const uint32_t*>(pSrc + elt.gdalOffset + 4));
                    memcpy(pDst + elt.nativeOffset + 4, &val, sizeof(val));
                }
                else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    const double dbl = *reinterpret_cast<const double*>(pSrc + elt.gdalOffset);
                    int64_t val = CPL_SWAP64(static_cast<int64_t>(dbl));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
                else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    const double dbl = *reinterpret_cast<const double*>(pSrc + elt.gdalOffset);
                    uint64_t val = CPL_SWAP64(static_cast<uint64_t>(dbl));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
                else
                {
                    const uint64_t val = CPL_SWAP64(
                        *reinterpret_cast<const uint64_t*>(pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
            }
            else if( elt.nativeSize == 16 )
            {
                uint64_t val =
                    CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.gdalOffset));
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                val =
                    CPL_SWAP64(*reinterpret_cast<const uint64_t*>(pSrc + elt.gdalOffset + 8));
                memcpy(pDst + elt.nativeOffset + 8, &val, sizeof(val));
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
                const int16_t int16Val = *reinterpret_cast<const int16_t*>(pSrc + elt.gdalOffset);
                const int8_t intVal = static_cast<int8_t>(int16Val);
                memcpy(pDst + elt.nativeOffset, &intVal, sizeof(intVal));
            }
            else if( elt.nativeType == DtypeElt::NativeType::IEEEFP &&
                     elt.nativeSize == 2 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float32 );
                const uint32_t uint32Val =
                    *reinterpret_cast<const uint32_t*>(pSrc + elt.gdalOffset);
                bool bHasWarned = false;
                const uint16_t uint16Val = FloatToHalf(uint32Val, bHasWarned);
                memcpy(pDst + elt.nativeOffset, &uint16Val, sizeof(uint16Val));
            }
            else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT &&
                     elt.nativeSize == 8 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                const double dbl = *reinterpret_cast<const double*>(pSrc + elt.gdalOffset);
                const int64_t val = static_cast<int64_t>(dbl);
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
            }
            else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT &&
                     elt.nativeSize == 8 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                const double dbl = *reinterpret_cast<const double*>(pSrc + elt.gdalOffset);
                const uint64_t val = static_cast<uint64_t>(dbl);
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if( elt.nativeType == DtypeElt::NativeType::STRING )
        {
            const char* pStr = *reinterpret_cast<const char* const*>(pSrc + elt.gdalOffset);
            if( pStr )
            {
                const size_t nLen = strlen(pStr);
                memcpy(pDst + elt.nativeOffset, pStr, std::min(nLen, elt.nativeSize));
                if( nLen < elt.nativeSize )
                    memset(pDst + elt.nativeOffset + nLen, 0, elt.nativeSize - nLen);
            }
            else
            {
                memset(pDst + elt.nativeOffset, 0, elt.nativeSize);
            }
        }
        else
        {
            CPLAssert( elt.nativeSize == elt.gdalSize );
            memcpy(pDst + elt.nativeOffset,
                   pSrc + elt.gdalOffset,
                   elt.nativeSize);
        }
    }
}

/************************************************************************/
/*           StripUselessItemsFromCompressorConfiguration()             */
/************************************************************************/

static void StripUselessItemsFromCompressorConfiguration(CPLJSONObject& o)
{
    o.Delete("num_threads"); // Blosc
    o.Delete("typesize"); // Blosc
    o.Delete("header"); // LZ4
}

/************************************************************************/
/*                    ZarrArray::SerializeV2()                          */
/************************************************************************/

void ZarrArray::SerializeV2()
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    CPLJSONArray oChunks;
    for( const auto nBlockSize: m_anBlockSize )
    {
        oChunks.Add(static_cast<GInt64>(nBlockSize));
    }
    oRoot.Add("chunks", oChunks);

    if( m_oCompressorJSonV2.IsValid() )
    {
        oRoot.Add("compressor", m_oCompressorJSonV2);
        CPLJSONObject compressor = oRoot["compressor"];
        StripUselessItemsFromCompressorConfiguration(compressor);
    }
    else
    {
        oRoot.AddNull("compressor");
    }

    if( m_dtype.GetType() == CPLJSONObject::Type::Object )
        oRoot.Add("dtype", m_dtype["dummy"]);
    else
        oRoot.Add("dtype", m_dtype);

    if( m_pabyNoData == nullptr )
    {
        oRoot.AddNull("fill_value");
    }
    else
    {
        switch( m_oType.GetClass() )
        {
            case GEDTC_NUMERIC:
            {
                const double dfVal = GetNoDataValueAsDouble();
                if( std::isnan(dfVal) )
                    oRoot.Add("fill_value", "NaN");
                else if( dfVal == std::numeric_limits<double>::infinity() )
                    oRoot.Add("fill_value", "Infinity");
                else if( dfVal == -std::numeric_limits<double>::infinity() )
                    oRoot.Add("fill_value", "-Infinity");
                else if( GDALDataTypeIsInteger(m_oType.GetNumericDataType()) )
                    oRoot.Add("fill_value", static_cast<GInt64>(dfVal));
                else
                    oRoot.Add("fill_value", dfVal);
                break;
            }

            case GEDTC_STRING:
            {
                char* pszStr;
                char** ppszStr = reinterpret_cast<char**>(m_pabyNoData);
                memcpy(&pszStr, ppszStr, sizeof(pszStr));
                if( pszStr )
                {
                    const size_t nNativeSize = m_aoDtypeElts.back().nativeOffset +
                                               m_aoDtypeElts.back().nativeSize;
                    char* base64 = CPLBase64Encode(
                        static_cast<int>(std::min(nNativeSize, strlen(pszStr))),
                        reinterpret_cast<const GByte *>(pszStr));
                    oRoot.Add("fill_value", base64);
                    CPLFree(base64);
                }
                else
                {
                    oRoot.AddNull("fill_value");
                }
                break;
            }

            case GEDTC_COMPOUND:
            {
                const size_t nNativeSize = m_aoDtypeElts.back().nativeOffset +
                                           m_aoDtypeElts.back().nativeSize;
                std::vector<GByte> nativeNoData(nNativeSize);
                EncodeElt(m_aoDtypeElts, m_pabyNoData, &nativeNoData[0]);
                char* base64 = CPLBase64Encode(static_cast<int>(nNativeSize),
                                               nativeNoData.data());
                oRoot.Add("fill_value", base64);
                CPLFree(base64);
            }
        }
    }

    if( m_oFiltersArray.Size() == 0 )
        oRoot.AddNull("filters");
    else
        oRoot.Add("filters", m_oFiltersArray);

    oRoot.Add("order", m_bFortranOrder ? "F": "C");

    CPLJSONArray oShape;
    for( const auto& poDim: m_aoDims )
    {
        oShape.Add(static_cast<GInt64>(poDim->GetSize()));
    }
    oRoot.Add("shape", oShape);

    oRoot.Add("zarr_format", m_nVersion);

    if( m_osDimSeparator != "." )
    {
        oRoot.Add("dimension_separator", m_osDimSeparator);
    }

    oDoc.Save(m_osFilename);

    m_poSharedResource->SetZMetadataItem(m_osFilename, oRoot);
}

/************************************************************************/
/*                    ZarrArray::SerializeV3()                          */
/************************************************************************/

void ZarrArray::SerializeV3(const CPLJSONObject& oAttrs)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    CPLJSONArray oShape;
    for( const auto& poDim: m_aoDims )
    {
        oShape.Add(static_cast<GInt64>(poDim->GetSize()));
    }
    oRoot.Add("shape", oShape);

    oRoot.Add("data_type", m_dtype.ToString());

    CPLJSONObject oChunkGrid;
    oChunkGrid.Add("type", "regular");
    CPLJSONArray oChunks;
    for( const auto nBlockSize: m_anBlockSize )
    {
        oChunks.Add(static_cast<GInt64>(nBlockSize));
    }
    oChunkGrid.Add("chunk_shape", oChunks);
    oChunkGrid.Add("separator", m_osDimSeparator);
    oRoot.Add("chunk_grid", oChunkGrid);

    if( m_oCompressorJSonV3.IsValid() )
    {
        oRoot.Add("compressor", m_oCompressorJSonV3);
        CPLJSONObject oConfiguration = oRoot["compressor"]["configuration"];
        StripUselessItemsFromCompressorConfiguration(oConfiguration);
    }

    if( m_pabyNoData == nullptr )
    {
        oRoot.AddNull("fill_value");
    }
    else
    {
        const double dfVal = GetNoDataValueAsDouble();
        if( std::isnan(dfVal) )
            oRoot.Add("fill_value", "NaN");
        else if( dfVal == std::numeric_limits<double>::infinity() )
            oRoot.Add("fill_value", "Infinity");
        else if( dfVal == -std::numeric_limits<double>::infinity() )
            oRoot.Add("fill_value", "-Infinity");
        else if( GDALDataTypeIsInteger(m_oType.GetNumericDataType()) )
            oRoot.Add("fill_value", static_cast<GInt64>(dfVal));
        else
            oRoot.Add("fill_value", dfVal);
    }

    oRoot.Add("chunk_memory_layout", m_bFortranOrder ? "F": "C");

    oRoot.Add("extensions", CPLJSONArray());

    oRoot.Add("attributes", oAttrs);

    oDoc.Save(m_osFilename);
}

/************************************************************************/
/*               ZarrArray::AllocateWorkingBuffers()                    */
/************************************************************************/

bool ZarrArray::AllocateWorkingBuffers() const
{
    if( m_bAllocateWorkingBuffersDone )
        return m_bWorkingBuffersOK;

    m_bAllocateWorkingBuffersDone = true;

    // Reserve a buffer for tile content
    if( m_nTileSize > 1024 * 1024 * 1024 &&
        !CPLTestBool(CPLGetConfigOption("ZARR_ALLOW_BIG_TILE_SIZE", "NO")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Zarr tile allocation would require " CPL_FRMT_GUIB " bytes. "
                 "By default the driver limits to 1 GB. To allow that memory "
                 "allocation, set the ZARR_ALLOW_BIG_TILE_SIZE configuration "
                 "option to YES.", static_cast<GUIntBig>(m_nTileSize));
        return false;
    }

    m_bWorkingBuffersOK = AllocateWorkingBuffers(m_abyRawTileData,
                                                 m_abyTmpRawTileData,
                                                 m_abyDecodedTileData);
    return m_bWorkingBuffersOK;
}

bool ZarrArray::AllocateWorkingBuffers(std::vector<GByte>& abyRawTileData,
                                       std::vector<GByte>& abyTmpRawTileData,
                                       std::vector<GByte>& abyDecodedTileData) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyTmpRawTileData cannot_use_here
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here

    try
    {
        abyRawTileData.resize( m_nTileSize );
        if( m_bFortranOrder || m_oFiltersArray.Size() != 0 )
            abyTmpRawTileData.resize( m_nTileSize );
    }
    catch( const std::bad_alloc& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    bool bNeedDecodedBuffer = false;
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;
    if( m_oType.GetClass() == GEDTC_COMPOUND && nSourceSize != m_oType.GetSize() )
    {
        bNeedDecodedBuffer = true;
    }
    else if( m_oType.GetClass() != GEDTC_STRING )
    {
        for( const auto& elt: m_aoDtypeElts )
        {
            if( elt.needByteSwapping || elt.gdalTypeIsApproxOfNative ||
                elt.nativeType == DtypeElt::NativeType::STRING )
            {
                bNeedDecodedBuffer = true;
                break;
            }
        }
    }
    if( bNeedDecodedBuffer )
    {
        size_t nDecodedBufferSize = m_oType.GetSize();
        for( const auto& nBlockSize: m_anBlockSize )
        {
            nDecodedBufferSize *= static_cast<size_t>(nBlockSize);
        }
        try
        {
            abyDecodedTileData.resize( nDecodedBufferSize );
        }
        catch( const std::bad_alloc& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }
    }

    return true;
#undef m_abyTmpRawTileData
#undef m_abyRawTileData
#undef m_abyDecodedTileData
}

/************************************************************************/
/*                    ZarrArray::GetSpatialRef()                        */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> ZarrArray::GetSpatialRef() const
{
    if( m_poSRS )
        return m_poSRS;
    return GDALPamMDArray::GetSpatialRef();
}

/************************************************************************/
/*                        SetRawNoDataValue()                           */
/************************************************************************/

bool ZarrArray::SetRawNoDataValue(const void* pRawNoData)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array opened in read-only mode");
        return false;
    }
    m_bDefinitionModified = true;
    RegisterNoDataValue(pRawNoData);
    return true;
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
                               std::vector<GByte>& abyDst,
                               bool bDecode) const
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

    if( bDecode )
    {
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
    }
    else
    {
        stack[0].dst_inc_offset = nSourceSize;
        for( size_t i = 1; i < nDims; ++i )
        {
            stack[i].dst_inc_offset = stack[i-1].dst_inc_offset *
                                    static_cast<size_t>(m_anBlockSize[i-1]);
        }

        stack[nDims-1].src_inc_offset = nSourceSize;
        for( size_t i = nDims - 1; i > 0; )
        {
            --i;
            stack[i].src_inc_offset = stack[i+1].src_inc_offset *
                                    static_cast<size_t>(m_anBlockSize[i+1]);
        }
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
/*                        DecodeSourceElt()                             */
/************************************************************************/

static void DecodeSourceElt(const std::vector<DtypeElt>& elts,
                            const GByte* pSrc,
                            GByte* pDst)
{
    for( auto& elt: elts )
    {
        if( elt.needByteSwapping )
        {
            if( elt.nativeSize == 2 )
            {
                uint16_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                if( elt.gdalTypeIsApproxOfNative )
                {
                    CPLAssert( elt.nativeType == DtypeElt::NativeType::IEEEFP );
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float32 );
                    uint32_t uint32Val = HalfToFloat(CPL_SWAP16(val));
                    memcpy(pDst + elt.gdalOffset, &uint32Val, sizeof(uint32Val));
                }
                else
                {
                    *reinterpret_cast<uint16_t*>(pDst + elt.gdalOffset) =
                        CPL_SWAP16(val);
                }
            }
            else if( elt.nativeSize == 4 )
            {
                uint32_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                    CPL_SWAP32(val);
            }
            else if( elt.nativeSize == 8 )
            {
                if( elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP )
                {
                    uint32_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset) =
                        CPL_SWAP32(val);
                    memcpy(&val, pSrc + elt.nativeOffset + 4, sizeof(val));
                    *reinterpret_cast<uint32_t*>(pDst + elt.gdalOffset + 4) =
                        CPL_SWAP32(val);
                }
                else if( elt.nativeType == DtypeElt::NativeType::SIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    int64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                        CPL_SWAP64(val));
                }
                else if( elt.nativeType == DtypeElt::NativeType::UNSIGNED_INT )
                {
                    CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float64 );
                    uint64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<double*>(pDst + elt.gdalOffset) = static_cast<double>(
                        CPL_SWAP64(val));
                }
                else
                {
                    uint64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                        CPL_SWAP64(val);
                }
            }
            else if( elt.nativeSize == 16 )
            {
                uint64_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset) =
                    CPL_SWAP64(val);
                memcpy(&val, pSrc + elt.nativeOffset + 8, sizeof(val));
                *reinterpret_cast<uint64_t*>(pDst + elt.gdalOffset + 8) =
                    CPL_SWAP64(val);
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
            else if( elt.nativeType == DtypeElt::NativeType::IEEEFP &&
                     elt.nativeSize == 2 )
            {
                CPLAssert( elt.gdalType.GetNumericDataType() == GDT_Float32 );
                uint16_t uint16Val;
                memcpy(&uint16Val, pSrc + elt.nativeOffset, sizeof(uint16Val));
                uint32_t uint32Val = HalfToFloat(uint16Val);
                memcpy(pDst + elt.gdalOffset, &uint32Val, sizeof(uint32Val));
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

/************************************************************************/
/*                        ZarrArray::LoadTileData()                     */
/************************************************************************/

bool ZarrArray::LoadTileData(const uint64_t* tileIndices,
                             bool& bMissingTileOut) const
{
    return LoadTileData(tileIndices,
                        false, // use mutex
                        m_psDecompressor,
                        m_abyRawTileData,
                        m_abyTmpRawTileData,
                        m_abyDecodedTileData,
                        bMissingTileOut);
}

bool ZarrArray::LoadTileData(const uint64_t* tileIndices,
                             bool bUseMutex,
                             const CPLCompressor* psDecompressor,
                             std::vector<GByte>& abyRawTileData,
                             std::vector<GByte>& abyTmpRawTileData,
                             std::vector<GByte>& abyDecodedTileData,
                             bool& bMissingTileOut) const
{
    // This method should NOT modify any ZarrArray member, as it is going to
    // be called concurrently from several threads.

    // Set those #define to avoid accidental use of some global variables
#define m_abyTmpRawTileData cannot_use_here
#define m_abyRawTileData cannot_use_here
#define m_abyDecodedTileData cannot_use_here
#define m_psDecompressor cannot_use_here

    bMissingTileOut = false;

    std::string osFilename;
    if( m_aoDims.empty() )
    {
        osFilename = "0";
    }
    else
    {
        for( size_t i = 0; i < m_aoDims.size(); ++i )
        {
            if( !osFilename.empty() )
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(tileIndices[i]);
        }
    }

    if( m_nVersion == 2 )
    {
        osFilename = CPLFormFilename(
            CPLGetDirname(m_osFilename.c_str()), osFilename.c_str(), nullptr);
    }
    else
    {
        std::string osTmp = m_osRootDirectoryName + "/data/root";
        if( GetFullName() != "/" )
            osTmp += GetFullName();
        osFilename = osTmp + "/c" + osFilename;
    }

    // For network file systems, get the streaming version of the filename,
    // as we don't need arbitrary seeking in the file
    osFilename = VSIFileManager::GetHandler( osFilename.c_str() )->GetStreamingFilename(osFilename);

    // First if we have a tile presence cache, check tile presence from it
    if( bUseMutex )
        m_oMutex.lock();
    auto poTilePresenceArray = OpenTilePresenceCache(false);
    if( poTilePresenceArray )
    {
        std::vector<GUInt64> anTileIdx(m_aoDims.size());
        const std::vector<size_t> anCount(m_aoDims.size(), 1);
        const std::vector<GInt64> anArrayStep(m_aoDims.size(), 0);
        const std::vector<GPtrDiff_t> anBufferStride(m_aoDims.size(), 0);
        const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);
        for( size_t i = 0; i < m_aoDims.size(); ++i )
        {
            anTileIdx[i] = static_cast<GUInt64>(tileIndices[i]);
        }
        GByte byValue = 0;
        if( poTilePresenceArray->Read(anTileIdx.data(),
                                       anCount.data(),
                                       anArrayStep.data(),
                                       anBufferStride.data(),
                                       eByteDT,
                                       &byValue) &&
            byValue == 0 )
        {
            if( bUseMutex )
                m_oMutex.unlock();
            CPLDebugOnly(ZARR_DEBUG_KEY, "Tile %s missing (=nodata)", osFilename.c_str());
            bMissingTileOut = true;
            return true;
        }
    }
    if( bUseMutex )
        m_oMutex.unlock();

    VSILFILE* fp = nullptr;
    // This is the number of files returned in a S3 directory listing operation
    constexpr uint64_t MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING = 1000;
    if( (m_osDimSeparator == "/" &&
            m_anBlockSize.back() > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING) ||
        (m_osDimSeparator != "/" &&
            m_nTotalTileCount > MAX_TILES_ALLOWED_FOR_DIRECTORY_LISTING) )
    {
        // Avoid issuing ReadDir() when a lot of files are expected
        CPLConfigOptionSetter optionSetter("GDAL_DISABLE_READDIR_ON_OPEN", "YES", true);
        fp = VSIFOpenL(osFilename.c_str(), "rb");
    }
    else
    {
        fp = VSIFOpenL(osFilename.c_str(), "rb");
    }
    if( fp == nullptr )
    {
        // Missing files are OK and indicate nodata_value
        CPLDebugOnly(ZARR_DEBUG_KEY, "Tile %s missing (=nodata)", osFilename.c_str());
        bMissingTileOut = true;
        return true;
    }

    bMissingTileOut = false;
    bool bRet = true;
    size_t nRawDataSize = abyRawTileData.size();
    if( psDecompressor == nullptr )
    {
        nRawDataSize = VSIFReadL(&abyRawTileData[0], 1, nRawDataSize, fp);
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
                void* out_buffer = &abyRawTileData[0];
                if( !psDecompressor->pfnFunc(abyCompressedData.data(),
                                             abyCompressedData.size(),
                                             &out_buffer, &nRawDataSize,
                                             nullptr,
                                             psDecompressor->user_data ))
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

    for( int i = m_oFiltersArray.Size(); bRet && i > 0; )
    {
        --i;
        const auto& oFilter = m_oFiltersArray[i];
        const auto osFilterId = oFilter["id"].ToString();
        const auto psFilterDecompressor = CPLGetDecompressor( osFilterId.c_str() );
        CPLAssert(psFilterDecompressor);

        CPLStringList aosOptions;
        for( const auto& obj: oFilter.GetChildren() )
        {
            aosOptions.SetNameValue(obj.GetName().c_str(),
                                    obj.ToString().c_str());
        }
        void* out_buffer = &abyTmpRawTileData[0];
        size_t nOutSize = abyTmpRawTileData.size();
        if( !psFilterDecompressor->pfnFunc(abyRawTileData.data(),
                                         nRawDataSize,
                                         &out_buffer,
                                         &nOutSize,
                                         aosOptions.List(),
                                         psFilterDecompressor->user_data ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Filter %s for tile %s failed",
                     osFilterId.c_str(), osFilename.c_str());
            return false;
        }

        nRawDataSize = nOutSize;
        std::swap(abyRawTileData, abyTmpRawTileData);
    }
    if( nRawDataSize != abyRawTileData.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Decompressed tile %s has not expected size after filters",
                 osFilename.c_str());
        return false;
    }

    if( bRet && !bMissingTileOut && m_bFortranOrder )
    {
        BlockTranspose(abyRawTileData, abyTmpRawTileData, true);
        std::swap(abyRawTileData, abyTmpRawTileData);
    }

    if( bRet && !bMissingTileOut && !abyDecodedTileData.empty() )
    {
        const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                                   m_aoDtypeElts.back().nativeSize;
        const auto nDTSize = m_oType.GetSize();
        const size_t nValues = abyDecodedTileData.size() / nDTSize;
        const GByte* pSrc = abyRawTileData.data();
        GByte* pDst = &abyDecodedTileData[0];
        for( size_t i = 0; i < nValues; i++, pSrc += nSourceSize, pDst += nDTSize )
        {
            DecodeSourceElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    return bRet;

#undef m_abyTmpRawTileData
#undef m_abyRawTileData
#undef m_abyDecodedTileData
#undef m_psDecompressor
}

/************************************************************************/
/*                      ZarrArray::IAdviseRead()                        */
/************************************************************************/

bool ZarrArray::IAdviseRead(const GUInt64* arrayStartIdx,
                            const size_t* count,
                            CSLConstList papszOptions) const
{
    const size_t nDims = m_aoDims.size();
    std::vector<uint64_t> anIndicesCur(nDims);
    std::vector<uint64_t> anIndicesMin(nDims);
    std::vector<uint64_t> anIndicesMax(nDims);

    // Compute min and max tile indices in each dimension, and the total
    // nomber of tiles this represents.
    uint64_t nReqTiles = 1;
    for( size_t i = 0; i < nDims; ++i )
    {
        anIndicesMin[i] = arrayStartIdx[i] / m_anBlockSize[i];
        anIndicesMax[i] = (arrayStartIdx[i] + count[i] - 1) / m_anBlockSize[i];
        // Overflow on number of tiles already checked in Create()
        nReqTiles *= (anIndicesMax[i] - anIndicesMin[i] + 1);
    }

    // Find available cache size
    const size_t nCacheSize = [papszOptions]()
    {
        size_t nCacheSizeTmp;
        const char* pszCacheSize = CSLFetchNameValue(papszOptions, "CACHE_SIZE");
        if( pszCacheSize )
        {
            const auto nCacheSizeBig = CPLAtoGIntBig(pszCacheSize);
            if( nCacheSizeBig < 0 ||
                static_cast<uint64_t>(nCacheSizeBig) >
                        std::numeric_limits<size_t>::max() / 2 )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too big CACHE_SIZE");
                return std::numeric_limits<size_t>::max();
            }
            nCacheSizeTmp = static_cast<size_t>(nCacheSizeBig);
        }
        else
        {
            // Arbitrarily take half of remaining cache size
            nCacheSizeTmp = static_cast<size_t>(
                std::min(static_cast<uint64_t>(
                            (GDALGetCacheMax64() - GDALGetCacheUsed64())/2),
                         static_cast<uint64_t>(
                             std::numeric_limits<size_t>::max() / 2)));
            CPLDebug(ZARR_DEBUG_KEY, "Using implicit CACHE_SIZE=" CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(nCacheSizeTmp));
        }
        return nCacheSizeTmp;
    }();
    if( nCacheSize == std::numeric_limits<size_t>::max() )
        return false;

    // Check that cache size is sufficient to hold all needed tiles.
    // Also check that anReqTilesIndices size computation won't overflow.
    if( nReqTiles > nCacheSize / std::max(m_nTileSize, nDims) )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "CACHE_SIZE=" CPL_FRMT_GUIB " is not big enough to cache "
                 "all needed tiles. "
                 "At least " CPL_FRMT_GUIB " bytes would be needed",
                 static_cast<GUIntBig>(nCacheSize),
                 static_cast<GUIntBig>(nReqTiles * std::max(m_nTileSize, nDims)));
        return false;
    }

    const int nThreads = [papszOptions, nReqTiles]()
    {
        int nThreadsTmp;
        const char* pszNumThreads = CSLFetchNameValueDef(
            papszOptions, "NUM_THREADS",
            CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS"));
        if( EQUAL(pszNumThreads, "ALL_CPUS") )
            nThreadsTmp = CPLGetNumCPUs();
        else
            nThreadsTmp = std::max(1, atoi(pszNumThreads));
        nThreadsTmp = static_cast<int>(
                        std::min(static_cast<uint64_t>(nThreadsTmp), nReqTiles));
        nThreadsTmp = std::min(nThreadsTmp, 10 * CPLGetNumCPUs());
        return nThreadsTmp;
    }();
    if( nThreads <= 1 )
        return true;
    CPLDebug(ZARR_DEBUG_KEY, "IAdviseRead(): Using %d threads", nThreads);

    m_oMapTileIndexToCachedTile.clear();

    std::vector<uint64_t> anReqTilesIndices;
    // Overflow checked above
    try
    {
        anReqTilesIndices.resize( static_cast<size_t>(nDims * nReqTiles) );
    }
    catch( const std::bad_alloc& e )
    {
         CPLError(CE_Failure, CPLE_OutOfMemory,
                  "Cannot allocate anReqTilesIndices: %s", e.what());
         return false;
    }

    size_t dimIdx = 0;
    size_t nTileIter = 0;
lbl_next_depth:
    if( dimIdx == nDims )
    {
        if( nDims == 2 )
        {
            // optimize in common case
            memcpy( &anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                    sizeof(uint64_t) * 2 );
        }
        else if( nDims == 3 )
        {
            // optimize in common case
            memcpy( &anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                    sizeof(uint64_t) * 3 );
        }
        else
        {
            memcpy( &anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                    sizeof(uint64_t) * nDims );
        }
        nTileIter ++;
    }
    else
    {
        // This level of loop loops over blocks
        anIndicesCur[dimIdx] = anIndicesMin[dimIdx];
        while(true)
        {
            dimIdx ++;
            goto lbl_next_depth;
lbl_return_to_caller:
            dimIdx --;
            if( anIndicesCur[dimIdx] == anIndicesMax[dimIdx] )
                break;
            ++ anIndicesCur[dimIdx];
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;
    assert( nTileIter == nReqTiles );

    CPLWorkerThreadPool* wtp = GDALGetGlobalThreadPool(nThreads);
    if( wtp == nullptr )
        return false;

    struct JobStruct
    {
        JobStruct() = default;

        JobStruct(const JobStruct&) = delete;
        JobStruct& operator= (const JobStruct&) = delete;

        JobStruct(JobStruct&&) = default;
        JobStruct& operator= (JobStruct&&) = default;

        const ZarrArray* poArray = nullptr;
        bool* pbGlobalStatus = nullptr;
        int* pnRemainingThreads = nullptr;
        const std::vector<uint64_t>* panReqTilesIndices = nullptr;
        size_t nFirstIdx = 0;
        size_t nLastIdxNotIncluded = 0;
    };
    std::vector<JobStruct> asJobStructs;

    bool bGlobalStatus = true;
    int nRemainingThreads = nThreads;
    // Check for very highly overflow in below loop
    assert( static_cast<size_t>(nThreads) <
                    std::numeric_limits<size_t>::max() / nReqTiles );

    // Setup jobs
    for( int i = 0; i < nThreads; i++ )
    {
        JobStruct jobStruct;
        jobStruct.poArray = this;
        jobStruct.pbGlobalStatus = &bGlobalStatus;
        jobStruct.pnRemainingThreads = &nRemainingThreads;
        jobStruct.panReqTilesIndices = &anReqTilesIndices;
        jobStruct.nFirstIdx = static_cast<size_t>(i * nReqTiles / nThreads);
        jobStruct.nLastIdxNotIncluded = std::min(
            static_cast<size_t>((i + 1) * nReqTiles / nThreads), nTileIter);
        asJobStructs.emplace_back(std::move(jobStruct));
    }

    const auto JobFunc = [](void* pThreadData)
    {
        const JobStruct* jobStruct = static_cast<const JobStruct*>(pThreadData);

        const auto poArray = jobStruct->poArray;
        const auto& aoDims = poArray->m_aoDims;
        const size_t l_nDims = poArray->GetDimensionCount();
        std::vector<GByte> abyRawTileData;
        std::vector<GByte> abyDecodedTileData;
        std::vector<GByte> abyTmpRawTileData;
        const CPLCompressor* psDecompressor =
            CPLGetDecompressor(poArray->m_osDecompressorId.c_str());

        for( size_t iReq = jobStruct->nFirstIdx; iReq < jobStruct->nLastIdxNotIncluded; ++iReq )
        {
            // Check if we must early exit
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                if( !(*jobStruct->pbGlobalStatus) )
                    return;
            }

            const uint64_t* tileIndices =
                jobStruct->panReqTilesIndices->data() + iReq * l_nDims;

            uint64_t nTileIdx = 0;
            for( size_t j = 0; j < l_nDims; ++j )
            {
                if( j > 0 )
                    nTileIdx *= aoDims[j-1]->GetSize();
                nTileIdx += tileIndices[j];
            }

            if( !poArray->AllocateWorkingBuffers(abyRawTileData,
                                                 abyTmpRawTileData,
                                                 abyDecodedTileData) )
            {
                std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            bool bIsEmpty = false;
            bool success = poArray->LoadTileData(tileIndices,
                                                 true, // use mutex
                                                 psDecompressor,
                                                 abyRawTileData,
                                                 abyTmpRawTileData,
                                                 abyDecodedTileData,
                                                 bIsEmpty);

            std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
            if( !success )
            {
                *jobStruct->pbGlobalStatus = false;
                break;
            }

            CachedTile cachedTile;
            if( !bIsEmpty )
            {
                if( !abyDecodedTileData.empty() )
                    std::swap(cachedTile.abyDecoded, abyDecodedTileData);
                else
                    std::swap(cachedTile.abyDecoded, abyRawTileData);
            }
            poArray->m_oMapTileIndexToCachedTile[nTileIdx] = std::move(cachedTile);
        }

        std::lock_guard<std::mutex> oLock(poArray->m_oMutex);
        (*jobStruct->pnRemainingThreads) --;
    };

    // Start jobs
    for( int i = 0; i < nThreads; i++ )
    {
        if( !wtp->SubmitJob(JobFunc, &asJobStructs[i]) )
        {
            std::lock_guard<std::mutex> oLock(m_oMutex);
            bGlobalStatus = false;
            nRemainingThreads = i;
            break;
        }
    }

    // Wait for all jobs to be finished
    while( true )
    {
        {
            std::lock_guard<std::mutex> oLock(m_oMutex);
            if( nRemainingThreads == 0 )
                break;
        }
        wtp->WaitEvent();
    }

    return bGlobalStatus;
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
    if( !AllocateWorkingBuffers() )
        return false;

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

    std::vector<uint64_t> indicesOuterLoop(nDims + 1);
    std::vector<GByte*> dstPtrStackOuterLoop(nDims + 1);

    std::vector<uint64_t> indicesInnerLoop(nDims + 1);
    std::vector<GByte*> dstPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> dstBufferStrideBytes;
    for( size_t i = 0; i < nDims; ++i )
    {
        dstBufferStrideBytes.push_back(
            bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize));
    }
    dstBufferStrideBytes.push_back(0);

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims + 1, 1);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT =
        m_oType.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    const bool bSameCompoundAndNoDynamicMem =
        m_oType.GetClass() == GEDTC_COMPOUND &&
        m_oType == bufferDataType &&
        !m_oType.NeedsFreeDynamicMemory();
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

        const GByte* pabySrcTile = m_abyDecodedTileData.empty() ?
                            m_abyRawTileData.data(): m_abyDecodedTileData.data();
        bool bMatchFoundInMapTileIndexToCachedTile = false;

        // Use cache built by IAdviseRead() if possible
        if( !m_oMapTileIndexToCachedTile.empty() )
        {
            uint64_t nTileIdx = 0;
            for( size_t j = 0; j < nDims; ++j )
            {
                if( j > 0 )
                    nTileIdx *= m_aoDims[j-1]->GetSize();
                nTileIdx += tileIndices[j];
            }
            const auto oIter = m_oMapTileIndexToCachedTile.find(nTileIdx);
            if( oIter != m_oMapTileIndexToCachedTile.end() )
            {
                bMatchFoundInMapTileIndexToCachedTile = true;
                if( oIter->second.abyDecoded.empty() )
                {
                    bEmptyTile = true;
                }
                else
                {
                    pabySrcTile = oIter->second.abyDecoded.data();
                }
            }
            else
            {
                CPLDebugOnly(ZARR_DEBUG_KEY, "Cache miss for tile " CPL_FRMT_GUIB,
                             static_cast<GUIntBig>(nTileIdx));
            }
        }

        if( !bMatchFoundInMapTileIndexToCachedTile )
        {
            if( !tileIndices.empty() && tileIndices == m_anCachedTiledIndices )
            {
                if( !m_bCachedTiledValid )
                    return false;
                bEmptyTile = m_bCachedTiledEmpty;
            }
            else
            {
                if( !FlushDirtyTile() )
                    return false;

                m_anCachedTiledIndices = tileIndices;
                m_bCachedTiledValid = LoadTileData(tileIndices.data(), bEmptyTile);
                if( !m_bCachedTiledValid )
                {
                    return false;
                }
                m_bCachedTiledEmpty = bEmptyTile;
            }

            pabySrcTile = m_abyDecodedTileData.empty() ?
                            m_abyRawTileData.data(): m_abyDecodedTileData.data();
        }
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
        if( nDims == 0 || dimIdxSubLoop == nDims - 1 )
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
            const auto step = nDims == 0 ? 0 : arrayStep[dimIdxSubLoop];

            if( m_bUseOptimizedCodePaths && bBothAreNumericDT &&
                step <= static_cast<GIntBig>(std::numeric_limits<int>::max() / nDTSize) &&
                dstBufferStrideBytes[dimIdxSubLoop] <= std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( src_ptr,
                                 m_oType.GetNumericDataType(),
                                 static_cast<int>(step * nDTSize),
                                 dst_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );

                goto end_inner_loop;
            }

            for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                    ++i,
                    src_ptr += step * nSrcDTSize,
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
                else if( bSameCompoundAndNoDynamicMem )
                {
                    memcpy(dst_ptr, src_ptr, nDTSize);
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
/*                    ZarrArray::FlushDirtyTile()                       */
/************************************************************************/

bool ZarrArray::FlushDirtyTile() const
{
    if( !m_bDirtyTile )
        return true;
    m_bDirtyTile = false;

    std::string osFilename;
    if( m_anCachedTiledIndices.empty() )
    {
        osFilename = "0";
    }
    else
    {
        for( const auto index: m_anCachedTiledIndices )
        {
            if( !osFilename.empty() )
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(index);
        }
    }

    if( m_nVersion == 2 )
    {
        osFilename = CPLFormFilename(
            CPLGetDirname(m_osFilename.c_str()), osFilename.c_str(), nullptr);
    }
    else
    {
        std::string osTmp = m_osRootDirectoryName + "/data/root";
        if( GetFullName() != "/" )
            osTmp += GetFullName();
        osFilename = osTmp + "/c" + osFilename;
    }

    const size_t nSourceSize = m_aoDtypeElts.back().nativeOffset +
                               m_aoDtypeElts.back().nativeSize;
    auto& abyTile = m_abyDecodedTileData.empty() ?
                            m_abyRawTileData: m_abyDecodedTileData;

    bool bEmptyTile = false;
    if( m_pabyNoData == nullptr ||
        (m_oType.GetClass() == GEDTC_NUMERIC && GetNoDataValueAsDouble() == 0.0) )
    {
        const size_t nBytes = abyTile.size();
        size_t i = 0;
        bEmptyTile = true;
        for(; i + (sizeof(size_t)-1) < nBytes; i += sizeof(size_t) )
        {
            if( *reinterpret_cast<const size_t*>(abyTile.data() + i) != 0 )
            {
                bEmptyTile = false;
                break;
            }
        }
        if( bEmptyTile )
        {
            for(; i < nBytes; ++i )
            {
                if( abyTile[i] != 0 )
                {
                    bEmptyTile = false;
                    break;
                }
            }
        }
    }
    else if( m_oType.GetClass() == GEDTC_NUMERIC &&
             !GDALDataTypeIsComplex(m_oType.GetNumericDataType()) )
    {
        const int nDTSize = static_cast<int>(m_oType.GetSize());
        const size_t nElts = abyTile.size() / nDTSize;
        const auto eDT = m_oType.GetNumericDataType();
        bEmptyTile = GDALBufferHasOnlyNoData(
            abyTile.data(),
            GetNoDataValueAsDouble(),
            nElts, // nWidth
            1, // nHeight
            nElts, // nLineStride
            1, // nComponents
            nDTSize * 8, // nBitsPerSample
            GDALDataTypeIsInteger(eDT) ?
                (GDALDataTypeIsSigned(eDT) ?
                    GSF_SIGNED_INT:
                    GSF_UNSIGNED_INT) :
                GSF_FLOATING_POINT);
    }

    if( bEmptyTile )
    {
        m_bCachedTiledEmpty = true;

        VSIStatBufL sStat;
        if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
        {
            CPLDebugOnly(ZARR_DEBUG_KEY, "Deleting tile %s that has now empty content",
                         osFilename.c_str());
            return VSIUnlink(osFilename.c_str()) == 0;
        }
        return true;
    }

    if( !m_abyDecodedTileData.empty() )
    {
        const size_t nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        GByte* pDst = &m_abyRawTileData[0];
        const GByte* pSrc = m_abyDecodedTileData.data();
        for( size_t i = 0; i < nValues; i++, pDst += nSourceSize, pSrc += nDTSize )
        {
            EncodeElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    if( m_bFortranOrder )
    {
        BlockTranspose(m_abyRawTileData, m_abyTmpRawTileData, false);
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    size_t nRawDataSize = m_abyRawTileData.size();
    for( const auto& oFilter: m_oFiltersArray )
    {
        const auto osFilterId = oFilter["id"].ToString();
        const auto psFilterCompressor = CPLGetCompressor( osFilterId.c_str() );
        CPLAssert(psFilterCompressor);

        CPLStringList aosOptions;
        for( const auto& obj: oFilter.GetChildren() )
        {
            aosOptions.SetNameValue(obj.GetName().c_str(),
                                    obj.ToString().c_str());
        }
        void* out_buffer = &m_abyTmpRawTileData[0];
        size_t nOutSize = m_abyTmpRawTileData.size();
        if( !psFilterCompressor->pfnFunc(m_abyRawTileData.data(),
                                         nRawDataSize,
                                         &out_buffer,
                                         &nOutSize,
                                         aosOptions.List(),
                                         psFilterCompressor->user_data ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Filter %s for tile %s failed",
                     osFilterId.c_str(), osFilename.c_str());
            return false;
        }

        nRawDataSize = nOutSize;
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    if( m_osDimSeparator == "/" )
    {
        std::string osDir = CPLGetDirname(osFilename.c_str());
        VSIStatBufL sStat;
        if( VSIStatL(osDir.c_str(), &sStat) != 0 )
        {
            if( VSIMkdirRecursive(osDir.c_str(), 0755) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osDir.c_str());
                return false;
            }
        }
    }

    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "wb");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create tile %s", osFilename.c_str());
        return false;
    }

    bool bRet = true;
    if( m_psCompressor == nullptr )
    {
        if( VSIFWriteL(m_abyRawTileData.data(), 1, nRawDataSize, fp) != nRawDataSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly",
                     osFilename.c_str());
            bRet = false;
        }
    }
    else
    {
        std::vector<GByte> abyCompressedData;
        try
        {
            constexpr size_t MIN_BUF_SIZE = 64; // somewhat arbitrary
            abyCompressedData.resize(static_cast<size_t>(
                MIN_BUF_SIZE +
                nRawDataSize +
                nRawDataSize / 3));

        }
        catch( const std::exception& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for tile %s",
                     osFilename.c_str());
            bRet = false;
        }

        if( bRet )
        {
            void* out_buffer = &abyCompressedData[0];
            size_t out_size = abyCompressedData.size();
            CPLStringList aosOptions;
            const auto compressorConfig = m_nVersion == 2 ?
                m_oCompressorJSonV2 : m_oCompressorJSonV3["configuration"];
            for( const auto& obj: compressorConfig.GetChildren() )
            {
                aosOptions.SetNameValue(obj.GetName().c_str(),
                                        obj.ToString().c_str());
            }
            if( EQUAL(m_psCompressor->pszId, "blosc") &&
                m_oType.GetClass() == GEDTC_NUMERIC )
            {
                aosOptions.SetNameValue("TYPESIZE",
                    CPLSPrintf("%d",
                       GDALGetDataTypeSizeBytes(
                           GDALGetNonComplexDataType(
                               m_oType.GetNumericDataType()))));
            }

            if( !m_psCompressor->pfnFunc(m_abyRawTileData.data(),
                                         nRawDataSize,
                                         &out_buffer, &out_size,
                                         aosOptions.List(),
                                         m_psCompressor->user_data ) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Compression of tile %s failed",
                         osFilename.c_str());
                bRet = false;
            }
            abyCompressedData.resize(out_size);
        }

        if( bRet &&
            VSIFWriteL(abyCompressedData.data(), 1, abyCompressedData.size(),
                       fp) != abyCompressedData.size() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly",
                     osFilename.c_str());
            bRet = false;
        }
    }
    VSIFCloseL(fp);

    return bRet;
}

/************************************************************************/
/*                           ZarrArray::IRead()                         */
/************************************************************************/

bool ZarrArray::IWrite(const GUInt64* arrayStartIdx,
                       const size_t* count,
                       const GInt64* arrayStep,
                       const GPtrDiff_t* bufferStride,
                       const GDALExtendedDataType& bufferDataType,
                       const void* pSrcBuffer)
{
    if( !AllocateWorkingBuffers() )
        return false;

    m_oMapTileIndexToCachedTile.clear();

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
                pSrcBuffer = static_cast<const GByte*>(pSrcBuffer) +
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

    std::vector<uint64_t> indicesOuterLoop(nDims + 1);
    std::vector<const GByte*> srcPtrStackOuterLoop(nDims + 1);

    std::vector<uint64_t> indicesInnerLoop(nDims + 1);
    std::vector<const GByte*> srcPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> srcBufferStrideBytes;
    for( size_t i = 0; i < nDims; ++i )
    {
        srcBufferStrideBytes.push_back(
            bufferStride[i] * static_cast<GPtrDiff_t>(nBufferDTSize));
    }
    srcBufferStrideBytes.push_back(0);

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nNativeSize = m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims + 1, 1);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT =
        m_oType.GetClass() == GEDTC_NUMERIC &&
        bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    const bool bSameCompoundAndNoDynamicMem =
        m_oType.GetClass() == GEDTC_COMPOUND &&
        m_oType == bufferDataType &&
        !m_oType.NeedsFreeDynamicMemory();

    size_t dimIdx = 0;
    srcPtrStackOuterLoop[0] = static_cast<const GByte*>(pSrcBuffer);
lbl_next_depth:
    if( dimIdx == nDims )
    {
        bool bWriteWholeTile = true;
        bool bPartialTile = false;
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
            if( bWriteWholeTile )
            {
                const bool bWholePartialTileThisDim =
                    indicesOuterLoop[i] + countInnerLoopInit[i] == m_aoDims[i]->GetSize();
                bWriteWholeTile = (countInnerLoopInit[i] == m_anBlockSize[i] ||
                                   bWholePartialTileThisDim);
                if( bWholePartialTileThisDim )
                {
                    bPartialTile = true;
                }
            }
        }

        size_t dimIdxSubLoop = 0;
        srcPtrStackInnerLoop[0] = srcPtrStackOuterLoop[nDims];
        const size_t nCacheDTSize = m_abyDecodedTileData.empty() ? nNativeSize : nDTSize;
        auto& abyTile = m_abyDecodedTileData.empty() ?
                            m_abyRawTileData: m_abyDecodedTileData;

        if( !tileIndices.empty() && tileIndices == m_anCachedTiledIndices )
        {
            if( !m_bCachedTiledValid )
                return false;
        }
        else
        {
            if( !FlushDirtyTile() )
                return false;

            m_anCachedTiledIndices = tileIndices;
            m_bCachedTiledValid = true;

            if( bWriteWholeTile )
            {
                if( bPartialTile )
                {
                    DeallocateDecodedTileData();
                    memset(&abyTile[0], 0, abyTile.size());
                }
            }
            else
            {
                // If we don't write the whole tile, we need to fetch a
                // potentially existing one.
                bool bEmptyTile = false;
                m_bCachedTiledValid = LoadTileData(tileIndices.data(), bEmptyTile);
                if( !m_bCachedTiledValid )
                {
                    return false;
                }

                if( bEmptyTile )
                {
                    DeallocateDecodedTileData();

                    if( m_pabyNoData == nullptr )
                    {
                        memset(&abyTile[0], 0, abyTile.size());
                    }
                    else
                    {
                        const size_t nElts = abyTile.size() / nCacheDTSize;
                        GByte* dstPtr = &abyTile[0];
                        if( m_oType.GetClass() == GEDTC_NUMERIC )
                        {
                            GDALCopyWords64( m_pabyNoData,
                                             m_oType.GetNumericDataType(),
                                             0,
                                             dstPtr,
                                             m_oType.GetNumericDataType(),
                                             static_cast<int>(m_oType.GetSize()),
                                             static_cast<GPtrDiff_t>(nElts) );
                        }
                        else
                        {
                            for(size_t i = 0; i < nElts; ++i )
                            {
                                GDALExtendedDataType::CopyValue(m_pabyNoData, m_oType,
                                                                dstPtr, m_oType);
                                dstPtr += nCacheDTSize;
                            }
                        }
                    }
                }
            }
        }
        m_bDirtyTile = true;
        m_bCachedTiledEmpty = false;

        GByte* pabyTile = &abyTile[0];

lbl_next_depth_inner_loop:
        if( nDims == 0 || dimIdxSubLoop == nDims - 1 )
        {
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            const void* src_ptr = srcPtrStackInnerLoop[dimIdxSubLoop];

            size_t nOffset = 0;
            for( size_t i = 0; i < nDims; i++ )
            {
                nOffset = static_cast<size_t>(nOffset * m_anBlockSize[i] +
                    (indicesInnerLoop[i] - tileIndices[i] * m_anBlockSize[i]));
            }
            GByte* dst_ptr = pabyTile + nOffset * nCacheDTSize;
            const auto step = nDims == 0 ? 0 : arrayStep[dimIdxSubLoop];

            if( m_bUseOptimizedCodePaths && bBothAreNumericDT &&
                step <= static_cast<GIntBig>(std::numeric_limits<int>::max() / nDTSize) &&
                srcBufferStrideBytes[dimIdxSubLoop] <= std::numeric_limits<int>::max() )
            {
                GDALCopyWords64( src_ptr,
                                 bufferDataType.GetNumericDataType(),
                                 static_cast<int>(srcBufferStrideBytes[dimIdxSubLoop]),
                                 dst_ptr,
                                 m_oType.GetNumericDataType(),
                                 static_cast<int>(step * nDTSize),
                                 static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]) );

                goto end_inner_loop;
            }

            for( size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                    ++i,
                    dst_ptr += step * nCacheDTSize,
                    src_ptr = static_cast<const uint8_t*>(src_ptr) + srcBufferStrideBytes[dimIdxSubLoop] )
            {
                if( bSameNumericDT )
                {
                    void* dst_ptr_v = dst_ptr;
                    if( nSameDTSize == 1 )
                        *static_cast<uint8_t*>(dst_ptr_v) = *static_cast<const uint8_t*>(src_ptr);
                    else if( nSameDTSize == 2 )
                    {
                        *static_cast<uint16_t*>(dst_ptr_v) = *static_cast<const uint16_t*>(src_ptr);
                    }
                    else if( nSameDTSize == 4 )
                    {
                        *static_cast<uint32_t*>(dst_ptr_v) = *static_cast<const uint32_t*>(src_ptr);
                    }
                    else if( nSameDTSize == 8 )
                    {
                        *static_cast<uint64_t*>(dst_ptr_v) = *static_cast<const uint64_t*>(src_ptr);
                    }
                    else if( nSameDTSize == 16 )
                    {
                        static_cast<uint64_t*>(dst_ptr_v)[0] = static_cast<const uint64_t*>(src_ptr)[0];
                        static_cast<uint64_t*>(dst_ptr_v)[1] = static_cast<const uint64_t*>(src_ptr)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if( bSameCompoundAndNoDynamicMem )
                {
                    memcpy(dst_ptr, src_ptr, nDTSize);
                }
                else if( m_oType.GetClass() == GEDTC_STRING )
                {
                    const char* pSrcStr = *static_cast<const char* const*>(src_ptr);
                    if( pSrcStr )
                    {
                        const size_t nLen = strlen(pSrcStr);
                        memcpy(dst_ptr, pSrcStr, std::min(nLen, nNativeSize));
                        if( nLen < nNativeSize )
                            memset(dst_ptr + nLen, 0, nNativeSize - nLen);
                    }
                    else
                    {
                        memset(dst_ptr, 0, nNativeSize);
                    }
                }
                else
                {
                    if( m_oType.NeedsFreeDynamicMemory() )
                        m_oType.FreeDynamicMemory(dst_ptr);
                    GDALExtendedDataType::CopyValue(src_ptr, bufferDataType,
                                                    dst_ptr, m_oType);
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
                srcPtrStackInnerLoop[dimIdxSubLoop] = srcPtrStackInnerLoop[dimIdxSubLoop-1];
                goto lbl_next_depth_inner_loop;
lbl_return_to_caller_inner_loop:
                dimIdxSubLoop --;
                -- countInnerLoop[dimIdxSubLoop];
                if( countInnerLoop[dimIdxSubLoop] == 0 )
                {
                    break;
                }
                indicesInnerLoop[dimIdxSubLoop] += arrayStep[dimIdxSubLoop];
                srcPtrStackInnerLoop[dimIdxSubLoop] += srcBufferStrideBytes[dimIdxSubLoop];
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
            srcPtrStackOuterLoop[dimIdx] = srcPtrStackOuterLoop[dimIdx - 1];
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
            srcPtrStackOuterLoop[dimIdx] += bufferStride[dimIdx] * static_cast<GPtrDiff_t>(nIncr * nBufferDTSize);
            tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        }
    }
    if( dimIdx > 0 )
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                             ParseDtype()                             */
/************************************************************************/

static size_t GetAlignment(const CPLJSONObject& obj)
{
    if( obj.GetType() == CPLJSONObject::Type::String )
    {
        const auto str = obj.ToString();
        if( str.size() < 3 )
            return 1;
        const char chType = str[1];
        const int nBytes = atoi(str.c_str() + 2);
        if( chType == 'S' )
            return sizeof(char*);
        if( chType == 'c' && nBytes == 8 )
            return sizeof(float);
        if( chType == 'c' && nBytes == 16 )
            return sizeof(double);
       return nBytes;
    }
    else if( obj.GetType() == CPLJSONObject::Type::Array )
    {
        const auto oArray = obj.ToArray();
        size_t nAlignment = 1;
        for( const auto& oElt: oArray )
        {
            const auto oEltArray = oElt.ToArray();
            if( !oEltArray.IsValid() || oEltArray.Size() != 2 ||
                oEltArray[0].GetType() != CPLJSONObject::Type::String )
            {
                return 1;
            }
            nAlignment = std::max(nAlignment, GetAlignment(oEltArray[1]));
            if( nAlignment == sizeof(void*) )
                break;
        }
        return nAlignment;
    }
    return 1;
}

static GDALExtendedDataType ParseDtype(bool isZarrV2,
                                       const CPLJSONObject& obj,
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
            char chEndianness = 0;
            char chType;
            int nBytes;
            DtypeElt elt;
            if( isZarrV2 )
            {
                if( str.size() < 3 )
                    break;
                chEndianness = str[0];
                chType = str[1];
                nBytes = atoi(str.c_str() + 2);
            }
            else
            {
                if( str.size() < 2 )
                    break;
                if( str == "bool" )
                {
                    chType = 'b';
                    nBytes = 1;
                }
                else if( str == "u1" || str == "i1" )
                {
                    chType = str[0];
                    nBytes = 1;
                }
                else
                {
                    if( str.size() < 3 )
                        break;
                    chEndianness = str[0];
                    chType = str[1];
                    nBytes = atoi(str.c_str() + 2);
                }
            }
            if( nBytes <= 0 || nBytes >= 1000 )
                break;

            if( chEndianness == '<' )
                elt.needByteSwapping = (CPL_IS_LSB == 0);
            else if( chEndianness == '>' )
                elt.needByteSwapping = (CPL_IS_LSB != 0);

            GDALDataType eDT;
            if( !elts.empty() )
            {
                elt.nativeOffset = elts.back().nativeOffset + elts.back().nativeSize;
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
            else if( chType == 'f' && nBytes == 2 )
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float32;
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
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
        else if( isZarrV2 && obj.GetType() == CPLJSONObject::Type::Array )
        {
            bool error = false;
            const auto oArray = obj.ToArray();
            std::vector<std::unique_ptr<GDALEDTComponent>> comps;
            size_t offset = 0;
            size_t alignmentMax = 1;
            for( const auto& oElt: oArray )
            {
                const auto oEltArray = oElt.ToArray();
                if( !oEltArray.IsValid() || oEltArray.Size() != 2 ||
                    oEltArray[0].GetType() != CPLJSONObject::Type::String )
                {
                    error = true;
                    break;
                }
                GDALExtendedDataType subDT = ParseDtype(isZarrV2, oEltArray[1], elts);
                if( subDT.GetClass() == GEDTC_NUMERIC &&
                    subDT.GetNumericDataType() == GDT_Unknown )
                {
                    error = true;
                    break;
                }

                const std::string osName = oEltArray[0].ToString();
                // Add padding for alignment
                const size_t alignmentSub = GetAlignment(oEltArray[1]);
                assert(alignmentSub);
                alignmentMax = std::max(alignmentMax, alignmentSub);
                offset = AlignOffsetOn(offset, alignmentSub);
                comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
                    new GDALEDTComponent(osName, offset, subDT)));
                offset += subDT.GetSize();
            }
            if( error )
                break;
            size_t nTotalSize = offset;
            nTotalSize = AlignOffsetOn(nTotalSize, alignmentMax);
            return GDALExtendedDataType::Create(obj.ToString(),
                                                nTotalSize,
                                                std::move(comps));
        }
    }
    while(false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for dtype: %s",
             obj.ToString().c_str());
    return GDALExtendedDataType::Create(GDT_Unknown);
}

static void SetGDALOffset(const GDALExtendedDataType& dt,
                          const size_t nBaseOffset,
                          std::vector<DtypeElt>& elts,
                          size_t& iCurElt)
{
    if( dt.GetClass() == GEDTC_COMPOUND )
    {
        const auto& comps = dt.GetComponents();
        for( const auto& comp: comps )
        {
            const size_t nBaseOffsetSub = nBaseOffset + comp->GetOffset();
            SetGDALOffset(comp->GetType(), nBaseOffsetSub, elts, iCurElt);
        }
    }
    else
    {
        elts[iCurElt].gdalOffset = nBaseOffset;
        iCurElt++;
    }
}

/************************************************************************/
/*                     ZarrGroupBase::LoadArray()                       */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrGroupBase::LoadArray(const std::string& osArrayName,
                                                const std::string& osZarrayFilename,
                                                const CPLJSONObject& oRoot,
                                                bool bLoadedFromZMetadata,
                                                const CPLJSONObject& oAttributesIn,
                                                std::set<std::string>& oSetFilenamesInLoading) const
{
    // Prevent too deep or recursive array loading
    if( oSetFilenamesInLoading.find(osZarrayFilename) != oSetFilenamesInLoading.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt at recursively loading %s", osZarrayFilename.c_str());
        return nullptr;
    }
    if( oSetFilenamesInLoading.size() == 32 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too deep call stack in LoadArray()");
        return nullptr;
    }

    struct SetFilenameAdder
    {
        std::set<std::string>& m_oSetFilenames;
        std::string m_osFilename;

        SetFilenameAdder(std::set<std::string>& oSetFilenamesIn,
                         const std::string& osFilename):
             m_oSetFilenames(oSetFilenamesIn),
             m_osFilename(osFilename)
        {
            m_oSetFilenames.insert(osFilename);
        }

        ~SetFilenameAdder()
        {
            m_oSetFilenames.erase(m_osFilename);
        }
    };

    // Add osZarrayFilename to oSetFilenamesInLoading during the scope
    // of this function call.
    SetFilenameAdder filenameAdder(oSetFilenamesInLoading, osZarrayFilename);

    const bool isZarrV2 = dynamic_cast<const ZarrGroupV2*>(this) != nullptr;

    if( isZarrV2 )
    {
        const auto osFormat = oRoot["zarr_format"].ToString();
        if( osFormat != "2" )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for zarr_format");
            return nullptr;
        }
    }

    bool bFortranOrder = false;
    const char* orderKey = isZarrV2 ? "order": "chunk_memory_layout";
    const auto osOrder = oRoot[orderKey].ToString();
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
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for %s", orderKey);
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if( !oShape.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    const char* chunksKey = isZarrV2 ? "chunks": "chunk_grid/chunk_shape";
    const auto oChunks = oRoot[chunksKey].ToArray();
    if( !oChunks.IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing or not an array",
                 chunksKey);
        return nullptr;
    }

    if( oShape.Size() != oChunks.Size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }

    CPLJSONObject oAttributes(oAttributesIn);
    if( !bLoadedFromZMetadata && isZarrV2 )
    {
        CPLJSONDocument oDoc;
        const std::string osZattrsFilename(
            CPLFormFilename(CPLGetDirname(osZarrayFilename.c_str()), ".zattrs", nullptr));
        CPLErrorHandlerPusher quietError(CPLQuietErrorHandler);
        CPLErrorStateBackuper errorStateBackuper;
        if( oDoc.Load(osZattrsFilename) )
        {
            oAttributes = oDoc.GetRoot();
        }
    }
    else if( !isZarrV2 )
    {
        oAttributes = oRoot["attributes"];
    }

    // Deep-clone of oAttributes
    {
        CPLJSONDocument oTmpDoc;
        oTmpDoc.SetRoot(oAttributes);
        CPL_IGNORE_RET_VAL(oTmpDoc.LoadMemory(oTmpDoc.SaveAsString()));
        oAttributes = oTmpDoc.GetRoot();
    }

    const auto crs = oAttributes["crs"];
    std::shared_ptr<OGRSpatialReference> poSRS;
    if( crs.GetType() == CPLJSONObject::Type::Object )
    {
        for( const char* key: { "url", "wkt", "projjson" } )
        {
            const auto item = crs[key];
            if( item.IsValid() )
            {
                poSRS = std::make_shared<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if( poSRS->SetFromUserInput(item.ToString().c_str(), OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) == OGRERR_NONE )
                {
                    oAttributes.Delete("crs");
                    break;
                }
                poSRS.reset();
            }
        }
    }

    const auto unit = oAttributes[CF_UNITS];
    std::string osUnit;
    if( unit.GetType() == CPLJSONObject::Type::String )
    {
        osUnit = unit.ToString();
        oAttributes.Delete(CF_UNITS);
    }

    bool bHasOffset = false;
    double dfOffset = 0.0;
    const auto offset = oAttributes[CF_ADD_OFFSET];
    const auto offsetType = offset.GetType();
    if( offsetType == CPLJSONObject::Type::Integer ||
        offsetType == CPLJSONObject::Type::Long ||
        offsetType == CPLJSONObject::Type::Double )
    {
        dfOffset = offset.ToDouble();
        bHasOffset = true;
        oAttributes.Delete(CF_ADD_OFFSET);
    }

    bool bHasScale = false;
    double dfScale = 1.0;
    const auto scale = oAttributes[CF_SCALE_FACTOR];
    const auto scaleType = scale.GetType();
    if( scaleType == CPLJSONObject::Type::Integer ||
        scaleType == CPLJSONObject::Type::Long ||
        scaleType == CPLJSONObject::Type::Double )
    {
        dfScale = scale.ToDouble();
        bHasScale = true;
        oAttributes.Delete(CF_SCALE_FACTOR);
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

    const auto GetDimensionTypeDirection = [&oAttributes, &osUnit](
                                            std::string& osType,
                                            std::string& osDirection)
    {
        const auto oStdName = oAttributes[CF_STD_NAME];
        if( oStdName.GetType() == CPLJSONObject::Type::String )
        {
            const auto osStdName = oStdName.ToString();
            if( osStdName == CF_PROJ_X_COORD ||
                osStdName == CF_LONGITUDE_STD_NAME )
            {
                osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                oAttributes.Delete(CF_STD_NAME);
                if( osUnit == CF_DEGREES_EAST )
                {
                    osDirection = "EAST";
                }
            }
            else if( osStdName == CF_PROJ_Y_COORD ||
                osStdName == CF_LATITUDE_STD_NAME )
            {
                osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                oAttributes.Delete(CF_STD_NAME);
                if( osUnit == CF_DEGREES_NORTH )
                {
                    osDirection = "NORTH";
                }
            }
            else if( osStdName == "time" )
            {
                osType = GDAL_DIM_TYPE_TEMPORAL;
                oAttributes.Delete(CF_STD_NAME);
            }
        }

        const auto osAxis = oAttributes[CF_AXIS].ToString();
        if( osAxis == "Z" )
        {
            osType = GDAL_DIM_TYPE_VERTICAL;
            const auto osPositive = oAttributes["positive"].ToString();
            if( osPositive == "up" )
            {
                osDirection = "UP";
                oAttributes.Delete("positive");
            }
            else if( osPositive == "down" )
            {
                osDirection = "DOWN";
                oAttributes.Delete("positive");
            }
            oAttributes.Delete(CF_AXIS);
        }
    };

    // XArray extension
    const auto arrayDimensionsObj = oAttributes["_ARRAY_DIMENSIONS"];

    const auto FindDimension = [this, &aoDims, &GetDimensionTypeDirection,
                                bLoadedFromZMetadata, &osArrayName,
                                &osZarrayFilename, &oSetFilenamesInLoading,
                                isZarrV2](
                                        const std::string& osDimName,
                                        std::shared_ptr<GDALDimension>& poDim,
                                        int i)
    {
        auto oIter = m_oMapDimensions.find(osDimName);
        if( oIter != m_oMapDimensions.end() )
        {
            if( oIter->second->GetSize() == poDim->GetSize() )
            {
                poDim = oIter->second;
                return true;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                     "Size of _ARRAY_DIMENSIONS[%d] different "
                     "from the one of shape", i);
                return false;
            }
        }

        // Try to load the indexing variable.

        // If loading from zmetadata, we should have normally
        // already loaded the dimension variables, unless they
        // are in a upper level.
        if( bLoadedFromZMetadata && osArrayName != osDimName &&
            m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end() )
        {
            auto poParent = m_poParent.lock();
            while( poParent != nullptr )
            {
                oIter = poParent->m_oMapDimensions.find(osDimName);
                if( oIter != poParent->m_oMapDimensions.end() &&
                    oIter->second->GetSize() == poDim->GetSize() )
                {
                    poDim = oIter->second;
                    return true;
                }
                poParent = poParent->m_poParent.lock();
            }
        }

        // Not loading from zmetadata, and not in m_oMapMDArrays,
        // then stat() the indexing variable.
        else if( !bLoadedFromZMetadata &&
                 osArrayName != osDimName &&
                 m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end() )
        {
            std::string osDirName = m_osDirectoryName;
            while( true )
            {
                const std::string osArrayFilenameDim =
                    isZarrV2 ?
                        CPLFormFilename(
                            CPLFormFilename(osDirName.c_str(),
                                            osDimName.c_str(),
                                            nullptr),
                            ".zarray", nullptr) :
                        CPLFormFilename(
                            CPLGetDirname(osZarrayFilename.c_str()),
                            (osDimName + ".array.json").c_str(),
                            nullptr);
                VSIStatBufL sStat;
                if( VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0 )
                {
                    CPLJSONDocument oDoc;
                    if( oDoc.Load(osArrayFilenameDim) )
                    {
                        LoadArray(
                            osDimName,
                            osArrayFilenameDim,
                            oDoc.GetRoot(),
                            false,
                            CPLJSONObject(),
                            oSetFilenamesInLoading);
                    }
                }
                else
                {
                    // Recurse to upper level for datasets such as
                    // /vsis3/hrrrzarr/sfc/20210809/20210809_00z_anl.zarr/0.1_sigma_level/HAIL_max_fcst/0.1_sigma_level/HAIL_max_fcst
                    const std::string osDirNameNew = CPLGetPath(osDirName.c_str());
                    if( !osDirNameNew.empty() && osDirNameNew != osDirName )
                    {
                        osDirName = osDirNameNew;
                        continue;
                    }
                }
                break;
            }
        }

        oIter = m_oMapDimensions.find(osDimName);
        if( oIter != m_oMapDimensions.end() &&
            oIter->second->GetSize() == poDim->GetSize() )
        {
            poDim = oIter->second;
            return true;
        }

        std::string osType;
        std::string osDirection;
        if( aoDims.size() == 1 && osArrayName == osDimName )
        {
            GetDimensionTypeDirection(osType, osDirection);
        }

        auto poDimLocal = std::make_shared<GDALDimensionWeakIndexingVar>(
            GetFullName(), osDimName,
            osType, osDirection, poDim->GetSize());
        m_oMapDimensions[osDimName] = poDimLocal;
        poDim = poDimLocal;
        return true;
    };

    if( arrayDimensionsObj.GetType() == CPLJSONObject::Type::Array )
    {
        const auto arrayDims = arrayDimensionsObj.ToArray();
        if( arrayDims.Size() == oShape.Size() )
        {
            bool ok = true;
            for( int i = 0; i < oShape.Size(); ++i )
            {
                if( arrayDims[i].GetType() == CPLJSONObject::Type::String )
                {
                    const auto osDimName = arrayDims[i].ToString();
                    ok &= FindDimension(osDimName, aoDims[i], i);
                }
            }
            if( ok )
            {
                oAttributes.Delete("_ARRAY_DIMENSIONS");
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Size of _ARRAY_DIMENSIONS different from the one of shape");
        }
    }

    // _NCZARR_ARRAY extension
    const auto nczarrArrayDimrefs = oRoot["_NCZARR_ARRAY"]["dimrefs"].ToArray();
    if( nczarrArrayDimrefs.IsValid() )
    {
        const auto arrayDims = nczarrArrayDimrefs.ToArray();
        if( arrayDims.Size() == oShape.Size() )
        {
            auto poRG = m_pSelf.lock();
            CPLAssert( poRG != nullptr );
            while(true)
            {
                auto poNewRG = poRG->m_poParent.lock();
                if( poNewRG == nullptr )
                    break;
                poRG = poNewRG;
            }

            for( int i = 0; i < oShape.Size(); ++i )
            {
                if( arrayDims[i].GetType() == CPLJSONObject::Type::String )
                {
                    const auto osDimFullpath = arrayDims[i].ToString();
                    auto poDim = poRG->OpenDimensionFromFullname(osDimFullpath);
                    if( poDim )
                    {
                        aoDims[i] = poDim;

                        // If this is an indexing variable, then fetch the
                        // dimension type and direction, and patch the dimension
                        const std::string osArrayFullname =
                            (GetFullName() != "/" ? GetFullName(): std::string()) + '/' + osArrayName;
                        if( aoDims.size() == 1 && osArrayFullname == poDim->GetFullName() )
                        {
                            std::string osType;
                            std::string osDirection;
                            GetDimensionTypeDirection(osType, osDirection);

                            std::string osDimParent = osDimFullpath;
                            const auto nPos = osDimParent.rfind('/');
                            if( nPos != std::string::npos )
                            {
                                if( nPos == 0 )
                                    osDimParent = '/';
                                else
                                    osDimParent.resize(nPos);
                                auto poDimParentGroup = dynamic_cast<ZarrGroupBase*>(
                                    poRG->OpenGroupFromFullname(osDimParent).get());
                                if( poDimParentGroup )
                                {
                                    auto poDimLocal = std::make_shared<GDALDimensionWeakIndexingVar>(
                                        poDimParentGroup->GetFullName(), poDim->GetName(),
                                        osType, osDirection, poDim->GetSize());
                                    aoDims[i] = poDimLocal;

                                    poDimParentGroup->m_oMapDimensions[poDim->GetName()] = poDimLocal;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Size of _NCZARR_ARRAY.dimrefs different from the one of shape");
        }
    }

    const char* dtypeKey = isZarrV2 ? "dtype" : "data_type";
    auto oDtype = oRoot[dtypeKey];
    if( !oDtype.IsValid() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s missing", dtypeKey);
        return nullptr;
    }
    if( !isZarrV2 && oDtype["fallback"].IsValid() )
        oDtype = oDtype["fallback"];
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtype(isZarrV2, oDtype, aoDtypeElts);
    if( oType.GetClass() == GEDTC_NUMERIC && oType.GetNumericDataType() == GDT_Unknown )
        return nullptr;
    size_t iCurElt = 0;
    SetGDALOffset(oType, 0, aoDtypeElts, iCurElt);

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

    std::string osDimSeparator;
    if( isZarrV2 )
    {
        osDimSeparator = oRoot["dimension_separator"].ToString();
        if( osDimSeparator.empty() )
            osDimSeparator = ".";
    }
    else
    {
        osDimSeparator = oRoot["chunk_grid/separator"].ToString();
        if( osDimSeparator.empty() )
            osDimSeparator = "/";
    }

    std::vector<GByte> abyNoData;

    struct NoDataFreer
    {
        std::vector<GByte>& m_abyNodata;
        const GDALExtendedDataType& m_oType;

        NoDataFreer(std::vector<GByte>& abyNoDataIn,
                    const GDALExtendedDataType& oTypeIn) :
                            m_abyNodata(abyNoDataIn), m_oType(oTypeIn) {}

        ~NoDataFreer()
        {
            if( !m_abyNodata.empty() )
                m_oType.FreeDynamicMemory(&m_abyNodata[0]);
        }
    };
    NoDataFreer NoDataFreer(abyNoData, oType);

    auto oFillValue = oRoot["fill_value"];
    auto eFillValueType = oFillValue.GetType();

    // Normally arrays are not supported, but that's what NCZarr 4.8.0 outputs
    if( eFillValueType == CPLJSONObject::Type::Array &&
        oFillValue.ToArray().Size() == 1 )
    {
        oFillValue = oFillValue.ToArray()[0];
        eFillValueType = oFillValue.GetType();
    }

    if( !oFillValue.IsValid() )
    {
        // fill_value is normally required but some implementations
        // are lacking it: https://github.com/Unidata/netcdf-c/issues/2059
        CPLError(CE_Warning, CPLE_AppDefined, "fill_value missing");
    }
    else if( eFillValueType == CPLJSONObject::Type::Null )
    {
        // Nothing to do
    }
    else if( eFillValueType == CPLJSONObject::Type::String )
    {
        const auto osFillValue = oFillValue.ToString();
        if( oType.GetClass() == GEDTC_NUMERIC &&
            CPLGetValueType(osFillValue.c_str()) != CPL_VALUE_STRING )
        {
            // Be tolerant with numeric values serialized as strings.
            const double dfNoDataValue = CPLAtof(osFillValue.c_str());
            abyNoData.resize(oType.GetSize());
            GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                          &abyNoData[0], oType.GetNumericDataType(), 0,
                          1);
        }
        else if( oType.GetClass() == GEDTC_NUMERIC )
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
                const float fNoDataValue = static_cast<float>(dfNoDataValue);
                abyNoData.resize(sizeof(fNoDataValue));
                memcpy( &abyNoData[0], &fNoDataValue, sizeof(fNoDataValue) );
            }
            else if( oType.GetNumericDataType() == GDT_Float64 )
            {
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
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(), osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes + 1);
            abyNativeFillValue[nBytes] = 0;
            abyNoData.resize( oType.GetSize() );
            char* pDstStr = CPLStrdup( reinterpret_cast<const char*>(&abyNativeFillValue[0]) );
            char** pDstPtr = reinterpret_cast<char**>(&abyNoData[0]);
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else
        {
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(), osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes);
            if( abyNativeFillValue.size() != aoDtypeElts.back().nativeOffset +
                                             aoDtypeElts.back().nativeSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            abyNoData.resize( oType.GetSize() );
            DecodeSourceElt( aoDtypeElts,
                             abyNativeFillValue.data(),
                             &abyNoData[0] );
        }
    }
    else if( eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double )
    {
        if( oType.GetClass() == GEDTC_NUMERIC )
        {
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

    const CPLCompressor* psCompressor = nullptr;
    const CPLCompressor* psDecompressor = nullptr;
    const auto oCompressor = oRoot["compressor"];
    std::string osDecompressorId("NONE");
    if( isZarrV2 )
    {
        if( !oCompressor.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "compressor missing");
            return nullptr;
        }
        if( oCompressor.GetType() == CPLJSONObject::Type::Null )
        {
            // nothing to do
        }
        else if( oCompressor.GetType() == CPLJSONObject::Type::Object )
        {
            osDecompressorId = oCompressor["id"].ToString();
            if( osDecompressorId.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Missing compressor id");
                return nullptr;
            }
            psCompressor = CPLGetCompressor( osDecompressorId.c_str() );
            psDecompressor = CPLGetDecompressor( osDecompressorId.c_str() );
            if( psCompressor == nullptr || psDecompressor == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decompressor %s not handled",
                         osDecompressorId.c_str());
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
            return nullptr;
        }
    }
    else if( oCompressor.IsValid() )
    {
        const auto oCodec = oCompressor["codec"];
        if( oCodec.GetType() == CPLJSONObject::Type::String )
        {
            const auto osCodec = oCodec.ToString();
            // See https://github.com/zarr-developers/zarr-specs/pull/119
            // We accept the plural form, but singular is the official one.
            for( const char* key : { "https://purl.org/zarr/spec/codec/",
                                     "https://purl.org/zarr/spec/codecs/" } )
            {
                if( osCodec.find(key) == 0 )
                {
                    auto osCodecName = osCodec.substr(strlen(key));
                    auto posSlash = osCodecName.find('/');
                    if( posSlash != std::string::npos )
                    {
                        osDecompressorId = osCodecName.substr(0, posSlash);
                        psCompressor = CPLGetCompressor( osDecompressorId.c_str() );
                        psDecompressor = CPLGetDecompressor( osDecompressorId.c_str() );
                    }
                    break;
                }
            }
            if( psCompressor == nullptr || psDecompressor == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decompressor %s not handled",
                         osCodec.c_str());
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
            return nullptr;
        }
    }

    CPLJSONArray oFiltersArray;
    if( isZarrV2 )
    {
        const auto oFilters = oRoot["filters"];
        if( !oFilters.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "filters missing");
            return nullptr;
        }
        if( oFilters.GetType() == CPLJSONObject::Type::Null )
        {
        }
        else if( oFilters.GetType() == CPLJSONObject::Type::Array )
        {
            oFiltersArray = oFilters.ToArray();
            for( const auto& oFilter: oFiltersArray )
            {
                const auto osFilterId = oFilter["id"].ToString();
                if( osFilterId.empty() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Missing filter id");
                    return nullptr;
                }
                const auto psFilterCompressor = CPLGetCompressor( osFilterId.c_str() );
                const auto psFilterDecompressor = CPLGetDecompressor( osFilterId.c_str() );
                if( psFilterCompressor == nullptr || psFilterDecompressor == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Filter %s not handled",
                             osFilterId.c_str());
                    return nullptr;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid filters");
            return nullptr;
        }
    }

    auto poArray = ZarrArray::Create(m_poSharedResource,
                                     GetFullName(),
                                     osArrayName,
                                     aoDims, oType, aoDtypeElts, anBlockSize,
                                     bFortranOrder);
    if( !poArray )
        return nullptr;
    poArray->SetUpdatable(m_bUpdatable); // must be set before SetAttributes()
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
    if( isZarrV2 )
        poArray->SetCompressorJsonV2(oCompressor);
    poArray->SetCompressorDecompressor(osDecompressorId, psCompressor, psDecompressor);
    poArray->SetFilters(oFiltersArray);
    if( !abyNoData.empty() )
    {
        poArray->RegisterNoDataValue(abyNoData.data());
    }
    poArray->SetSRS(poSRS);
    poArray->SetAttributes(oAttributes);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetVersion(isZarrV2 ? 2 : 3);
    poArray->SetDtype(oDtype);
    poArray->RegisterUnit(osUnit);
    if( bHasOffset )
        poArray->RegisterOffset(dfOffset);
    if( bHasScale )
        poArray->RegisterScale(dfScale);
    RegisterArray(poArray);

    // If this is an indexing variable, attach it to the dimension.
    if( aoDims.size() == 1 &&
        aoDims[0]->GetName() == poArray->GetName() )
    {
        auto oIter = m_oMapDimensions.find(poArray->GetName());
        if( oIter != m_oMapDimensions.end() )
        {
            oIter->second->SetIndexingVariable(poArray);
        }
    }

    if( CPLTestBool(m_poSharedResource->GetOpenOptions().FetchNameValueDef(
                                                "CACHE_TILE_PRESENCE", "NO")) )
    {
        poArray->CacheTilePresence();
    }

    return poArray;
}

/************************************************************************/
/*                  ZarrArray::OpenTilePresenceCache()                  */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrArray::OpenTilePresenceCache(bool bCanCreate) const
{
    if( m_bHasTriedCacheTilePresenceArray )
        return m_poCacheTilePresenceArray;
    m_bHasTriedCacheTilePresenceArray = true;

    if( m_nTotalTileCount == 1 )
        return nullptr;

    std::string osCacheFilename;
    auto poRGCache = GetCacheRootGroup(bCanCreate, osCacheFilename);
    if( !poRGCache )
        return nullptr;

    const std::string osTilePresenceArrayName(MassageName(GetFullName()) + "_tile_presence");
    auto poTilePresenceArray = poRGCache->OpenMDArray(osTilePresenceArrayName);
    const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);
    if( poTilePresenceArray )
    {
        bool ok = true;
        const auto apoDimsCache = poTilePresenceArray->GetDimensions();
        if( poTilePresenceArray->GetDataType() != eByteDT ||
            apoDimsCache.size() != m_aoDims.size() )
        {
            ok = false;
        }
        else
        {
            for( size_t i = 0; i < m_aoDims.size(); i++ )
            {
                const auto nExpectedDimSize =
                    (m_aoDims[i]->GetSize() + m_anBlockSize[i]-1) / m_anBlockSize[i];
                if( apoDimsCache[i]->GetSize() != nExpectedDimSize )
                {
                    ok = false;
                    break;
                }
            }
        }
        if( !ok )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Array %s in %s has not expected characteristics",
                     osTilePresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }

        if( !poTilePresenceArray->GetAttribute("filling_status") && !bCanCreate )
        {
            CPLDebug(ZARR_DEBUG_KEY,
                     "Cache tile presence array for %s found, but filling not finished",
                     GetFullName().c_str());
            return nullptr;
        }

        CPLDebug(ZARR_DEBUG_KEY, "Using cache tile presence for %s", GetFullName().c_str());
    }
    else if( bCanCreate )
    {
        int idxDim = 0;
        std::string osBlockSize;
        std::vector<std::shared_ptr<GDALDimension>> apoNewDims;
        for( const auto& poDim: m_aoDims )
        {
            auto poNewDim = poRGCache->CreateDimension(
                osTilePresenceArrayName + '_' + std::to_string(idxDim),
                std::string(),
                std::string(),
                (poDim->GetSize() + m_anBlockSize[idxDim]-1) / m_anBlockSize[idxDim]);
            if( !poNewDim )
                return nullptr;
            apoNewDims.emplace_back(poNewDim);

            if( !osBlockSize.empty() )
                osBlockSize += ',';
            constexpr GUInt64 BLOCKSIZE = 256;
            osBlockSize += std::to_string(
                                std::min(poNewDim->GetSize(),BLOCKSIZE));

            idxDim ++;
        }

        CPLStringList aosOptionsTilePresence;
        aosOptionsTilePresence.SetNameValue("BLOCKSIZE", osBlockSize.c_str());
        poTilePresenceArray = poRGCache->CreateMDArray(osTilePresenceArrayName,
                                                       apoNewDims,
                                                       eByteDT,
                                                       aosOptionsTilePresence.List());
        if( !poTilePresenceArray )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot create %s in %s",
                     osTilePresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }
        poTilePresenceArray->SetNoDataValue(0);
    }
    else
    {
        return nullptr;
    }

    m_poCacheTilePresenceArray = poTilePresenceArray;

    return poTilePresenceArray;
}

/************************************************************************/
/*                    ZarrArray::CacheTilePresence()                    */
/************************************************************************/

bool ZarrArray::CacheTilePresence()
{
    if( m_nTotalTileCount == 1 )
        return true;

    const std::string osDirectoryName = [this]()
    {
        if( m_nVersion == 2 )
            return std::string(CPLGetDirname(m_osFilename.c_str()));

        std::string osTmp = m_osRootDirectoryName + "/data/root";
        if( GetFullName() != "/" )
            osTmp += GetFullName();
        return osTmp;
    }
    ();

    struct DirCloser
    {
        DirCloser(const DirCloser&) = delete;
        DirCloser& operator= (const DirCloser&) = delete;

        VSIDIR* m_psDir;

        explicit DirCloser(VSIDIR* psDir): m_psDir(psDir) {}
        ~DirCloser() { VSICloseDir(m_psDir); }
    };

    auto psDir = VSIOpenDir(osDirectoryName.c_str(), -1, nullptr);
    if( !psDir )
        return false;
    DirCloser dirCloser(psDir);

    auto poTilePresenceArray = OpenTilePresenceCache(true);
    if( !poTilePresenceArray )
    {
        return false;
    }

    if( poTilePresenceArray->GetAttribute("filling_status") )
    {
        CPLDebug(ZARR_DEBUG_KEY,
                 "CacheTilePresence(): %s already filled. Nothing to do",
                 poTilePresenceArray->GetName().c_str());
        return true;
    }

    std::vector<GUInt64> anTileIdx(m_aoDims.size());
    const std::vector<size_t> anCount(m_aoDims.size(), 1);
    const std::vector<GInt64> anArrayStep(m_aoDims.size(), 0);
    const std::vector<GPtrDiff_t> anBufferStride(m_aoDims.size(), 0);
    const auto apoDimsCache = poTilePresenceArray->GetDimensions();
    const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);

    CPLDebug(ZARR_DEBUG_KEY,
             "CacheTilePresence(): Iterating over %s to find which tiles are present...",
             osDirectoryName.c_str());
    uint64_t nCounter = 0;
    while( const VSIDIREntry* psEntry = VSIGetNextDirEntry(psDir) )
    {
        if( !VSI_ISDIR(psEntry->nMode) )
        {
            int nOff = 0;
            if( m_nVersion == 3 )
            {
                if( psEntry->pszName[0] != 'c' )
                    continue;
                nOff = 1;
            }
            const CPLStringList aosTokens(
                CSLTokenizeString2( psEntry->pszName + nOff, m_osDimSeparator.c_str(), 0 ));
            if( aosTokens.size() == static_cast<int>(m_aoDims.size()) )
            {
                // Get tile indices from filename
                bool unexpectedIndex = false;
                for( int i = 0; i < aosTokens.size(); ++i )
                {
                    if( CPLGetValueType(aosTokens[i]) != CPL_VALUE_INTEGER )
                    {
                        unexpectedIndex = true;
                    }
                    anTileIdx[i] = static_cast<GUInt64>(CPLAtoGIntBig(aosTokens[i]));
                    if( anTileIdx[i] >= apoDimsCache[i]->GetSize() )
                    {
                        unexpectedIndex = true;
                    }
                }
                if( unexpectedIndex )
                {
                    continue;
                }

                nCounter ++;
                if( (nCounter % 1000) == 0 )
                {
                    CPLDebug(ZARR_DEBUG_KEY,
                             "CacheTilePresence(): Listing in progress "
                             "(last examined %s, at least %.02f %% completed)",
                             psEntry->pszName,
                             100.0 * double(nCounter) / double(m_nTotalTileCount));
                }
                constexpr GByte byOne = 1;
                //CPLDebugOnly(ZARR_DEBUG_KEY, "Marking %s has present", psEntry->pszName);
                if( !poTilePresenceArray->Write(anTileIdx.data(),
                                                anCount.data(),
                                                anArrayStep.data(),
                                                anBufferStride.data(),
                                                eByteDT,
                                                &byOne) )
                {
                    return false;
                }
            }
        }
    }
    CPLDebug(ZARR_DEBUG_KEY, "CacheTilePresence(): finished");

    // Write filling_status attribute
    auto poAttr = poTilePresenceArray->CreateAttribute(
        "filling_status", {}, GDALExtendedDataType::CreateString(), nullptr );
    if( poAttr )
    {
        if( nCounter == 0 )
            poAttr->Write("no_tile_present");
        else if( nCounter == m_nTotalTileCount )
            poAttr->Write("all_tiles_present");
        else
            poAttr->Write("some_tiles_missing");
    }

    // Force closing
    m_poCacheTilePresenceArray = nullptr;
    m_bHasTriedCacheTilePresenceArray = false;

    return true;
}

/************************************************************************/
/*                      ZarrArray::CreateAttribute()                    */
/************************************************************************/

std::shared_ptr<GDALAttribute> ZarrArray::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if( anDimensions.size() >= 2 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create attributes of dimension >= 2");
        return nullptr;
    }
    return m_oAttrGroup.CreateAttribute(osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                      ZarrArray::SetSpatialRef()                      */
/************************************************************************/

bool ZarrArray::SetSpatialRef(const OGRSpatialReference* poSRS)
{
    if( !m_bUpdatable )
    {
        return GDALPamMDArray::SetSpatialRef(poSRS);
    }
    m_poSRS.reset();
    if( poSRS )
        m_poSRS.reset(poSRS->Clone());
    m_bSRSModified = true;
    return true;
}

/************************************************************************/
/*                         ZarrArray::SetUnit()                         */
/************************************************************************/

bool ZarrArray::SetUnit(const std::string& osUnit)
{
    if( !m_bUpdatable )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }
    m_osUnit = osUnit;
    m_bUnitModified = true;
    return true;
}

/************************************************************************/
/*                       ZarrArray::GetOffset()                         */
/************************************************************************/

double ZarrArray::GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const
{
    if( pbHasOffset )
        *pbHasOffset = m_bHasOffset;
    if( peStorageType )
        *peStorageType = GDT_Unknown;
    return m_dfOffset;
}

/************************************************************************/
/*                       ZarrArray::GetScale()                          */
/************************************************************************/

double ZarrArray::GetScale(bool* pbHasScale, GDALDataType* peStorageType) const
{
    if( pbHasScale )
        *pbHasScale = m_bHasScale;
    if( peStorageType )
        *peStorageType = GDT_Unknown;
    return m_dfScale;
}

/************************************************************************/
/*                       ZarrArray::SetOffset()                         */
/************************************************************************/

bool ZarrArray::SetOffset(double dfOffset, GDALDataType /* eStorageType */)
{
    m_dfOffset = dfOffset;
    m_bHasOffset = true;
    m_bOffsetModified = true;
    return true;
}

/************************************************************************/
/*                       ZarrArray::SetScale()                          */
/************************************************************************/

bool ZarrArray::SetScale(double dfScale, GDALDataType /* eStorageType */)
{
    m_dfScale = dfScale;
    m_bHasScale = true;
    m_bScaleModified = true;
    return true;
}

/************************************************************************/
/*                      GetCoordinateVariables()                        */
/************************************************************************/

std::vector<std::shared_ptr<GDALMDArray>> ZarrArray::GetCoordinateVariables() const
{
    std::vector<std::shared_ptr<GDALMDArray>> ret;
    const auto poCoordinates = GetAttribute("coordinates");
    if( poCoordinates && poCoordinates->GetDataType().GetClass() == GEDTC_STRING &&
        poCoordinates->GetDimensionCount() == 0 )
    {
        const char* pszCoordinates = poCoordinates->ReadAsString();
        if( pszCoordinates )
        {
            auto poGroup = m_poGroupWeak.lock();
            if( !poGroup )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot access coordinate variables of %s has "
                         "belonging group has gone out of scope",
                         GetName().c_str());
            }
            else
            {
                const CPLStringList aosNames(CSLTokenizeString2(pszCoordinates, " ", 0));
                for( int i = 0; i < aosNames.size(); i++ )
                {
                    auto poCoordinateVar = poGroup->OpenMDArray(aosNames[i]);
                    if( poCoordinateVar )
                    {
                        ret.emplace_back(poCoordinateVar);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find variable corresponding to coordinate %s",
                                 aosNames[i]);
                    }
                }
            }
        }
    }

    return ret;
}
