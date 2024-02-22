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
#include <limits>
#include <map>
#include <set>

/************************************************************************/
/*                      ZarrV3Group::Create()                           */
/************************************************************************/

std::shared_ptr<ZarrV3Group>
ZarrV3Group::Create(const std::shared_ptr<ZarrSharedResource> &poSharedResource,
                    const std::string &osParentName, const std::string &osName,
                    const std::string &osRootDirectoryName)
{
    auto poGroup = std::shared_ptr<ZarrV3Group>(new ZarrV3Group(
        poSharedResource, osParentName, osName, osRootDirectoryName));
    poGroup->SetSelf(poGroup);
    return poGroup;
}

/************************************************************************/
/*                             OpenZarrArray()                          */
/************************************************************************/

std::shared_ptr<ZarrArray> ZarrV3Group::OpenZarrArray(const std::string &osName,
                                                      CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
        return oIter->second;

    const std::string osSubDir =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    const std::string osZarrayFilename =
        CPLFormFilename(osSubDir.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osZarrayFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osZarrayFilename))
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        return LoadArray(osName, osZarrayFilename, oRoot);
    }

    return nullptr;
}

/************************************************************************/
/*                   ZarrV3Group::LoadAttributes()                      */
/************************************************************************/

void ZarrV3Group::LoadAttributes() const
{
    if (m_bAttributesLoaded)
        return;
    m_bAttributesLoaded = true;

    const std::string osFilename =
        CPLFormFilename(m_osDirectoryName.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osFilename))
            return;
        auto oRoot = oDoc.GetRoot();
        m_oAttrGroup.Init(oRoot["attributes"], m_bUpdatable);
    }
}

/************************************************************************/
/*                        ExploreDirectory()                            */
/************************************************************************/

void ZarrV3Group::ExploreDirectory() const
{
    if (m_bDirectoryExplored)
        return;
    m_bDirectoryExplored = true;

    auto psDir = VSIOpenDir(m_osDirectoryName.c_str(), 0, nullptr);
    if (!psDir)
        return;
    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
    {
        if (VSI_ISDIR(psEntry->nMode))
        {
            const std::string osSubDir = CPLFormFilename(
                m_osDirectoryName.c_str(), psEntry->pszName, nullptr);
            VSIStatBufL sStat;
            std::string osZarrJsonFilename =
                CPLFormFilename(osSubDir.c_str(), "zarr.json", nullptr);
            if (VSIStatL(osZarrJsonFilename.c_str(), &sStat) == 0)
            {
                CPLJSONDocument oDoc;
                if (oDoc.Load(osZarrJsonFilename.c_str()))
                {
                    const auto oRoot = oDoc.GetRoot();
                    if (oRoot.GetInteger("zarr_format") != 3)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled zarr_format value");
                        continue;
                    }
                    const std::string osNodeType = oRoot.GetString("node_type");
                    if (osNodeType == "array")
                    {
                        if (std::find(m_aosArrays.begin(), m_aosArrays.end(),
                                      psEntry->pszName) == m_aosArrays.end())
                        {
                            m_aosArrays.emplace_back(psEntry->pszName);
                        }
                    }
                    else if (osNodeType == "group")
                    {
                        if (std::find(m_aosGroups.begin(), m_aosGroups.end(),
                                      psEntry->pszName) == m_aosGroups.end())
                        {
                            m_aosGroups.emplace_back(psEntry->pszName);
                        }
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled node_type value");
                        continue;
                    }
                }
            }
            else
            {
                // Implicit group
                if (std::find(m_aosGroups.begin(), m_aosGroups.end(),
                              psEntry->pszName) == m_aosGroups.end())
                {
                    m_aosGroups.emplace_back(psEntry->pszName);
                }
            }
        }
    }
    VSICloseDir(psDir);
}

/************************************************************************/
/*                      ZarrV3Group::ZarrV3Group()                      */
/************************************************************************/

ZarrV3Group::ZarrV3Group(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::string &osDirectoryName)
    : ZarrGroupBase(poSharedResource, osParentName, osName)
{
    m_osDirectoryName = osDirectoryName;
}

/************************************************************************/
/*                      ZarrV3Group::~ZarrV3Group()                     */
/************************************************************************/

