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

#include "zarr.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                       ZarrV3Array::ZarrV3Array()                     */
/************************************************************************/

ZarrV3Array::ZarrV3Array(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anBlockSize, bool bFortranOrder)
    : GDALAbstractMDArray(osParentName, osName),
      ZarrArray(poSharedResource, osParentName, osName, aoDims, oType,
                aoDtypeElts, anBlockSize, bFortranOrder)
{
    m_oCompressorJSon.Deinit();
}

/************************************************************************/
/*                         ZarrV3Array::Create()                        */
/************************************************************************/

std::shared_ptr<ZarrV3Array>
ZarrV3Array::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName,
                    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
                    const GDALExtendedDataType &oType,
                    const std::vector<DtypeElt> &aoDtypeElts,
                    const std::vector<GUInt64> &anBlockSize, bool bFortranOrder)
{
    uint64_t nTotalTileCount = 1;
    for (size_t i = 0; i < aoDims.size(); ++i)
    {
        uint64_t nTileThisDim =
            (aoDims[i]->GetSize() / anBlockSize[i]) +
            (((aoDims[i]->GetSize() % anBlockSize[i]) != 0) ? 1 : 0);
        if (nTileThisDim != 0 &&
            nTotalTileCount >
                std::numeric_limits<uint64_t>::max() / nTileThisDim)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Array %s has more than 2^64 tiles. This is not supported.",
                osName.c_str());
            return nullptr;
        }
        nTotalTileCount *= nTileThisDim;
    }

    auto arr = std::shared_ptr<ZarrV3Array>(
        new ZarrV3Array(poSharedResource, osParentName, osName, aoDims, oType,
                        aoDtypeElts, anBlockSize, bFortranOrder));
    arr->SetSelf(arr);

    arr->m_nTotalTileCount = nTotalTileCount;
    arr->m_bUseOptimizedCodePaths = CPLTestBool(
        CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES"));

    return arr;
}

/************************************************************************/
/*                             ~ZarrV3Array()                           */
/************************************************************************/

ZarrV3Array::~ZarrV3Array()
{
    ZarrV3Array::Flush();
}

/************************************************************************/
/*                                Flush()                               */
/************************************************************************/

void ZarrV3Array::Flush()
{
    ZarrV3Array::FlushDirtyTile();

    CPLJSONArray j_ARRAY_DIMENSIONS;
    if (!m_aoDims.empty())
    {
        for (const auto &poDim : m_aoDims)
        {
            const auto poZarrDim =
                dynamic_cast<const ZarrDimension *>(poDim.get());
            if (poZarrDim && poZarrDim->IsXArrayDimension())
            {
                if (poZarrDim->IsModified())
                    m_bDefinitionModified = true;
                j_ARRAY_DIMENSIONS.Add(poDim->GetName());
            }
            else
            {
                j_ARRAY_DIMENSIONS = CPLJSONArray();
                break;
            }
        }
    }

    CPLJSONObject oAttrs;
    if (m_oAttrGroup.IsModified() || m_bUnitModified || m_bOffsetModified ||
        m_bScaleModified || m_bSRSModified)
    {
        m_bNew = false;

        oAttrs = SerializeSpecialAttributes();

        m_bDefinitionModified = true;
    }

    if (m_bDefinitionModified)
    {
        if (j_ARRAY_DIMENSIONS.Size() != 0)
        {
            oAttrs.Delete("_ARRAY_DIMENSIONS");
            oAttrs.Add("_ARRAY_DIMENSIONS", j_ARRAY_DIMENSIONS);
        }

        Serialize(oAttrs);
        m_bDefinitionModified = false;
    }
}

/************************************************************************/
/*           StripUselessItemsFromCompressorConfiguration()             */
/************************************************************************/

static void StripUselessItemsFromCompressorConfiguration(CPLJSONObject &o)
{
    if (o.GetType() == CPLJSONObject::Type::Object)
    {
        o.Delete("num_threads");  // Blosc
        o.Delete("typesize");     // Blosc
        o.Delete("header");       // LZ4
    }
}

/************************************************************************/
/*                    ZarrV3Array::Serialize()                          */
/************************************************************************/

