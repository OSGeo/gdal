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
    auto oIter = m_oMapMDArrays.find(osName);
    if (oIter != m_oMapMDArrays.end())
        return oIter->second;

    std::string osFilenamePrefix = m_osDirectoryName + "/meta/root";
    if (!(GetFullName() == "/" && osName == "/"))
    {
        osFilenamePrefix += GetFullName();
        if (GetFullName() != "/")
            osFilenamePrefix += '/';
        osFilenamePrefix += osName;
    }

    std::string osFilename(osFilenamePrefix);
    osFilename += ".array.json";

    VSIStatBufL sStat;
    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
    {
        CPLJSONDocument oDoc;
        if (!oDoc.Load(osFilename))
            return nullptr;
        const auto oRoot = oDoc.GetRoot();
        std::set<std::string> oSetFilenamesInLoading;
        return LoadArray(osName, osFilename, oRoot, oSetFilenamesInLoading);
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

    std::string osFilename = m_osDirectoryName + "/meta/root";
    if (GetFullName() != "/")
        osFilename += GetFullName();
    osFilename += ".group.json";

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

    const std::string osDirname =
        m_osDirectoryName + "/meta/root" + GetFullName();

    if (GetFullName() == "/")
    {
        VSIStatBufL sStat;
        if (VSIStatL((m_osDirectoryName + "/meta/root.array.json").c_str(),
                     &sStat) == 0)
        {
            m_aosArrays.emplace_back("/");
        }
    }

    const CPLStringList aosFiles(VSIReadDir(osDirname.c_str()));
    std::set<std::string> oSetGroups;
    for (int i = 0; i < aosFiles.size(); ++i)
    {
        const std::string osFilename(aosFiles[i]);
        if (osFilename.size() > strlen(".group.json") &&
            osFilename.substr(osFilename.size() - strlen(".group.json")) ==
                ".group.json")
        {
            const auto osGroupName =
                osFilename.substr(0, osFilename.size() - strlen(".group.json"));
            if (oSetGroups.find(osGroupName) == oSetGroups.end())
            {
                oSetGroups.insert(osGroupName);
                m_aosGroups.emplace_back(osGroupName);
            }
        }
        else if (osFilename.size() > strlen(".array.json") &&
                 osFilename.substr(osFilename.size() - strlen(".array.json")) ==
                     ".array.json")
        {
            const auto osArrayName =
                osFilename.substr(0, osFilename.size() - strlen(".array.json"));
            m_aosArrays.emplace_back(osArrayName);
        }
        else if (osFilename != "." && osFilename != "..")
        {
            VSIStatBufL sStat;
            if (VSIStatL(CPLFormFilename(osDirname.c_str(), osFilename.c_str(),
                                         nullptr),
                         &sStat) == 0 &&
                VSI_ISDIR(sStat.st_mode))
            {
                const auto &osGroupName = osFilename;
                if (oSetGroups.find(osGroupName) == oSetGroups.end())
                {
                    oSetGroups.insert(osGroupName);
                    m_aosGroups.emplace_back(osGroupName);
                }
            }
        }
    }
}

/************************************************************************/
/*                      ZarrV3GroupGetFilename()                        */
/************************************************************************/

static std::string
ZarrV3GroupGetFilename(const std::string &osParentFullName,
                       const std::string &osName,
                       const std::string &osRootDirectoryName)
{
    const std::string osMetaDir(
        CPLFormFilename(osRootDirectoryName.c_str(), "meta", nullptr));

    std::string osGroupFilename(osMetaDir);
    if (osName == "/")
    {
        osGroupFilename += "/root.group.json";
    }
    else
    {
        osGroupFilename += "/root";
        osGroupFilename +=
            (osParentFullName == "/" ? std::string() : osParentFullName);
        osGroupFilename += '/';
        osGroupFilename += osName;
        osGroupFilename += ".group.json";
    }
    return osGroupFilename;
}

/************************************************************************/
/*                      ZarrV3Group::ZarrV3Group()                      */
/************************************************************************/

ZarrV3Group::ZarrV3Group(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::string &osRootDirectoryName)
    : ZarrGroupBase(poSharedResource, osParentName, osName),
      m_osGroupFilename(
          ZarrV3GroupGetFilename(osParentName, osName, osRootDirectoryName))
{
    m_osDirectoryName = osRootDirectoryName;
}

/************************************************************************/
/*                      ZarrV3Group::~ZarrV3Group()                     */
/************************************************************************/

ZarrV3Group::~ZarrV3Group()
{
    if (m_bNew || m_oAttrGroup.IsModified())
    {
        CPLJSONDocument oDoc;
        auto oRoot = oDoc.GetRoot();
        oRoot.Add("extensions", CPLJSONArray());
        oRoot.Add("attributes", m_oAttrGroup.Serialize());
        oDoc.Save(m_osGroupFilename);
    }
}