ZarrV3Group::~ZarrV3Group()
{
    if (m_bValid && m_oAttrGroup.IsModified())
    {
        CPLJSONDocument oDoc;
        auto oRoot = oDoc.GetRoot();
        oRoot.Add("zarr_format", 3);
        oRoot.Add("node_type", "group");
        oRoot.Add("attributes", m_oAttrGroup.Serialize());
        const std::string osZarrJsonFilename =
            CPLFormFilename(m_osDirectoryName.c_str(), "zarr.json", nullptr);
        oDoc.Save(osZarrJsonFilename);
    }
}

/************************************************************************/
/*                            OpenZarrGroup()                           */
/************************************************************************/

std::shared_ptr<ZarrGroupBase>
ZarrV3Group::OpenZarrGroup(const std::string &osName, CSLConstList) const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
        return oIter->second;

    const std::string osSubDir =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    const std::string osSubDirZarrJsonFilename =
        CPLFormFilename(osSubDir.c_str(), "zarr.json", nullptr);

    VSIStatBufL sStat;
    // Explicit group
    if (VSIStatL(osSubDirZarrJsonFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (oDoc.Load(osSubDirZarrJsonFilename.c_str()))
        {
            const auto oRoot = oDoc.GetRoot();
            if (oRoot.GetInteger("zarr_format") != 3)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unhandled zarr_format value");
                return nullptr;
            }
            const std::string osNodeType = oRoot.GetString("node_type");
            if (osNodeType != "group")
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s is a %s, not a group",
                         osName.c_str(), osNodeType.c_str());
                return nullptr;
            }
            auto poSubGroup = ZarrV3Group::Create(
                m_poSharedResource, GetFullName(), osName, osSubDir);
            poSubGroup->m_poParent =
                std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
            poSubGroup->SetUpdatable(m_bUpdatable);
            m_oMapGroups[osName] = poSubGroup;
            return poSubGroup;
        }
        return nullptr;
    }

    // Implicit group
    if (VSIStatL(osSubDir.c_str(), &sStat) == 0 && VSI_ISDIR(sStat.st_mode))
    {
        auto poSubGroup = ZarrV3Group::Create(m_poSharedResource, GetFullName(),
                                              osName, osSubDir);
        poSubGroup->m_poParent =
            std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    return nullptr;
}

/************************************************************************/
/*                   ZarrV3Group::CreateOnDisk()                        */
/************************************************************************/

std::shared_ptr<ZarrV3Group> ZarrV3Group::CreateOnDisk(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentFullName, const std::string &osName,
    const std::string &osDirectoryName)
{
    if (VSIMkdir(osDirectoryName.c_str(), 0755) != 0)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osDirectoryName.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                     osDirectoryName.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osDirectoryName.c_str());
        }
        return nullptr;
    }

    const std::string osZarrJsonFilename(
        CPLFormFilename(osDirectoryName.c_str(), "zarr.json", nullptr));
    VSILFILE *fp = VSIFOpenL(osZarrJsonFilename.c_str(), "wb");
    if (!fp)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                 osZarrJsonFilename.c_str());
        return nullptr;
    }
    VSIFPrintfL(fp, "{\n"
                    "    \"zarr_format\": 3,\n"
                    "    \"node_type\": \"group\",\n"
                    "    \"attributes\": {}\n"
                    "}\n");
    VSIFCloseL(fp);

    auto poGroup = ZarrV3Group::Create(poSharedResource, osParentFullName,
                                       osName, osDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;
    return poGroup;
}

/************************************************************************/
/*                      ZarrV3Group::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup>
ZarrV3Group::CreateGroup(const std::string &osName,
                         CSLConstList /* papszOptions */)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (!IsValidObjectName(osName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid group name");
        return nullptr;
    }

    GetGroupNames();

    if (std::find(m_aosGroups.begin(), m_aosGroups.end(), osName) !=
        m_aosGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }

    const std::string osDirectoryName =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    auto poGroup = CreateOnDisk(m_poSharedResource, GetFullName(), osName,
                                osDirectoryName);
    if (!poGroup)
        return nullptr;
    poGroup->m_poParent =
        std::dynamic_pointer_cast<ZarrGroupBase>(m_pSelf.lock());
    m_oMapGroups[osName] = poGroup;
    m_aosGroups.emplace_back(osName);
    return poGroup;
}

/************************************************************************/
/*                          FillDTypeElts()                             */
/************************************************************************/