void ZarrV3Array::Serialize(const CPLJSONObject &oAttrs)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    CPLJSONArray oShape;
    for (const auto &poDim : m_aoDims)
    {
        oShape.Add(static_cast<GInt64>(poDim->GetSize()));
    }
    oRoot.Add("shape", oShape);

    oRoot.Add("data_type", m_dtype.ToString());

    CPLJSONObject oChunkGrid;
    oChunkGrid.Add("type", "regular");
    CPLJSONArray oChunks;
    for (const auto nBlockSize : m_anBlockSize)
    {
        oChunks.Add(static_cast<GInt64>(nBlockSize));
    }
    oChunkGrid.Add("chunk_shape", oChunks);
    oChunkGrid.Add("separator", m_osDimSeparator);
    oRoot.Add("chunk_grid", oChunkGrid);

    if (m_oCompressorJSon.IsValid())
    {
        oRoot.Add("compressor", m_oCompressorJSon);
        CPLJSONObject oConfiguration = oRoot["compressor"]["configuration"];
        StripUselessItemsFromCompressorConfiguration(oConfiguration);
    }

    if (m_pabyNoData == nullptr)
    {
        oRoot.AddNull("fill_value");
    }
    else
    {
        SerializeNumericNoData(oRoot);
    }

    oRoot.Add("chunk_memory_layout", m_bFortranOrder ? "F" : "C");

    oRoot.Add("extensions", CPLJSONArray());

    oRoot.Add("attributes", oAttrs);

    oDoc.Save(m_osFilename);
}

/************************************************************************/
/*                    ZarrV3Array::FlushDirtyTile()                     */
/************************************************************************/

bool ZarrV3Array::FlushDirtyTile() const
{
    if (!m_bDirtyTile)
        return true;
    m_bDirtyTile = false;

    std::string osFilename = BuildTileFilename(m_anCachedTiledIndices.data());

    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
    auto &abyTile =
        m_abyDecodedTileData.empty() ? m_abyRawTileData : m_abyDecodedTileData;

    if (IsEmptyTile(abyTile))
    {
        m_bCachedTiledEmpty = true;

        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
        {
            CPLDebugOnly(ZARR_DEBUG_KEY,
                         "Deleting tile %s that has now empty content",
                         osFilename.c_str());
            return VSIUnlink(osFilename.c_str()) == 0;
        }
        return true;
    }

    if (!m_abyDecodedTileData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        GByte *pDst = &m_abyRawTileData[0];
        const GByte *pSrc = m_abyDecodedTileData.data();
        for (size_t i = 0; i < nValues;
             i++, pDst += nSourceSize, pSrc += nDTSize)
        {
            EncodeElt(m_aoDtypeElts, pSrc, pDst);
        }
    }

    if (m_bFortranOrder && !m_aoDims.empty())
    {
        BlockTranspose(m_abyRawTileData, m_abyTmpRawTileData, false);
        std::swap(m_abyRawTileData, m_abyTmpRawTileData);
    }

    size_t nRawDataSize = m_abyRawTileData.size();
    CPLAssert(m_oFiltersArray.Size() == 0);

    if (m_osDimSeparator == "/")
    {
        std::string osDir = CPLGetDirname(osFilename.c_str());
        VSIStatBufL sStat;
        if (VSIStatL(osDir.c_str(), &sStat) != 0)
        {
            if (VSIMkdirRecursive(osDir.c_str(), 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osDir.c_str());
                return false;
            }
        }
    }

    VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create tile %s",
                 osFilename.c_str());
        return false;
    }

    bool bRet = true;
    if (m_psCompressor == nullptr)
    {
        if (VSIFWriteL(m_abyRawTileData.data(), 1, nRawDataSize, fp) !=
            nRawDataSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly", osFilename.c_str());
            bRet = false;
        }
    }
    else
    {
        std::vector<GByte> abyCompressedData;
        try
        {
            constexpr size_t MIN_BUF_SIZE = 64;  // somewhat arbitrary
            abyCompressedData.resize(static_cast<size_t>(
                MIN_BUF_SIZE + nRawDataSize + nRawDataSize / 3));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for tile %s", osFilename.c_str());
            bRet = false;
        }

        if (bRet)
        {
            void *out_buffer = &abyCompressedData[0];
            size_t out_size = abyCompressedData.size();
            CPLStringList aosOptions;
            const auto compressorConfig = m_oCompressorJSon["configuration"];
            for (const auto &obj : compressorConfig.GetChildren())
            {
                aosOptions.SetNameValue(obj.GetName().c_str(),
                                        obj.ToString().c_str());
            }
            if (EQUAL(m_psCompressor->pszId, "blosc") &&
                m_oType.GetClass() == GEDTC_NUMERIC)
            {
                aosOptions.SetNameValue(
                    "TYPESIZE",
                    CPLSPrintf("%d", GDALGetDataTypeSizeBytes(
                                         GDALGetNonComplexDataType(
                                             m_oType.GetNumericDataType()))));
            }

            if (!m_psCompressor->pfnFunc(
                    m_abyRawTileData.data(), nRawDataSize, &out_buffer,
                    &out_size, aosOptions.List(), m_psCompressor->user_data))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Compression of tile %s failed", osFilename.c_str());
                bRet = false;
            }
            abyCompressedData.resize(out_size);
        }

        if (bRet &&
            VSIFWriteL(abyCompressedData.data(), 1, abyCompressedData.size(),
                       fp) != abyCompressedData.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not write tile %s correctly", osFilename.c_str());
            bRet = false;
        }
    }
    VSIFCloseL(fp);

    return bRet;
}