/************************************************************************/
/*                            OpenZarrGroup()                           */
/************************************************************************/

std::shared_ptr<ZarrGroupBase>
ZarrV3Group::OpenZarrGroup(const std::string &osName, CSLConstList) const
{
    auto oIter = m_oMapGroups.find(osName);
    if (oIter != m_oMapGroups.end())
        return oIter->second;

    std::string osFilenamePrefix =
        m_osDirectoryName + "/meta/root" + GetFullName();
    if (GetFullName() != "/")
        osFilenamePrefix += '/';
    osFilenamePrefix += osName;

    std::string osFilename(osFilenamePrefix);
    osFilename += ".group.json";

    VSIStatBufL sStat;
    // Explicit group
    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
    {
        auto poSubGroup = ZarrV3Group::Create(m_poSharedResource, GetFullName(),
                                              osName, m_osDirectoryName);
        poSubGroup->m_poParent = m_pSelf;
        poSubGroup->SetUpdatable(m_bUpdatable);
        m_oMapGroups[osName] = poSubGroup;
        return poSubGroup;
    }

    // Implicit group
    if (VSIStatL(osFilenamePrefix.c_str(), &sStat) == 0 &&
        VSI_ISDIR(sStat.st_mode))
    {
        auto poSubGroup = ZarrV3Group::Create(m_poSharedResource, GetFullName(),
                                              osName, m_osDirectoryName);
        poSubGroup->m_poParent = m_pSelf;
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
    const std::string &osRootDirectoryName)
{
    const std::string osMetaDir(
        CPLFormFilename(osRootDirectoryName.c_str(), "meta", nullptr));
    std::string osGroupDir(osMetaDir);
    osGroupDir += "/root";

    if (osParentFullName.empty())
    {
        if (VSIMkdir(osRootDirectoryName.c_str(), 0755) != 0)
        {
            VSIStatBufL sStat;
            if (VSIStatL(osRootDirectoryName.c_str(), &sStat) == 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Directory %s already exists.",
                         osRootDirectoryName.c_str());
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                         osRootDirectoryName.c_str());
            }
            return nullptr;
        }

        const std::string osZarrJsonFilename(
            CPLFormFilename(osRootDirectoryName.c_str(), "zarr.json", nullptr));
        VSILFILE *fp = VSIFOpenL(osZarrJsonFilename.c_str(), "wb");
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s.",
                     osZarrJsonFilename.c_str());
            return nullptr;
        }
        VSIFPrintfL(fp, "{\n"
                        "    \"zarr_format\": "
                        "\"https://purl.org/zarr/spec/protocol/core/3.0\",\n"
                        "    \"metadata_encoding\": "
                        "\"https://purl.org/zarr/spec/protocol/core/3.0\",\n"
                        "    \"metadata_key_suffix\": \".json\",\n"
                        "    \"extensions\": []\n"
                        "}\n");
        VSIFCloseL(fp);

        if (VSIMkdir(osMetaDir.c_str(), 0755) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                     osMetaDir.c_str());
            return nullptr;
        }
    }
    else
    {
        osGroupDir +=
            (osParentFullName == "/" ? std::string() : osParentFullName);
        osGroupDir += '/';
        osGroupDir += osName;
    }

    if (VSIMkdir(osGroupDir.c_str(), 0755) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s.",
                 osGroupDir.c_str());
        return nullptr;
    }

    auto poGroup = ZarrV3Group::Create(poSharedResource, osParentFullName,
                                       osName, osRootDirectoryName);
    poGroup->SetUpdatable(true);
    poGroup->m_bDirectoryExplored = true;
    poGroup->m_bNew = true;
    return poGroup;
}

/************************************************************************/
/*                      ZarrV3Group::CreateGroup()                      */
/************************************************************************/