static CPLJSONObject FillDTypeElts(const GDALExtendedDataType &oDataType,
                                   std::vector<DtypeElt> &aoDtypeElts)
{
    CPLJSONObject dtype;
    const std::string dummy("dummy");

    const auto eDT = oDataType.GetNumericDataType();
    DtypeElt elt;
    bool bUnsupported = false;
    switch (eDT)
    {
        case GDT_Byte:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint8");
            break;
        }
        case GDT_Int8:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int8");
            break;
        }
        case GDT_UInt16:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint16");
            break;
        }
        case GDT_Int16:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int16");
            break;
        }
        case GDT_UInt32:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint32");
            break;
        }
        case GDT_Int32:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int32");
            break;
        }
        case GDT_UInt64:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "uint64");
            break;
        }
        case GDT_Int64:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "int64");
            break;
        }
        case GDT_Float32:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "float32");
            break;
        }
        case GDT_Float64:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "float64");
            break;
        }
        case GDT_Unknown:
        case GDT_CInt16:
        case GDT_CInt32:
        {
            bUnsupported = true;
            break;
        }
        case GDT_CFloat32:
        {
            elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
            dtype.Set(dummy, "complex64");
            break;
        }
        case GDT_CFloat64:
        {
            elt.nativeType = DtypeElt::NativeType::COMPLEX_IEEEFP;
            dtype.Set(dummy, "complex128");
            break;
        }
        case GDT_TypeCount:
        {
            static_assert(GDT_TypeCount == GDT_Int8 + 1,
                          "GDT_TypeCount == GDT_Int8 + 1");
            break;
        }
    }
    if (bUnsupported)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type: %s",
                 GDALGetDataTypeName(eDT));
        dtype = CPLJSONObject();
        dtype.Deinit();
        return dtype;
    }
    elt.nativeOffset = 0;
    elt.nativeSize = GDALGetDataTypeSizeBytes(eDT);
    elt.gdalOffset = 0;
    elt.gdalSize = elt.nativeSize;
#ifdef CPL_MSB
    elt.needByteSwapping = elt.nativeSize > 1;
#endif
    aoDtypeElts.emplace_back(elt);

    return dtype;
}

/************************************************************************/
/*                     ZarrV3Group::CreateMDArray()                     */
/************************************************************************/