/************************************************************************/
/*                          BuildTileFilename()                         */
/************************************************************************/

std::string ZarrV3Array::BuildTileFilename(const uint64_t *tileIndices) const
{
    std::string osFilename;
    if (m_aoDims.empty())
    {
        osFilename = "0";
    }
    else
    {
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            if (!osFilename.empty())
                osFilename += m_osDimSeparator;
            osFilename += std::to_string(tileIndices[i]);
        }
    }

    std::string osTmp = m_osRootDirectoryName + "/data/root";
    if (GetFullName() != "/")
        osTmp += GetFullName();
    osFilename = osTmp + "/c" + osFilename;

    return osFilename;
}

/************************************************************************/
/*                          GetDataDirectory()                          */
/************************************************************************/

std::string ZarrV3Array::GetDataDirectory() const
{
    std::string osTmp = m_osRootDirectoryName + "/data/root";
    if (GetFullName() != "/")
        osTmp += GetFullName();
    return osTmp;
}

/************************************************************************/
/*                        GetTileIndicesFromFilename()                  */
/************************************************************************/

CPLStringList
ZarrV3Array::GetTileIndicesFromFilename(const char *pszFilename) const
{
    if (pszFilename[0] != 'c')
        return CPLStringList();
    return CPLStringList(
        CSLTokenizeString2(pszFilename + 1, m_osDimSeparator.c_str(), 0));
}

/************************************************************************/
/*                             ParseDtype()                             */
/************************************************************************/

static GDALExtendedDataType ParseDtype(const CPLJSONObject &obj,
                                       std::vector<DtypeElt> &elts)
{
    do
    {
        if (obj.GetType() == CPLJSONObject::Type::String)
        {
            const auto str = obj.ToString();
            char chEndianness = 0;
            char chType;
            int nBytes;
            DtypeElt elt;

            if (str.size() < 2)
                break;
            if (str == "bool")
            {
                chType = 'b';
                nBytes = 1;
            }
            else if (str == "u1" || str == "i1")
            {
                chType = str[0];
                nBytes = 1;
            }
            else
            {
                if (str.size() < 3)
                    break;
                chEndianness = str[0];
                chType = str[1];
                nBytes = atoi(str.c_str() + 2);
            }

            if (nBytes <= 0 || nBytes >= 1000)
                break;

            elt.needByteSwapping = false;
            if ((nBytes > 1 && chType != 'S') || chType == 'U')
            {
                if (chEndianness == '<')
                    elt.needByteSwapping = (CPL_IS_LSB == 0);
                else if (chEndianness == '>')
                    elt.needByteSwapping = (CPL_IS_LSB != 0);
            }

            GDALDataType eDT;
            if (!elts.empty())
            {
                elt.nativeOffset =
                    elts.back().nativeOffset + elts.back().nativeSize;
            }
            elt.nativeSize = nBytes;
            if (chType == 'b' && nBytes == 1)  // boolean
            {
                elt.nativeType = DtypeElt::NativeType::BOOLEAN;
                eDT = GDT_Byte;
            }
            else if (chType == 'u' && nBytes == 1)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_Byte;
            }
            else if (chType == 'i' && nBytes == 1)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int8;
            }
            else if (chType == 'i' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int16;
            }
            else if (chType == 'i' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int32;
            }
            else if (chType == 'i' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
                eDT = GDT_Int64;
            }
            else if (chType == 'u' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt16;
            }
            else if (chType == 'u' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt32;
            }
            else if (chType == 'u' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
                eDT = GDT_UInt64;
            }
            else if (chType == 'f' && nBytes == 2)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                elt.gdalTypeIsApproxOfNative = true;
                eDT = GDT_Float32;
            }
            else if (chType == 'f' && nBytes == 4)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float32;
            }
            else if (chType == 'f' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::IEEEFP;
                eDT = GDT_Float64;
            }
            else if (chType == 'c' && nBytes == 8)
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat32;
            }
            else if (chType == 'c' && nBytes == 16)
            {
                elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
                eDT = GDT_CFloat64;
            }
            else
                break;
            elt.gdalType = GDALExtendedDataType::Create(eDT);
            elt.gdalSize = elt.gdalType.GetSize();
            elts.emplace_back(elt);
            return GDALExtendedDataType::Create(eDT);
        }
    } while (false);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Invalid or unsupported format for dtype: %s",
             obj.ToString().c_str());
    return GDALExtendedDataType::Create(GDT_Unknown);
}