std::shared_ptr<GDALGroup>
ZarrV3Group::CreateGroup(const std::string &osName,
                         CSLConstList /* papszOptions */)
{
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

    if (m_oMapGroups.find(osName) != m_oMapGroups.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }

    auto poGroup = CreateOnDisk(m_poSharedResource, GetFullName(), osName,
                                m_osDirectoryName);
    if (!poGroup)
        return nullptr;
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
            dtype.Set(dummy, "u1");
            break;
        }
        case GDT_Int8:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "i1");
            break;
        }
        case GDT_UInt16:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "<u2");
            break;
        }
        case GDT_Int16:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "<i2");
            break;
        }
        case GDT_UInt32:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "<u4");
            break;
        }
        case GDT_Int32:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "<i4");
            break;
        }
        case GDT_UInt64:
        {
            elt.nativeType = DtypeElt::NativeType::UNSIGNED_INT;
            dtype.Set(dummy, "<u8");
            break;
        }
        case GDT_Int64:
        {
            elt.nativeType = DtypeElt::NativeType::SIGNED_INT;
            dtype.Set(dummy, "<i8");
            break;
        }
        case GDT_Float32:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "<f4");
            break;
        }
        case GDT_Float64:
        {
            elt.nativeType = DtypeElt::NativeType::IEEEFP;
            dtype.Set(dummy, "<f8");
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
        case GDT_CFloat64:
        {
            bUnsupported = true;
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

    if (m_oMapMDArrays.find(osName) != m_oMapMDArrays.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }

    CPLJSONObject oCompressor;
    oCompressor.Deinit();
    const char *pszCompressor =
        CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    const CPLCompressor *psCompressor = nullptr;
    const CPLCompressor *psDecompressor = nullptr;
    if (!EQUAL(pszCompressor, "NONE"))
    {
        psCompressor = CPLGetCompressor(pszCompressor);
        psDecompressor = CPLGetCompressor(pszCompressor);
        if (psCompressor == nullptr || psDecompressor == nullptr)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Compressor/decompressor for %s not available",
                     pszCompressor);
            return nullptr;
        }
        const char *pszOptions =
            CSLFetchNameValue(psCompressor->papszMetadata, "OPTIONS");
        if (pszOptions)
        {
            CPLXMLTreeCloser oTree(CPLParseXMLString(pszOptions));
            const auto psRoot =
                oTree.get() ? CPLGetXMLNode(oTree.get(), "=Options") : nullptr;
            if (psRoot)
            {
                CPLJSONObject configuration;
                for (const CPLXMLNode *psNode = psRoot->psChild;
                     psNode != nullptr; psNode = psNode->psNext)
                {
                    if (psNode->eType == CXT_Element &&
                        strcmp(psNode->pszValue, "Option") == 0)
                    {
                        const char *pszName =
                            CPLGetXMLValue(psNode, "name", nullptr);
                        const char *pszType =
                            CPLGetXMLValue(psNode, "type", nullptr);
                        if (pszName && pszType)
                        {
                            const char *pszVal = CSLFetchNameValueDef(
                                papszOptions,
                                (std::string(pszCompressor) + '_' + pszName)
                                    .c_str(),
                                CPLGetXMLValue(psNode, "default", nullptr));
                            if (pszVal)
                            {
                                if (EQUAL(pszName, "SHUFFLE") &&
                                    EQUAL(pszVal, "BYTE"))
                                {
                                    pszVal = "1";
                                    pszType = "integer";
                                }

                                if (!oCompressor.IsValid())
                                {
                                    oCompressor = CPLJSONObject();
                                    oCompressor.Add(
                                        "codec",
                                        "https://purl.org/zarr/spec/codec/" +
                                            CPLString(pszCompressor).tolower() +
                                            "/1.0");
                                    oCompressor.Add("configuration",
                                                    configuration);
                                }

                                std::string osOptName(
                                    CPLString(pszName).tolower());
                                if (STARTS_WITH(pszType, "int"))
                                    configuration.Add(osOptName, atoi(pszVal));
                                else
                                    configuration.Add(osOptName, pszVal);
                            }
                        }
                    }
                }
            }
        }
    }

    std::string osFilenamePrefix = m_osDirectoryName + "/meta/root";
    if (!(GetFullName() == "/" && osName == "/"))
    {
        osFilenamePrefix += GetFullName();
        if (GetFullName() != "/")
            osFilenamePrefix += '/';
        osFilenamePrefix += osName;
    }

    std::string osFilename(osFilenamePrefix);
    osFilename += ".array.json";

    std::vector<GUInt64> anBlockSize;
    if (!ZarrArray::FillBlockSize(aoDimensions, oDataType, anBlockSize,
                                  papszOptions))
        return nullptr;

    const bool bFortranOrder = EQUAL(
        CSLFetchNameValueDef(papszOptions, "CHUNK_MEMORY_LAYOUT", "C"), "F");

    const char *pszDimSeparator =
        CSLFetchNameValueDef(papszOptions, "DIM_SEPARATOR", "/");

    auto poArray = ZarrV3Array::Create(m_poSharedResource, GetFullName(),
                                       osName, aoDimensions, oDataType,
                                       aoDtypeElts, anBlockSize, bFortranOrder);

    if (!poArray)
        return nullptr;
    poArray->SetNew(true);
    poArray->SetFilename(osFilename);
    poArray->SetRootDirectoryName(m_osDirectoryName);
    poArray->SetDimSeparator(pszDimSeparator);
    poArray->SetDtype(dtype);
    poArray->SetCompressorDecompressor(pszCompressor, psCompressor,
                                       psDecompressor);
    if (oCompressor.IsValid())
        poArray->SetCompressorJson(oCompressor);
    poArray->SetUpdatable(true);
    poArray->SetDefinitionModified(true);
    poArray->Flush();
    RegisterArray(poArray);

    return poArray;
}