std::shared_ptr<GDALMDArray> ZarrV3Group::CreateMDArray(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (!IsValidObjectName(osName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid array name");
        return nullptr;
    }

    if (oDataType.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported data type with Zarr V3");
        return nullptr;
    }

    if (!EQUAL(CSLFetchNameValueDef(papszOptions, "FILTER", "NONE"), "NONE"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FILTER option not supported with Zarr V3");
        return nullptr;
    }

    std::vector<DtypeElt> aoDtypeElts;
    const auto dtype = FillDTypeElts(oDataType, aoDtypeElts)["dummy"];
    if (!dtype.IsValid() || aoDtypeElts.empty())
        return nullptr;

    GetMDArrayNames();

    if (std::find(m_aosArrays.begin(), m_aosArrays.end(), osName) !=
        m_aosArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::FillBlockSize(aoDimensions, oDataType, anBlockSize,
                                  papszOptions))
        return nullptr;

    const char *pszDimSeparator =
        CSLFetchNameValueDef(papszOptions, "DIM_SEPARATOR", "/");

    const std::string osArrayDirectory =
        CPLFormFilename(m_osDirectoryName.c_str(), osName.c_str(), nullptr);
    if (VSIMkdir(osArrayDirectory.c_str(), 0755) != 0)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osArrayDirectory.c_str(), &sStat) == 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Directory %s already exists.",
                     osArrayDirectory.c_str());
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osArrayDirectory.c_str());
        }
        return nullptr;
    }

    std::unique_ptr<ZarrV3CodecSequence> poCodecs;
    CPLJSONArray oCodecs;

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");
    if (bFortranOrder)
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "transpose");
        oCodec.Add("configuration",
                   ZarrV3CodecTranspose::GetConfiguration("F"));
        oCodecs.Add(oCodec);
    }

    // Not documented
    const char *pszEndian = CSLFetchNameValue(papszOptions, "@ENDIAN");
    if (pszEndian)
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "endian");
        oCodec.Add("configuration", ZarrV3CodecEndian::GetConfiguration(
                                        EQUAL(pszEndian, "little")));
        oCodecs.Add(oCodec);
    }

    const char *pszCompressor =
        CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    if (EQUAL(pszCompressor, "GZIP"))
    {
        CPLJSONObject oCodec;
        oCodec.Add("name", "gzip");
        const char *pszLevel =
            CSLFetchNameValueDef(papszOptions, "GZIP_LEVEL", "6");
        oCodec.Add("configuration",
                   ZarrV3CodecGZip::GetConfiguration(atoi(pszLevel)));
        oCodecs.Add(oCodec);
    }
    else if (EQUAL(pszCompressor, "BLOSC"))
    {
        const auto psCompressor = CPLGetCompressor("blosc");
        if (!psCompressor)
            return nullptr;
        const char *pszOptions =
            CSLFetchNameValueDef(psCompressor->papszMetadata, "OPTIONS", "");
        CPLXMLTreeCloser oTreeCompressor(CPLParseXMLString(pszOptions));
        const auto psRoot =
            oTreeCompressor.get()
                ? CPLGetXMLNode(oTreeCompressor.get(), "=Options")
                : nullptr;
        if (!psRoot)
            return nullptr;

        const char *cname = "zlib";
        for (const CPLXMLNode *psNode = psRoot->psChild; psNode != nullptr;
             psNode = psNode->psNext)
        {
            if (psNode->eType == CXT_Element)
            {
                const char *pszName = CPLGetXMLValue(psNode, "name", "");
                if (EQUAL(pszName, "CNAME"))
                {
                    cname = CPLGetXMLValue(psNode, "default", cname);
                }
            }
        }

        CPLJSONObject oCodec;
        oCodec.Add("name", "blosc");
        cname = CSLFetchNameValueDef(papszOptions, "BLOSC_CNAME", cname);
        const int clevel =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOSC_CLEVEL", "5"));
        const char *shuffle =
            CSLFetchNameValueDef(papszOptions, "BLOSC_SHUFFLE", "BYTE");
        shuffle = (EQUAL(shuffle, "0") || EQUAL(shuffle, "NONE")) ? "noshuffle"
                  : (EQUAL(shuffle, "1") || EQUAL(shuffle, "BYTE")) ? "shuffle"
                  : (EQUAL(shuffle, "2") || EQUAL(shuffle, "BIT"))
                      ? "bitshuffle"
                      : "invalid";
        const int typesize = atoi(CSLFetchNameValueDef(
            papszOptions, "BLOSC_TYPESIZE",
            CPLSPrintf("%d", GDALGetDataTypeSizeBytes(GDALGetNonComplexDataType(
                                 oDataType.GetNumericDataType())))));
        const int blocksize =
            atoi(CSLFetchNameValueDef(papszOptions, "BLOSC_BLOCKSIZE", "0"));
        oCodec.Add("configuration",
                   ZarrV3CodecBlosc::GetConfiguration(cname, clevel, shuffle,
                                                      typesize, blocksize));
        oCodecs.Add(oCodec);
    }
    else if (!EQUAL(pszCompressor, "NONE"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "COMPRESS = %s not implemented with Zarr V3", pszCompressor);
        return nullptr;
    }

    if (oCodecs.Size() > 0)
    {
        // Byte swapping will be done by the codec chain
        aoDtypeElts.back().needByteSwapping = false;

        ZarrArrayMetadata oInputArrayMetadata;
        for (auto &nSize : anBlockSize)
            oInputArrayMetadata.anBlockSizes.push_back(
                static_cast<size_t>(nSize));
        oInputArrayMetadata.oElt = aoDtypeElts.back();
        poCodecs = std::make_unique<ZarrV3CodecSequence>(oInputArrayMetadata);
        if (!poCodecs->InitFromJson(oCodecs))
            return nullptr;
    }

    auto poArray =
        ZarrV3Array::Create(m_poSharedResource, GetFullName(), osName,
                            aoDimensions, oDataType, aoDtypeElts, anBlockSize);

    if (!poArray)
        return nullptr;
    poArray->SetNew(true);
    std::string osFilename =
        CPLFormFilename(osArrayDirectory.c_str(), "zarr.json", nullptr);
    poArray->SetFilename(osFilename);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetDtype(dtype);
    if (poCodecs)
        poArray->SetCodecs(std::move(poCodecs));
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    poArray->Flush();
    RegisterArray(poArray);

    return poArray;
}