/************************************************************************/
/*                     ZarrV3Group::LoadArray()                         */
/************************************************************************/

std::shared_ptr<ZarrArray>
ZarrV3Group::LoadArray(const std::string &osArrayName,
                       const std::string &osZarrayFilename,
                       const CPLJSONObject &oRoot,
                       std::set<std::string> &oSetFilenamesInLoading) const
{
    // Prevent too deep or recursive array loading
    if (oSetFilenamesInLoading.find(osZarrayFilename) !=
        oSetFilenamesInLoading.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt at recursively loading %s", osZarrayFilename.c_str());
        return nullptr;
    }
    if (oSetFilenamesInLoading.size() == 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too deep call stack in LoadArray()");
        return nullptr;
    }

    struct SetFilenameAdder
    {
        std::set<std::string> &m_oSetFilenames;
        std::string m_osFilename;

        SetFilenameAdder(std::set<std::string> &oSetFilenamesIn,
                         const std::string &osFilename)
            : m_oSetFilenames(oSetFilenamesIn), m_osFilename(osFilename)
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

    bool bFortranOrder = false;
    const char *orderKey = "chunk_memory_layout";
    const auto osOrder = oRoot[orderKey].ToString();
    if (osOrder == "C")
    {
        // ok
    }
    else if (osOrder == "F")
    {
        bFortranOrder = true;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid value for %s",
                 orderKey);
        return nullptr;
    }

    const auto oShape = oRoot["shape"].ToArray();
    if (!oShape.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "shape missing or not an array");
        return nullptr;
    }

    const char *chunksKey = "chunk_grid/chunk_shape";
    const auto oChunks = oRoot[chunksKey].ToArray();
    if (!oChunks.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s missing or not an array",
                 chunksKey);
        return nullptr;
    }

    if (oShape.Size() != oChunks.Size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "shape and chunks arrays are of different size");
        return nullptr;
    }

    CPLJSONObject oAttributes = oRoot["attributes"];

    // Deep-clone of oAttributes
    {
        CPLJSONDocument oTmpDoc;
        oTmpDoc.SetRoot(oAttributes);
        CPL_IGNORE_RET_VAL(oTmpDoc.LoadMemory(oTmpDoc.SaveAsString()));
        oAttributes = oTmpDoc.GetRoot();
    }

    std::vector<std::shared_ptr<GDALDimension>> aoDims;
    for (int i = 0; i < oShape.Size(); ++i)
    {
        const auto nSize = static_cast<GUInt64>(oShape[i].ToLong());
        if (nSize == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for shape");
            return nullptr;
        }
        aoDims.emplace_back(std::make_shared<ZarrDimension>(
            m_poSharedResource, m_pSelf, std::string(), CPLSPrintf("dim%d", i),
            std::string(), std::string(), nSize));
    }

    // XArray extension
    const auto arrayDimensionsObj = oAttributes["_ARRAY_DIMENSIONS"];

    const auto FindDimension =
        [this, &aoDims, &osArrayName, &oAttributes,
         &oSetFilenamesInLoading](const std::string &osDimName,
                                  std::shared_ptr<GDALDimension> &poDim, int i)
    {
        auto oIter = m_oMapDimensions.find(osDimName);
        if (oIter != m_oMapDimensions.end())
        {
            if (m_bDimSizeInUpdate ||
                oIter->second->GetSize() == poDim->GetSize())
            {
                poDim = oIter->second;
                return true;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Size of _ARRAY_DIMENSIONS[%d] different "
                         "from the one of shape",
                         i);
                return false;
            }
        }

        // Try to load the indexing variable.
        // Not in m_oMapMDArrays,
        // then stat() the indexing variable.
        else if (osArrayName != osDimName &&
                 m_oMapMDArrays.find(osDimName) == m_oMapMDArrays.end())
        {
            std::string osDirName = m_osDirectoryName;
            while (true)
            {
                const std::string osArrayFilenameDim =
                    CPLFormFilename(CPLFormFilename(osDirName.c_str(),
                                                    osDimName.c_str(), nullptr),
                                    ".zarray", nullptr);
                VSIStatBufL sStat;
                if (VSIStatL(osArrayFilenameDim.c_str(), &sStat) == 0)
                {
                    CPLJSONDocument oDoc;
                    if (oDoc.Load(osArrayFilenameDim))
                    {
                        LoadArray(osDimName, osArrayFilenameDim, oDoc.GetRoot(),
                                  oSetFilenamesInLoading);
                    }
                }
                else
                {
                    // Recurse to upper level for datasets such as
                    // /vsis3/hrrrzarr/sfc/20210809/20210809_00z_anl.zarr/0.1_sigma_level/HAIL_max_fcst/0.1_sigma_level/HAIL_max_fcst
                    const std::string osDirNameNew =
                        CPLGetPath(osDirName.c_str());
                    if (!osDirNameNew.empty() && osDirNameNew != osDirName)
                    {
                        osDirName = osDirNameNew;
                        continue;
                    }
                }
                break;
            }
        }

        oIter = m_oMapDimensions.find(osDimName);
        if (oIter != m_oMapDimensions.end() &&
            oIter->second->GetSize() == poDim->GetSize())
        {
            poDim = oIter->second;
            return true;
        }

        std::string osType;
        std::string osDirection;
        if (aoDims.size() == 1 && osArrayName == osDimName)
        {
            ZarrArray::GetDimensionTypeDirection(oAttributes, osType,
                                                 osDirection);
        }

        auto poDimLocal = std::make_shared<ZarrDimension>(
            m_poSharedResource, m_pSelf, GetFullName(), osDimName, osType,
            osDirection, poDim->GetSize());
        poDimLocal->SetXArrayDimension();
        m_oMapDimensions[osDimName] = poDimLocal;
        poDim = poDimLocal;
        return true;
    };

    if (arrayDimensionsObj.GetType() == CPLJSONObject::Type::Array)
    {
        const auto arrayDims = arrayDimensionsObj.ToArray();
        if (arrayDims.Size() == oShape.Size())
        {
            bool ok = true;
            for (int i = 0; i < oShape.Size(); ++i)
            {
                if (arrayDims[i].GetType() == CPLJSONObject::Type::String)
                {
                    const auto osDimName = arrayDims[i].ToString();
                    ok &= FindDimension(osDimName, aoDims[i], i);
                }
            }
            if (ok)
            {
                oAttributes.Delete("_ARRAY_DIMENSIONS");
            }
        }
        else
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Size of _ARRAY_DIMENSIONS different from the one of shape");
        }
    }

    const char *dtypeKey = "data_type";
    auto oDtype = oRoot[dtypeKey];
    if (!oDtype.IsValid())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "%s missing", dtypeKey);
        return nullptr;
    }
    if (oDtype["fallback"].IsValid())
        oDtype = oDtype["fallback"];
    std::vector<DtypeElt> aoDtypeElts;
    const auto oType = ParseDtype(oDtype, aoDtypeElts);
    if (oType.GetClass() == GEDTC_NUMERIC &&
        oType.GetNumericDataType() == GDT_Unknown)
        return nullptr;

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::ParseChunkSize(oChunks, oType, anBlockSize))
        return nullptr;

    std::string osDimSeparator = oRoot["chunk_grid/separator"].ToString();
    if (osDimSeparator.empty())
        osDimSeparator = "/";

    std::vector<GByte> abyNoData;

    struct NoDataFreer
    {
        std::vector<GByte> &m_abyNodata;
        const GDALExtendedDataType &m_oType;

        NoDataFreer(std::vector<GByte> &abyNoDataIn,
                    const GDALExtendedDataType &oTypeIn)
            : m_abyNodata(abyNoDataIn), m_oType(oTypeIn)
        {
        }

        ~NoDataFreer()
        {
            if (!m_abyNodata.empty())
                m_oType.FreeDynamicMemory(&m_abyNodata[0]);
        }
    };
    NoDataFreer NoDataFreer(abyNoData, oType);

    auto oFillValue = oRoot["fill_value"];
    auto eFillValueType = oFillValue.GetType();

    if (!oFillValue.IsValid())
    {
        // fill_value is normally required but some implementations
        // are lacking it: https://github.com/Unidata/netcdf-c/issues/2059
        CPLError(CE_Warning, CPLE_AppDefined, "fill_value missing");
    }
    else if (eFillValueType == CPLJSONObject::Type::Null)
    {
        // Nothing to do
    }
    else if (eFillValueType == CPLJSONObject::Type::String)
    {
        const auto osFillValue = oFillValue.ToString();
        if (oType.GetClass() == GEDTC_NUMERIC &&
            CPLGetValueType(osFillValue.c_str()) != CPL_VALUE_STRING)
        {
            abyNoData.resize(oType.GetSize());
            // Be tolerant with numeric values serialized as strings.
            if (oType.GetNumericDataType() == GDT_Int64)
            {
                const int64_t nVal = static_cast<int64_t>(
                    std::strtoll(osFillValue.c_str(), nullptr, 10));
                GDALCopyWords(&nVal, GDT_Int64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else if (oType.GetNumericDataType() == GDT_UInt64)
            {
                const uint64_t nVal = static_cast<uint64_t>(
                    std::strtoull(osFillValue.c_str(), nullptr, 10));
                GDALCopyWords(&nVal, GDT_UInt64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else
            {
                const double dfNoDataValue = CPLAtof(osFillValue.c_str());
                GDALCopyWords(&dfNoDataValue, GDT_Float64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
        }
        else if (oType.GetClass() == GEDTC_NUMERIC)
        {
            double dfNoDataValue;
            if (osFillValue == "NaN")
            {
                dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
            }
            else if (osFillValue == "Infinity")
            {
                dfNoDataValue = std::numeric_limits<double>::infinity();
            }
            else if (osFillValue == "-Infinity")
            {
                dfNoDataValue = -std::numeric_limits<double>::infinity();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            if (oType.GetNumericDataType() == GDT_Float32)
            {
                const float fNoDataValue = static_cast<float>(dfNoDataValue);
                abyNoData.resize(sizeof(fNoDataValue));
                memcpy(&abyNoData[0], &fNoDataValue, sizeof(fNoDataValue));
            }
            else if (oType.GetNumericDataType() == GDT_Float64)
            {
                abyNoData.resize(sizeof(dfNoDataValue));
                memcpy(&abyNoData[0], &dfNoDataValue, sizeof(dfNoDataValue));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
        }
        else if (oType.GetClass() == GEDTC_STRING)
        {
            // zarr.open('unicode_be.zarr', mode = 'w', shape=(1,), dtype =
            // '>U1', compressor = None) oddly generates "fill_value": "0"
            if (osFillValue != "0")
            {
                std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
                memcpy(&abyNativeFillValue[0], osFillValue.data(),
                       osFillValue.size());
                int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
                abyNativeFillValue.resize(nBytes + 1);
                abyNativeFillValue[nBytes] = 0;
                abyNoData.resize(oType.GetSize());
                char *pDstStr = CPLStrdup(
                    reinterpret_cast<const char *>(&abyNativeFillValue[0]));
                char **pDstPtr = reinterpret_cast<char **>(&abyNoData[0]);
                memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
            }
        }
        else
        {
            std::vector<GByte> abyNativeFillValue(osFillValue.size() + 1);
            memcpy(&abyNativeFillValue[0], osFillValue.data(),
                   osFillValue.size());
            int nBytes = CPLBase64DecodeInPlace(&abyNativeFillValue[0]);
            abyNativeFillValue.resize(nBytes);
            if (abyNativeFillValue.size() !=
                aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid fill_value");
                return nullptr;
            }
            abyNoData.resize(oType.GetSize());
            ZarrArray::DecodeSourceElt(aoDtypeElts, abyNativeFillValue.data(),
                                       &abyNoData[0]);
        }
    }
    else if (eFillValueType == CPLJSONObject::Type::Boolean ||
             eFillValueType == CPLJSONObject::Type::Integer ||
             eFillValueType == CPLJSONObject::Type::Long ||
             eFillValueType == CPLJSONObject::Type::Double)
    {
        if (oType.GetClass() == GEDTC_NUMERIC)
        {
            const double dfNoDataValue = oFillValue.ToDouble();
            if (oType.GetNumericDataType() == GDT_Int64)
            {
                const int64_t nNoDataValue =
                    static_cast<int64_t>(oFillValue.ToLong());
                abyNoData.resize(oType.GetSize());
                GDALCopyWords(&nNoDataValue, GDT_Int64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else if (oType.GetNumericDataType() == GDT_UInt64 &&
                     /* we can't really deal with nodata value between */
                     /* int64::max and uint64::max due to json-c limitations */
                     dfNoDataValue >= 0)
            {
                const int64_t nNoDataValue =
                    static_cast<int64_t>(oFillValue.ToLong());
                abyNoData.resize(oType.GetSize());
                GDALCopyWords(&nNoDataValue, GDT_Int64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
            else
            {
                abyNoData.resize(oType.GetSize());
                GDALCopyWords(&dfNoDataValue, GDT_Float64, 0, &abyNoData[0],
                              oType.GetNumericDataType(), 0, 1);
            }
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

    const CPLCompressor *psCompressor = nullptr;
    const CPLCompressor *psDecompressor = nullptr;
    const auto oCompressor = oRoot["compressor"];
    std::string osDecompressorId("NONE");
    if (oCompressor.IsValid())
    {
        const auto oCodec = oCompressor["codec"];
        if (oCodec.GetType() == CPLJSONObject::Type::String)
        {
            const auto osCodec = oCodec.ToString();
            // See https://github.com/zarr-developers/zarr-specs/pull/119
            // We accept the plural form, but singular is the official one.
            for (const char *key : {"https://purl.org/zarr/spec/codec/",
                                    "https://purl.org/zarr/spec/codecs/"})
            {
                if (osCodec.find(key) == 0)
                {
                    auto osCodecName = osCodec.substr(strlen(key));
                    auto posSlash = osCodecName.find('/');
                    if (posSlash != std::string::npos)
                    {
                        osDecompressorId = osCodecName.substr(0, posSlash);
                        psCompressor =
                            CPLGetCompressor(osDecompressorId.c_str());
                        psDecompressor =
                            CPLGetDecompressor(osDecompressorId.c_str());
                    }
                    break;
                }
            }
            if (psCompressor == nullptr || psDecompressor == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decompressor %s not handled", osCodec.c_str());
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid compressor");
            return nullptr;
        }
    }

    auto poArray = ZarrV3Array::Create(m_poSharedResource, GetFullName(),
                                       osArrayName, aoDims, oType, aoDtypeElts,
                                       anBlockSize, bFortranOrder);
    poArray->SetUpdatable(m_bUpdatable);  // must be set before SetAttributes()
    poArray->SetFilename(osZarrayFilename);
    poArray->SetDimSeparator(osDimSeparator);
    poArray->SetCompressorDecompressor(osDecompressorId, psCompressor,
                                       psDecompressor);
    if (!abyNoData.empty())
    {
        poArray->RegisterNoDataValue(abyNoData.data());
    }
    poArray->ParseSpecialAttributes(oAttributes);
    poArray->SetAttributes(oAttributes);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetDtype(oDtype);
    RegisterArray(poArray);

    // If this is an indexing variable, attach it to the dimension.
    if (aoDims.size() == 1 && aoDims[0]->GetName() == poArray->GetName())
    {
        auto oIter = m_oMapDimensions.find(poArray->GetName());
        if (oIter != m_oMapDimensions.end())
        {
            oIter->second->SetIndexingVariable(poArray);
        }
    }

    if (CPLTestBool(m_poSharedResource->GetOpenOptions().FetchNameValueDef(
            "CACHE_TILE_PRESENCE", "NO")))
    {
        poArray->CacheTilePresence();
    }

    return poArray;
}
