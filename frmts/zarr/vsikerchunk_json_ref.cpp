/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver. Virtual file system for
 *           https://fsspec.github.io/kerchunk/spec.html#version-1
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#undef _REENTRANT

#include "vsikerchunk.h"
#include "vsikerchunk_inline.hpp"

#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_json_streaming_parser.h"
#include "cpl_json_streaming_writer.h"
#include "cpl_mem_cache.h"
#include "cpl_vsi_error.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cerrno>
#include <cinttypes>
#include <limits>
#include <mutex>
#include <set>
#include <utility>

/************************************************************************/
/*                         VSIKerchunkKeyInfo                           */
/************************************************************************/

struct VSIKerchunkKeyInfo
{
    // points to an element in VSIKerchunkRefFile::m_oSetURI
    const std::string *posURI = nullptr;

    uint64_t nOffset = 0;
    uint32_t nSize = 0;
    std::vector<GByte> abyValue{};
};

/************************************************************************/
/*                         VSIKerchunkRefFile                           */
/************************************************************************/

class VSIKerchunkRefFile
{
  private:
    std::set<std::string> m_oSetURI{};
    std::map<std::string, VSIKerchunkKeyInfo> m_oMapKeys{};

  public:
    const std::map<std::string, VSIKerchunkKeyInfo> &GetMapKeys() const
    {
        return m_oMapKeys;
    }

    void AddInlineContent(const std::string &key, std::vector<GByte> &&abyValue)
    {
        VSIKerchunkKeyInfo info;
        info.abyValue = std::move(abyValue);
        m_oMapKeys[key] = std::move(info);
    }

    bool AddInlineContent(const std::string &key, const std::string_view &str)
    {
        std::vector<GByte> abyValue;
        if (cpl::starts_with(str, "base64:"))
        {
            abyValue.insert(
                abyValue.end(),
                reinterpret_cast<const GByte *>(str.data()) + strlen("base64:"),
                reinterpret_cast<const GByte *>(str.data()) + str.size());
            abyValue.push_back(0);
            const int nSize = CPLBase64DecodeInPlace(abyValue.data());
            if (nSize == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VSIKerchunkJSONRefFileSystem: Base64 decoding "
                         "failed for key '%s'",
                         key.c_str());
                return false;
            }
            abyValue.resize(nSize);
        }
        else
        {
            abyValue.insert(
                abyValue.end(), reinterpret_cast<const GByte *>(str.data()),
                reinterpret_cast<const GByte *>(str.data()) + str.size());
        }

        AddInlineContent(key, std::move(abyValue));
        return true;
    }

    void AddReferencedContent(const std::string &key, const std::string &osURI,
                              uint64_t nOffset, uint32_t nSize)
    {
        auto oPair = m_oSetURI.insert(osURI);

        VSIKerchunkKeyInfo info;
        info.posURI = &(*(oPair.first));
        info.nOffset = nOffset;
        info.nSize = nSize;
        m_oMapKeys[key] = std::move(info);
    }

    bool ConvertToParquetRef(const std::string &osCacheDir,
                             GDALProgressFunc pfnProgress, void *pProgressData);
};

/************************************************************************/
/*                    VSIKerchunkJSONRefFileSystem                      */
/************************************************************************/

class VSIKerchunkJSONRefFileSystem final : public VSIFilesystemHandler
{
  public:
    VSIKerchunkJSONRefFileSystem()
    {
        IsFileSystemInstantiated() = true;
    }

    ~VSIKerchunkJSONRefFileSystem()
    {
        IsFileSystemInstantiated() = false;
    }

    static bool &IsFileSystemInstantiated()
    {
        static bool bIsFileSystemInstantiated = false;
        return bIsFileSystemInstantiated;
    }

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError, CSLConstList papszOptions) override;

    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;

  private:
    friend bool VSIKerchunkConvertJSONToParquet(const char *pszSrcJSONFilename,
                                                const char *pszDstDirname,
                                                GDALProgressFunc pfnProgress,
                                                void *pProgressData);

    lru11::Cache<std::string, std::shared_ptr<VSIKerchunkRefFile>, std::mutex>
        m_oCache{};

    static std::pair<std::string, std::string>
    SplitFilename(const char *pszFilename);

    std::pair<std::shared_ptr<VSIKerchunkRefFile>, std::string>
    Load(const std::string &osJSONFilename, bool bUseCache);
    std::shared_ptr<VSIKerchunkRefFile>
    LoadInternal(const std::string &osJSONFilename,
                 GDALProgressFunc pfnProgress, void *pProgressData);
    std::shared_ptr<VSIKerchunkRefFile>
    LoadStreaming(const std::string &osJSONFilename,
                  GDALProgressFunc pfnProgress, void *pProgressData);
};

/************************************************************************/
/*            VSIKerchunkJSONRefFileSystem::SplitFilename()             */
/************************************************************************/

/*static*/
std::pair<std::string, std::string>
VSIKerchunkJSONRefFileSystem::SplitFilename(const char *pszFilename)
{
    if (STARTS_WITH(pszFilename, JSON_REF_FS_PREFIX))
        pszFilename += strlen(JSON_REF_FS_PREFIX);
    else if (STARTS_WITH(pszFilename, JSON_REF_CACHED_FS_PREFIX))
        pszFilename += strlen(JSON_REF_CACHED_FS_PREFIX);
    else
        return {std::string(), std::string()};

    std::string osJSONFilename;

    if (*pszFilename == '{')
    {
        // Parse /vsikerchunk_json_ref/{/path/to/some.json}[key]
        int nLevel = 1;
        ++pszFilename;
        for (; *pszFilename; ++pszFilename)
        {
            if (*pszFilename == '{')
            {
                ++nLevel;
            }
            else if (*pszFilename == '}')
            {
                --nLevel;
                if (nLevel == 0)
                {
                    ++pszFilename;
                    break;
                }
            }
            osJSONFilename += *pszFilename;
        }
        if (nLevel != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid %s syntax: should be "
                     "%s{/path/to/some/file}[/optional_key]",
                     JSON_REF_FS_PREFIX, JSON_REF_FS_PREFIX);
            return {std::string(), std::string()};
        }

        return {osJSONFilename,
                *pszFilename == '/' ? pszFilename + 1 : pszFilename};
    }
    else
    {
        int nCountDotJson = 0;
        const char *pszIter = pszFilename;
        const char *pszAfterJSON = nullptr;
        while ((pszIter = strstr(pszIter, ".json")) != nullptr)
        {
            ++nCountDotJson;
            if (nCountDotJson == 1)
                pszAfterJSON = pszIter + strlen(".json");
            else
                pszAfterJSON = nullptr;
            pszIter += strlen(".json");
        }
        if (!pszAfterJSON)
        {
            if (nCountDotJson >= 2)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Ambiguous %s syntax: should be "
                         "%s{/path/to/some/file}[/optional_key]",
                         JSON_REF_FS_PREFIX, JSON_REF_FS_PREFIX);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid %s syntax: should be "
                         "%s/path/to/some.json[/optional_key] or "
                         "%s{/path/to/some/file}[/optional_key]",
                         JSON_REF_FS_PREFIX, JSON_REF_FS_PREFIX,
                         JSON_REF_FS_PREFIX);
            }
            return {std::string(), std::string()};
        }
        return {std::string(pszFilename, pszAfterJSON - pszFilename),
                *pszAfterJSON == '/' ? pszAfterJSON + 1 : pszAfterJSON};
    }
}

/************************************************************************/
/*                  class VSIKerchunkJSONRefParser                      */
/************************************************************************/

namespace
{
class VSIKerchunkJSONRefParser final : public CPLJSonStreamingParser
{
  public:
    explicit VSIKerchunkJSONRefParser(
        const std::shared_ptr<VSIKerchunkRefFile> &refFile)
        : m_refFile(refFile)
    {
        m_oWriter.SetPrettyFormatting(false);
    }

    ~VSIKerchunkJSONRefParser()
    {
        // In case the parsing would be stopped, the writer may be in
        // an inconsistent state. This avoids assertion in debug mode.
        m_oWriter.clear();
    }

  protected:
    void String(const char *pszValue, size_t nLength) override
    {
        if (m_nLevel == m_nKeyLevel && m_nArrayLevel == 0)
        {
            if (nLength > 0 && pszValue[nLength - 1] == 0)
                --nLength;

            if (!m_refFile->AddInlineContent(
                    m_osCurKey, std::string_view(pszValue, nLength)))
            {
                StopParsing();
            }

            m_oWriter.clear();

            m_osCurKey.clear();
        }
        else if (m_nLevel == m_nKeyLevel && m_nArrayLevel == 1)
        {
            if (m_iArrayMemberIdx == 0)
            {
                m_osURI.assign(pszValue, nLength);
            }
            else
            {
                UnexpectedContentInArray();
            }
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.Add(std::string_view(pszValue, nLength));
        }
    }

    void Number(const char *pszValue, size_t nLength) override
    {
        if (m_nLevel == m_nKeyLevel)
        {
            if (m_nArrayLevel == 1)
            {
                if (m_iArrayMemberIdx == 1)
                {
                    m_osTmpForNumber.assign(pszValue, nLength);
                    errno = 0;
                    m_nOffset =
                        std::strtoull(m_osTmpForNumber.c_str(), nullptr, 10);
                    if (errno != 0 || m_osTmpForNumber[0] == '-' ||
                        m_osTmpForNumber.find('.') != std::string::npos)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "VSIKerchunkJSONRefFileSystem: array value at "
                            "index 1 for key '%s' is not an unsigned 64 bit "
                            "integer",
                            m_osCurKey.c_str());
                        StopParsing();
                    }
                }
                else if (m_iArrayMemberIdx == 2)
                {
                    m_osTmpForNumber.assign(pszValue, nLength);
                    errno = 0;
                    const uint64_t nSize =
                        std::strtoull(m_osTmpForNumber.c_str(), nullptr, 10);
                    if (errno != 0 || m_osTmpForNumber[0] == '-' ||
                        nSize > std::numeric_limits<uint32_t>::max() ||
                        m_osTmpForNumber.find('.') != std::string::npos)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "VSIKerchunkJSONRefFileSystem: array value at "
                            "index 2 for key '%s' is not an unsigned 32 bit "
                            "integer",
                            m_osCurKey.c_str());
                        StopParsing();
                    }
                    else
                    {
                        m_nSize = static_cast<uint32_t>(nSize);
                    }
                }
                else
                {
                    UnexpectedContentInArray();
                }
            }
            else
            {
                UnexpectedContent();
            }
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.AddSerializedValue(std::string_view(pszValue, nLength));
        }
    }

    void Boolean(bool b) override
    {
        if (m_nLevel == m_nKeyLevel)
        {
            UnexpectedContent();
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.Add(b);
        }
    }

    void Null() override
    {
        if (m_nLevel == m_nKeyLevel)
        {
            UnexpectedContent();
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.AddNull();
        }
    }

    void StartObject() override
    {
        if (m_nLevel == m_nKeyLevel && m_nArrayLevel == 1)
        {
            UnexpectedContentInArray();
        }
        else
        {
            if (m_nLevel >= m_nKeyLevel)
            {
                m_oWriter.StartObj();
            }
            ++m_nLevel;
            m_bFirstMember = true;
        }
    }

    void EndObject() override
    {
        if (m_nLevel == m_nKeyLevel)
        {
            FinishObjectValueProcessing();
        }
        --m_nLevel;
        if (m_nLevel >= m_nKeyLevel)
        {
            m_oWriter.EndObj();
        }
    }

    void StartObjectMember(const char *pszKey, size_t nLength) override
    {
        if (m_nLevel == 1 && m_bFirstMember)
        {
            if (nLength == strlen("version") &&
                memcmp(pszKey, "version", nLength) == 0)
            {
                m_nKeyLevel = 2;
            }
            else
            {
                m_nKeyLevel = 1;
            }
        }
        else if (m_nLevel == 1 && m_nKeyLevel == 2 &&
                 nLength == strlen("templates") &&
                 memcmp(pszKey, "templates", nLength) == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'templates' key found, but "
                     "not supported");
            StopParsing();
        }
        else if (m_nLevel == 1 && m_nKeyLevel == 2 &&
                 nLength == strlen("gen") &&
                 memcmp(pszKey, "gen", nLength) == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'gen' key found, but not "
                     "supported");
            StopParsing();
        }

        if (m_nLevel == m_nKeyLevel)
        {
            FinishObjectValueProcessing();
            m_osCurKey.assign(pszKey, nLength);
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.AddObjKey(std::string_view(pszKey, nLength));
        }
        m_bFirstMember = false;
    }

    void StartArray() override
    {
        if (m_nLevel == m_nKeyLevel)
        {
            if (m_nArrayLevel == 0)
            {
                m_iArrayMemberIdx = -1;
                m_osURI.clear();
                m_nOffset = 0;
                m_nSize = 0;
                m_nArrayLevel = 1;
            }
            else
            {
                UnexpectedContentInArray();
            }
        }
        else if (m_nLevel > m_nKeyLevel)
        {
            m_oWriter.StartArray();
            ++m_nArrayLevel;
        }
    }

    void EndArray() override
    {
        if (m_nLevel == m_nKeyLevel && m_nArrayLevel == 1)
        {
            if (m_iArrayMemberIdx == -1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VSIKerchunkJSONRefFileSystem: array value for key "
                         "'%s' is not of size 1 or 3",
                         m_osCurKey.c_str());
                StopParsing();
            }
            else
            {
                m_refFile->AddReferencedContent(m_osCurKey, m_osURI, m_nOffset,
                                                m_nSize);
                --m_nArrayLevel;
                m_oWriter.clear();
                m_osCurKey.clear();
            }
        }
        else if (m_nLevel >= m_nKeyLevel)
        {
            --m_nArrayLevel;
            if (m_nLevel > m_nKeyLevel)
                m_oWriter.EndArray();
        }
    }

    void StartArrayMember() override
    {
        if (m_nLevel >= m_nKeyLevel)
            ++m_iArrayMemberIdx;
    }

    void Exception(const char *pszMessage) override
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMessage);
    }

  private:
    std::shared_ptr<VSIKerchunkRefFile> m_refFile{};
    int m_nLevel = 0;
    int m_nArrayLevel = 0;
    int m_iArrayMemberIdx = -1;
    bool m_bFirstMember = false;
    int m_nKeyLevel = std::numeric_limits<int>::max();
    std::string m_osCurKey{};
    std::string m_osURI{};
    std::string m_osTmpForNumber{};
    uint64_t m_nOffset = 0;
    uint32_t m_nSize = 0;

    CPLJSonStreamingWriter m_oWriter{nullptr, nullptr};

    void FinishObjectValueProcessing()
    {
        if (!m_osCurKey.empty())
        {
            const std::string &osStr = m_oWriter.GetString();
            CPL_IGNORE_RET_VAL(m_refFile->AddInlineContent(m_osCurKey, osStr));

            m_oWriter.clear();

            m_osCurKey.clear();
        }
    }

    void UnexpectedContent()
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected content");
        StopParsing();
    }

    void UnexpectedContentInArray()
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected content at position %d of array",
                 m_iArrayMemberIdx);
        StopParsing();
    }
};
}  // namespace

/************************************************************************/
/*           VSIKerchunkJSONRefFileSystem::LoadStreaming()              */
/************************************************************************/

std::shared_ptr<VSIKerchunkRefFile>
VSIKerchunkJSONRefFileSystem::LoadStreaming(const std::string &osJSONFilename,
                                            GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{
    auto refFile = std::make_shared<VSIKerchunkRefFile>();
    VSIKerchunkJSONRefParser parser(refFile);

    CPLDebugOnly("VSIKerchunkJSONRefFileSystem",
                 "Using streaming parser for %s", osJSONFilename.c_str());

    // For network file systems, get the streaming version of the filename,
    // as we don't need arbitrary seeking in the file
    const std::string osFilename =
        VSIFileManager::GetHandler(osJSONFilename.c_str())
            ->GetStreamingFilename(osJSONFilename);

    auto f = VSIVirtualHandleUniquePtr(VSIFOpenL(osFilename.c_str(), "rb"));
    if (!f)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Load json file %s failed",
                 osJSONFilename.c_str());
        return nullptr;
    }
    uint64_t nTotalSize = 0;
    if (!cpl::starts_with(osFilename, "/vsigzip/"))
    {
        f->Seek(0, SEEK_END);
        nTotalSize = f->Tell();
        f->Seek(0, SEEK_SET);
    }
    std::string sBuffer;
    constexpr size_t BUFFER_SIZE = 10 * 1024 * 1024;  // Arbitrary
    sBuffer.resize(BUFFER_SIZE);
    while (true)
    {
        const size_t nRead = f->Read(sBuffer.data(), 1, sBuffer.size());
        const bool bFinished = nRead < sBuffer.size();
        try
        {
            if (!parser.Parse(sBuffer.data(), nRead, bFinished))
            {
                // The parser will have emitted an error
                return nullptr;
            }
        }
        catch (const std::exception &e)
        {
            // Out-of-memory typically
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Exception occurred while parsing %s: %s",
                     osJSONFilename.c_str(), e.what());
            return nullptr;
        }
        if (nTotalSize)
        {
            const double dfProgressRatio = static_cast<double>(f->Tell()) /
                                           static_cast<double>(nTotalSize);
            CPLDebug("VSIKerchunkJSONRefFileSystem", "%02.1f %% of %s read",
                     100 * dfProgressRatio, osJSONFilename.c_str());
            if (pfnProgress &&
                !pfnProgress(dfProgressRatio, "Parsing of JSON file",
                             pProgressData))
            {
                return nullptr;
            }
        }
        else
        {
            CPLDebug("VSIKerchunkJSONRefFileSystem",
                     "%" PRIu64 " bytes read in %s",
                     static_cast<uint64_t>(f->Tell()), osJSONFilename.c_str());
        }
        if (nRead < sBuffer.size())
        {
            break;
        }
    }
    if (f->Tell() == 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Load json file %s failed",
                 osJSONFilename.c_str());
        return nullptr;
    }
    if (pfnProgress)
        pfnProgress(1.0, "Parsing of JSON file", pProgressData);

    return refFile;
}

/************************************************************************/
/*             VSIKerchunkJSONRefFileSystem::LoadInternal()             */
/************************************************************************/

std::shared_ptr<VSIKerchunkRefFile>
VSIKerchunkJSONRefFileSystem::LoadInternal(const std::string &osJSONFilename,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    const char *pszUseStreamingParser = VSIGetPathSpecificOption(
        osJSONFilename.c_str(), "VSIKERCHUNK_USE_STREAMING_PARSER", "AUTO");
    if (EQUAL(pszUseStreamingParser, "AUTO"))
    {
        auto f =
            VSIVirtualHandleUniquePtr(VSIFOpenL(osJSONFilename.c_str(), "rb"));
        if (!f)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Load json file %s failed",
                     osJSONFilename.c_str());
            return nullptr;
        }
        std::string sBuffer;
        constexpr size_t HEADER_SIZE = 1024;  // Arbitrary
        sBuffer.resize(HEADER_SIZE);
        const size_t nRead = f->Read(sBuffer.data(), 1, sBuffer.size());
        sBuffer.resize(nRead);
        if (ZARRIsLikelyStreamableKerchunkJSONRefContent(sBuffer))
        {
            return LoadStreaming(osJSONFilename, pfnProgress, pProgressData);
        }
    }
    else if (CPLTestBool(pszUseStreamingParser))
    {
        return LoadStreaming(osJSONFilename, pfnProgress, pProgressData);
    }

    CPLJSONDocument oDoc;
    {
#if SIZEOF_VOIDP > 4
        CPLConfigOptionSetter oSetter("CPL_JSON_MAX_SIZE", "1GB", true);
#endif
        if (!oDoc.Load(osJSONFilename))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSIKerchunkJSONRefFileSystem: cannot open %s",
                     osJSONFilename.c_str());
            return nullptr;
        }
    }

    auto oRoot = oDoc.GetRoot();
    const auto oVersion = oRoot.GetObj("version");
    CPLJSONObject oRefs;
    if (!oVersion.IsValid())
    {
        // Cf https://fsspec.github.io/kerchunk/spec.html#version-0

        CPLDebugOnly("VSIKerchunkJSONRefFileSystem",
                     "'version' key not found. Assuming version 0");
        oRefs = std::move(oRoot);
        if (!oRefs.GetObj(".zgroup").IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSIKerchunkJSONRefFileSystem: '.zgroup' key not found");
            return nullptr;
        }
    }
    else if (oVersion.GetType() != CPLJSONObject::Type::Integer)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkJSONRefFileSystem: 'version' key not integer");
        return nullptr;
    }
    else if (oVersion.ToInteger() != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VSIKerchunkJSONRefFileSystem: 'version' = %d not handled",
                 oVersion.ToInteger());
        return nullptr;
    }
    else
    {
        // Cf https://fsspec.github.io/kerchunk/spec.html#version-1

        if (oRoot.GetObj("templates").IsValid())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'templates' key found, but "
                     "not supported");
            return nullptr;
        }

        if (oRoot.GetObj("gen").IsValid())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'gen' key found, but not "
                     "supported");
            return nullptr;
        }

        oRefs = oRoot.GetObj("refs");
        if (!oRefs.IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSIKerchunkJSONRefFileSystem: 'refs' key not found");
            return nullptr;
        }

        if (oRoot.GetObj("templates").IsValid())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'templates' key found but "
                     "not supported");
            return nullptr;
        }

        if (oRoot.GetObj("templates").IsValid())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKerchunkJSONRefFileSystem: 'templates' key found but "
                     "not supported");
            return nullptr;
        }
    }

    if (oRefs.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VSIKerchunkJSONRefFileSystem: value of 'refs' is not a dict");
        return nullptr;
    }

    auto refFile = std::make_shared<VSIKerchunkRefFile>();
    for (const auto &oEntry : oRefs.GetChildren())
    {
        const std::string osKeyName = oEntry.GetName();
        if (oEntry.GetType() == CPLJSONObject::Type::String)
        {
            if (!refFile->AddInlineContent(osKeyName, oEntry.ToString()))
            {
                return nullptr;
            }
        }
        else if (oEntry.GetType() == CPLJSONObject::Type::Object)
        {
            const std::string osSerialized =
                oEntry.Format(CPLJSONObject::PrettyFormat::Plain);
            CPL_IGNORE_RET_VAL(
                refFile->AddInlineContent(osKeyName, osSerialized));
        }
        else if (oEntry.GetType() == CPLJSONObject::Type::Array)
        {
            const auto oArray = oEntry.ToArray();
            // Some files such as https://ncsa.osn.xsede.org/Pangeo/pangeo-forge/pangeo-forge/aws-noaa-oisst-feedstock/aws-noaa-oisst-avhrr-only.zarr/reference.json
            // (pointed by https://guide.cloudnativegeo.org/kerchunk/kerchunk-in-practice.html)
            // have array entries with just the URL, and no offset/size
            // This is when the whole file needs to be read
            if (oArray.Size() != 1 && oArray.Size() != 3)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VSIKerchunkJSONRefFileSystem: array value for key "
                         "'%s' is not of size 1 or 3",
                         osKeyName.c_str());
                return nullptr;
            }
            if (oArray[0].GetType() != CPLJSONObject::Type::String)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "VSIKerchunkJSONRefFileSystem: array value at index 0 "
                         "for key '%s' is not a string",
                         osKeyName.c_str());
                return nullptr;
            }
            if (oArray.Size() == 3)
            {
                if ((oArray[1].GetType() != CPLJSONObject::Type::Integer &&
                     oArray[1].GetType() != CPLJSONObject::Type::Long) ||
                    !(oArray[1].ToLong() >= 0))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "VSIKerchunkJSONRefFileSystem: array value at index 1 "
                        "for key '%s' is not an unsigned 64 bit integer",
                        osKeyName.c_str());
                    return nullptr;
                }
                if ((oArray[2].GetType() != CPLJSONObject::Type::Integer &&
                     oArray[2].GetType() != CPLJSONObject::Type::Long) ||
                    !(oArray[2].ToLong() >= 0 &&
                      static_cast<uint64_t>(oArray[2].ToLong()) <=
                          std::numeric_limits<uint32_t>::max()))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "VSIKerchunkJSONRefFileSystem: array value at index 2 "
                        "for key '%s' is not an unsigned 32 bit integer",
                        osKeyName.c_str());
                    return nullptr;
                }
            }

            refFile->AddReferencedContent(
                osKeyName, oArray[0].ToString(),
                oArray.Size() == 3 ? oArray[1].ToLong() : 0,
                oArray.Size() == 3 ? static_cast<uint32_t>(oArray[2].ToLong())
                                   : 0);
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "VSIKerchunkJSONRefFileSystem: invalid value type for key '%s'",
                osKeyName.c_str());
            return nullptr;
        }
    }

    return refFile;
}

/************************************************************************/
/*               VSIKerchunkJSONRefFileSystem::Load()                   */
/************************************************************************/

std::pair<std::shared_ptr<VSIKerchunkRefFile>, std::string>
VSIKerchunkJSONRefFileSystem::Load(const std::string &osJSONFilename,
                                   bool bUseCache)
{
    std::shared_ptr<VSIKerchunkRefFile> refFile;
    if (m_oCache.tryGet(osJSONFilename, refFile))
        return {refFile, std::string()};

    // Deal with local file cache
    const char *pszUseCache = VSIGetPathSpecificOption(
        osJSONFilename.c_str(), "VSIKERCHUNK_USE_CACHE", "NO");
    if (bUseCache || CPLTestBool(pszUseCache))
    {
        if (GDALGetDriverByName("PARQUET") == nullptr)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VSIKERCHUNK_USE_CACHE=YES only enabled if PARQUET driver "
                     "is available");
            return {nullptr, std::string()};
        }

        VSIStatBufL sStat;
        if (VSIStatL(osJSONFilename.c_str(), &sStat) != 0 ||
            VSI_ISDIR(sStat.st_mode))
        {
            CPLError(CE_Failure, CPLE_FileIO, "Load json file %s failed",
                     osJSONFilename.c_str());
            return {nullptr, std::string()};
        }

        std::string osCacheSubDir = CPLGetBasenameSafe(osJSONFilename.c_str());
        osCacheSubDir += CPLSPrintf("_%" PRIu64 "_%" PRIu64,
                                    static_cast<uint64_t>(sStat.st_size),
                                    static_cast<uint64_t>(sStat.st_mtime));

        const std::string osRootCacheDir = GDALGetCacheDirectory();
        if (!osRootCacheDir.empty())
        {
            const std::string osKerchunkCacheDir = VSIGetPathSpecificOption(
                osJSONFilename.c_str(), "VSIKERCHUNK_CACHE_DIR",
                CPLFormFilenameSafe(osRootCacheDir.c_str(),
                                    "zarr_kerchunk_cache", nullptr)
                    .c_str());
            const std::string osCacheDir = CPLFormFilenameSafe(
                osKerchunkCacheDir.c_str(), osCacheSubDir.c_str(), "zarr");
            CPLDebug("VSIKerchunkJSONRefFileSystem", "Using cache dir %s",
                     osCacheDir.c_str());

            if (VSIStatL(CPLFormFilenameSafe(osCacheDir.c_str(), ".zmetadata",
                                             nullptr)
                             .c_str(),
                         &sStat) == 0)
            {
                CPLDebug("VSIKerchunkJSONRefFileSystem",
                         "Using Kerchunk Parquet cache %s", osCacheDir.c_str());
                return {nullptr, osCacheDir};
            }

            if (VSIMkdirRecursive(osCacheDir.c_str(), 0755) != 0 &&
                !(VSIStatL(osCacheDir.c_str(), &sStat) == 0 &&
                  VSI_ISDIR(sStat.st_mode)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create directory %s", osCacheDir.c_str());
                return {nullptr, std::string()};
            }

            const std::string osLockFilename =
                CPLFormFilenameSafe(osCacheDir.c_str(), ".lock", nullptr);

            CPLLockFileHandle hLockHandle = nullptr;
            CPLStringList aosOptions;
            aosOptions.SetNameValue("VERBOSE_WAIT_MESSAGE", "YES");
            const char *pszKerchunkDebug =
                CPLGetConfigOption("VSIKERCHUNK_FOR_TESTS", nullptr);
            if (pszKerchunkDebug &&
                strstr(pszKerchunkDebug, "SHORT_DELAY_STALLED_LOCK"))
            {
                aosOptions.SetNameValue("STALLED_DELAY", "1");
            }

            CPLDebug("VSIKerchunkJSONRefFileSystem", "Acquiring lock");
            switch (CPLLockFileEx(osLockFilename.c_str(), &hLockHandle,
                                  aosOptions.List()))
            {
                case CLFS_OK:
                    break;
                case CLFS_CANNOT_CREATE_LOCK:
                    CPLError(CE_Failure, CPLE_FileIO, "Cannot create lock %s",
                             osLockFilename.c_str());
                    break;
                case CLFS_LOCK_BUSY:
                    CPLAssert(false);  // cannot happen with infinite wait time
                    break;
                case CLFS_API_MISUSE:
                    CPLAssert(false);
                    break;
                case CLFS_THREAD_CREATION_FAILED:
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Thread creation failed for refresh of %s",
                             osLockFilename.c_str());
                    break;
            }
            if (!hLockHandle)
            {
                return {nullptr, std::string()};
            }

            struct LockFileHolder
            {
                CPLLockFileHandle m_hLockHandle = nullptr;

                explicit LockFileHolder(CPLLockFileHandle hLockHandleIn)
                    : m_hLockHandle(hLockHandleIn)
                {
                }

                ~LockFileHolder()
                {
                    release();
                }

                void release()
                {
                    if (m_hLockHandle)
                    {
                        CPLDebug("VSIKerchunkJSONRefFileSystem",
                                 "Releasing lock");
                        CPLUnlockFileEx(m_hLockHandle);
                        m_hLockHandle = nullptr;
                    }
                }

                CPL_DISALLOW_COPY_ASSIGN(LockFileHolder)
            };

            LockFileHolder lockFileHolder(hLockHandle);

            if (VSIStatL(CPLFormFilenameSafe(osCacheDir.c_str(), ".zmetadata",
                                             nullptr)
                             .c_str(),
                         &sStat) == 0)
            {
                CPLDebug("VSIKerchunkJSONRefFileSystem",
                         "Using Kerchunk Parquet cache %s (after lock taking)",
                         osCacheDir.c_str());
                return {nullptr, osCacheDir};
            }

            refFile = LoadInternal(osJSONFilename, nullptr, nullptr);

            if (refFile)
            {
                CPLDebug("VSIKerchunkJSONRefFileSystem",
                         "Generating Kerchunk Parquet cache %s...",
                         osCacheDir.c_str());

                if (pszKerchunkDebug &&
                    strstr(pszKerchunkDebug,
                           "WAIT_BEFORE_CONVERT_TO_PARQUET_REF"))
                {
                    CPLSleep(0.5);
                }

                if (refFile->ConvertToParquetRef(osCacheDir, nullptr, nullptr))
                {
                    CPLDebug("VSIKerchunkJSONRefFileSystem",
                             "Generation Kerchunk Parquet cache %s: OK",
                             osCacheDir.c_str());
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Generation of Kerchunk Parquet cache %s failed",
                             osCacheDir.c_str());
                    refFile.reset();
                }

                lockFileHolder.release();
                m_oCache.insert(osJSONFilename, refFile);
            }

            return {refFile, std::string()};
        }
    }

    refFile = LoadInternal(osJSONFilename, nullptr, nullptr);
    if (refFile)
        m_oCache.insert(osJSONFilename, refFile);
    return {refFile, std::string()};
}

/************************************************************************/
/*          VSIKerchunkRefFile::ConvertToParquetRef()                   */
/************************************************************************/

bool VSIKerchunkRefFile::ConvertToParquetRef(const std::string &osCacheDir,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    struct Serializer
    {
        VSIVirtualHandle *m_poFile = nullptr;

        explicit Serializer(VSIVirtualHandle *poFile) : m_poFile(poFile)
        {
        }

        static void func(const char *pszTxt, void *pUserData)
        {
            Serializer *self = static_cast<Serializer *>(pUserData);
            self->m_poFile->Write(pszTxt, 1, strlen(pszTxt));
        }
    };

    const std::string osZMetadataFilename =
        CPLFormFilenameSafe(osCacheDir.c_str(), ".zmetadata", nullptr);
    const std::string osZMetadataTmpFilename = osZMetadataFilename + ".tmp";
    auto poFile = std::unique_ptr<VSIVirtualHandle>(
        VSIFOpenL(osZMetadataTmpFilename.c_str(), "wb"));
    if (!poFile)
        return false;
    Serializer serializer(poFile.get());

    struct ZarrArrayInfo
    {
        std::vector<uint64_t> anChunkCount{};
        std::map<uint64_t, const VSIKerchunkKeyInfo *> chunkInfo{};
    };

    std::map<std::string, ZarrArrayInfo> zarrArrays;

    // First pass on keys to write JSON objects in .zmetadata
    CPLJSonStreamingWriter oWriter(Serializer::func, &serializer);
    oWriter.StartObj();
    oWriter.AddObjKey("metadata");
    oWriter.StartObj();

    bool bOK = true;
    size_t nCurObjectIter = 0;
    const size_t nTotalObjects = m_oMapKeys.size();
    for (const auto &[key, info] : m_oMapKeys)
    {
        if (cpl::ends_with(key, ".zarray") || cpl::ends_with(key, ".zgroup") ||
            cpl::ends_with(key, ".zattrs"))
        {
            CPLJSONDocument oDoc;
            std::string osStr;
            osStr.append(reinterpret_cast<const char *>(info.abyValue.data()),
                         info.abyValue.size());
            if (!oDoc.LoadMemory(osStr.c_str()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot parse JSON content for %s", key.c_str());
                bOK = false;
                break;
            }

            if (cpl::ends_with(key, ".zarray"))
            {
                const std::string osArrayName = CPLGetDirnameSafe(key.c_str());

                const auto oShape = oDoc.GetRoot().GetArray("shape");
                const auto oChunks = oDoc.GetRoot().GetArray("chunks");
                if (oShape.IsValid() && oChunks.IsValid() &&
                    oShape.Size() == oChunks.Size())
                {
                    std::vector<uint64_t> anChunkCount;
                    uint64_t nTotalChunkCount = 1;
                    for (int i = 0; i < oShape.Size(); ++i)
                    {
                        const auto nShape = oShape[i].ToLong();
                        const auto nChunk = oChunks[i].ToLong();
                        if (nShape == 0 || nChunk == 0)
                        {
                            bOK = false;
                            break;
                        }
                        const uint64_t nChunkCount =
                            DIV_ROUND_UP(nShape, nChunk);
                        if (nChunkCount > std::numeric_limits<uint64_t>::max() /
                                              nTotalChunkCount)
                        {
                            bOK = false;
                            break;
                        }
                        anChunkCount.push_back(nChunkCount);
                        nTotalChunkCount *= nChunkCount;
                    }
                    zarrArrays[osArrayName].anChunkCount =
                        std::move(anChunkCount);
                }
                else
                {
                    bOK = false;
                }
                if (!bOK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid Zarr array definition for %s",
                             osArrayName.c_str());
                    oWriter.clear();
                    break;
                }
            }

            oWriter.AddObjKey(key);
            oWriter.AddSerializedValue(osStr);

            ++nCurObjectIter;
            if (pfnProgress &&
                !pfnProgress(static_cast<double>(nCurObjectIter) /
                                 static_cast<double>(nTotalObjects),
                             "", pProgressData))
            {
                return false;
            }
        }
    }

    constexpr int nRecordSize = 100000;

    if (bOK)
    {
        oWriter.EndObj();
        oWriter.AddObjKey("record_size");
        oWriter.Add(nRecordSize);
        oWriter.EndObj();
    }

    bOK = (poFile->Close() == 0) && bOK;
    poFile.reset();

    if (!bOK)
    {
        oWriter.clear();
        VSIUnlink(osZMetadataTmpFilename.c_str());
        return false;
    }

    // Second pass to retrieve chunks
    for (const auto &[key, info] : m_oMapKeys)
    {
        if (cpl::ends_with(key, ".zarray") || cpl::ends_with(key, ".zgroup") ||
            cpl::ends_with(key, ".zattrs"))
        {
            // already done
        }
        else
        {
            const std::string osArrayName = CPLGetDirnameSafe(key.c_str());
            auto oIter = zarrArrays.find(osArrayName);
            if (oIter != zarrArrays.end())
            {
                auto &oArrayInfo = oIter->second;
                const CPLStringList aosIndices(
                    CSLTokenizeString2(CPLGetFilename(key.c_str()), ".", 0));
                if ((static_cast<size_t>(aosIndices.size()) ==
                     oArrayInfo.anChunkCount.size()) ||
                    (aosIndices.size() == 1 &&
                     strcmp(aosIndices[0], "0") == 0 &&
                     oArrayInfo.anChunkCount.empty()))
                {
                    std::vector<uint64_t> anIndices;
                    for (size_t i = 0; i < oArrayInfo.anChunkCount.size(); ++i)
                    {
                        char *endptr = nullptr;
                        anIndices.push_back(
                            std::strtoull(aosIndices[i], &endptr, 10));
                        if (aosIndices[i][0] == '-' ||
                            endptr != aosIndices[i] + strlen(aosIndices[i]) ||
                            anIndices[i] >= oArrayInfo.anChunkCount[i])
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Invalid key indices: %s", key.c_str());
                            return false;
                        }
                    }

                    uint64_t nLinearIndex = 0;
                    uint64_t nMulFactor = 1;
                    for (size_t i = anIndices.size(); i > 0;)
                    {
                        --i;
                        nLinearIndex += anIndices[i] * nMulFactor;
                        nMulFactor *= oArrayInfo.anChunkCount[i];
                    }

#ifdef DEBUG_VERBOSE
                    CPLDebugOnly("VSIKerchunkJSONRefFileSystem",
                                 "Chunk %" PRIu64 " of array %s found",
                                 nLinearIndex, osArrayName.c_str());
#endif
                    oArrayInfo.chunkInfo[nLinearIndex] = &info;
                }
            }
        }
    }

    auto poDrv = GetGDALDriverManager()->GetDriverByName("PARQUET");
    if (!poDrv)
    {
        // shouldn't happen given earlier checks
        CPLAssert(false);
        return false;
    }

    // Third pass to create Parquet files
    CPLStringList aosLayerCreationOptions;
    aosLayerCreationOptions.SetNameValue("ROW_GROUP_SIZE",
                                         CPLSPrintf("%d", nRecordSize));

    for (const auto &[osArrayName, oArrayInfo] : zarrArrays)
    {
        uint64_t nChunkCount = 1;
        for (size_t i = 0; i < oArrayInfo.anChunkCount.size(); ++i)
        {
            nChunkCount *= oArrayInfo.anChunkCount[i];
        }

        std::unique_ptr<GDALDataset> poDS;
        OGRLayer *poLayer = nullptr;

        for (uint64_t i = 0; i < nChunkCount; ++i)
        {
            if ((i % nRecordSize) == 0)
            {
                if (poDS)
                {
                    if (poDS->Close() != CE_None)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Close() on %s failed",
                                 poDS->GetDescription());
                        return false;
                    }
                    poDS.reset();
                }
                const std::string osParqFilename = CPLFormFilenameSafe(
                    CPLFormFilenameSafe(osCacheDir.c_str(), osArrayName.c_str(),
                                        nullptr)
                        .c_str(),
                    CPLSPrintf("refs.%" PRIu64 ".parq", i / nRecordSize),
                    nullptr);
                CPLDebugOnly("VSIKerchunkJSONRefFileSystem", "Creating %s",
                             osParqFilename.c_str());
                VSIMkdirRecursive(
                    CPLGetPathSafe(osParqFilename.c_str()).c_str(), 0755);
                poDS.reset(poDrv->Create(osParqFilename.c_str(), 0, 0, 0,
                                         GDT_Unknown, nullptr));
                if (!poDS)
                    return false;
                poLayer = poDS->CreateLayer(
                    CPLGetBasenameSafe(osParqFilename.c_str()).c_str(), nullptr,
                    wkbNone, aosLayerCreationOptions.List());
                if (poLayer)
                {
                    {
                        OGRFieldDefn oFieldDefn("path", OFTString);
                        poLayer->CreateField(&oFieldDefn);
                    }
                    {
                        OGRFieldDefn oFieldDefn("offset", OFTInteger64);
                        poLayer->CreateField(&oFieldDefn);
                    }
                    {
                        OGRFieldDefn oFieldDefn("size", OFTInteger64);
                        poLayer->CreateField(&oFieldDefn);
                    }
                    {
                        OGRFieldDefn oFieldDefn("raw", OFTBinary);
                        poLayer->CreateField(&oFieldDefn);
                    }
                }
            }
            if (!poLayer)
                return false;

            auto poFeature =
                std::make_unique<OGRFeature>(poLayer->GetLayerDefn());
            auto oIter = oArrayInfo.chunkInfo.find(i);
            if (oIter != oArrayInfo.chunkInfo.end())
            {
                const auto *chunkInfo = oIter->second;
                if (chunkInfo->posURI)
                    poFeature->SetField(0, chunkInfo->posURI->c_str());
                poFeature->SetField(1,
                                    static_cast<GIntBig>(chunkInfo->nOffset));
                poFeature->SetField(2, static_cast<GIntBig>(chunkInfo->nSize));
                if (!chunkInfo->abyValue.empty())
                {
                    if (chunkInfo->abyValue.size() >
                        static_cast<size_t>(INT_MAX))
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Too big blob for chunk %" PRIu64
                                 " of array %s",
                                 i, osArrayName.c_str());
                        return false;
                    }
                    poFeature->SetField(
                        3, static_cast<int>(chunkInfo->abyValue.size()),
                        static_cast<const void *>(chunkInfo->abyValue.data()));
                }
            }

            if (poLayer->CreateFeature(poFeature.get()) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "CreateFeature() on %s failed",
                         poDS->GetDescription());
                return false;
            }

            ++nCurObjectIter;
            if (pfnProgress && (nCurObjectIter % 1000) == 0 &&
                !pfnProgress(static_cast<double>(nCurObjectIter) /
                                 static_cast<double>(nTotalObjects),
                             "", pProgressData))
            {
                return false;
            }
        }

        if (poDS)
        {
            if (poDS->Close() != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Close() on %s failed",
                         poDS->GetDescription());
                return false;
            }
            poDS.reset();
        }
    }

    if (VSIRename(osZMetadataTmpFilename.c_str(),
                  osZMetadataFilename.c_str()) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot rename %s to %s",
                 osZMetadataTmpFilename.c_str(), osZMetadataFilename.c_str());
        return false;
    }

    if (pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    return true;
}

/************************************************************************/
/*                   VSIKerchunkConvertJSONToParquet()                  */
/************************************************************************/

bool VSIKerchunkConvertJSONToParquet(const char *pszSrcJSONFilename,
                                     const char *pszDstDirname,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    if (GDALGetDriverByName("PARQUET") == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Conversion to a Parquet reference store is not possible "
                 "because the PARQUET driver is not available.");
        return false;
    }

    auto poFS = cpl::down_cast<VSIKerchunkJSONRefFileSystem *>(
        VSIFileManager::GetHandler(JSON_REF_FS_PREFIX));
    std::shared_ptr<VSIKerchunkRefFile> refFile;
    if (!poFS->m_oCache.tryGet(pszSrcJSONFilename, refFile))
    {
        void *pScaledProgressData =
            GDALCreateScaledProgress(0.0, 0.5, pfnProgress, pProgressData);
        try
        {
            refFile = poFS->LoadInternal(
                pszSrcJSONFilename,
                pScaledProgressData ? GDALScaledProgress : nullptr,
                pScaledProgressData);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "VSIKerchunkJSONRefFileSystem::Load() failed: %s",
                     e.what());
            GDALDestroyScaledProgress(pScaledProgressData);
            return false;
        }
        GDALDestroyScaledProgress(pScaledProgressData);
    }
    if (!refFile)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is not a Kerchunk JSON reference store",
                 pszSrcJSONFilename);
        return false;
    }

    VSIMkdir(pszDstDirname, 0755);

    void *pScaledProgressData =
        GDALCreateScaledProgress(0.5, 1.0, pfnProgress, pProgressData);
    const bool bRet = refFile->ConvertToParquetRef(
        pszDstDirname, pScaledProgressData ? GDALScaledProgress : nullptr,
        pScaledProgressData);
    GDALDestroyScaledProgress(pScaledProgressData);
    return bRet;
}

/************************************************************************/
/*               VSIKerchunkJSONRefFileSystem::Open()                   */
/************************************************************************/

VSIVirtualHandle *
VSIKerchunkJSONRefFileSystem::Open(const char *pszFilename,
                                   const char *pszAccess, bool /* bSetError */,
                                   CSLConstList /* papszOptions */)
{
    CPLDebugOnly("VSIKerchunkJSONRefFileSystem", "Open(%s)", pszFilename);
    if (strcmp(pszAccess, "r") != 0 && strcmp(pszAccess, "rb") != 0)
        return nullptr;

    const auto [osJSONFilename, osKey] = SplitFilename(pszFilename);
    if (osJSONFilename.empty())
        return nullptr;

    const auto [refFile, osParqFilename] = Load(
        osJSONFilename, STARTS_WITH(pszFilename, JSON_REF_CACHED_FS_PREFIX));
    if (!refFile)
    {
        if (osParqFilename.empty())
            return nullptr;

        return VSIFOpenL(
            CPLFormFilenameSafe(CPLSPrintf("%s{%s}", PARQUET_REF_FS_PREFIX,
                                           osParqFilename.c_str()),
                                osKey.c_str(), nullptr)
                .c_str(),
            pszAccess);
    }

    const auto oIter = refFile->GetMapKeys().find(osKey);
    if (oIter == refFile->GetMapKeys().end())
        return nullptr;

    const auto &keyInfo = oIter->second;
    if (!keyInfo.posURI)
    {
        return VSIFileFromMemBuffer(
            nullptr, const_cast<GByte *>(keyInfo.abyValue.data()),
            keyInfo.abyValue.size(), /* bTakeOwnership = */ false);
    }
    else
    {
        std::string osVSIPath = VSIKerchunkMorphURIToVSIPath(
            *(keyInfo.posURI), CPLGetPathSafe(osJSONFilename.c_str()));
        if (osVSIPath.empty())
            return nullptr;
        const std::string osPath =
            keyInfo.nSize
                ? CPLSPrintf("/vsisubfile/%" PRIu64 "_%u,%s", keyInfo.nOffset,
                             keyInfo.nSize, osVSIPath.c_str())
                : std::move(osVSIPath);
        CPLDebugOnly("VSIKerchunkJSONRefFileSystem", "Opening %s",
                     osPath.c_str());
        CPLConfigOptionSetter oSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                      "EMPTY_DIR", false);
        auto fp = VSIFOpenEx2L(osPath.c_str(), "rb", true, nullptr);
        if (!fp)
        {
            if (!VSIToCPLError(CE_Failure, CPLE_FileIO))
                CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                         osPath.c_str());
        }
        return fp;
    }
}

/************************************************************************/
/*               VSIKerchunkJSONRefFileSystem::Stat()                   */
/************************************************************************/

int VSIKerchunkJSONRefFileSystem::Stat(const char *pszFilename,
                                       VSIStatBufL *pStatBuf, int nFlags)
{
    CPLDebugOnly("VSIKerchunkJSONRefFileSystem", "Stat(%s)", pszFilename);
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    const auto [osJSONFilename, osKey] = SplitFilename(pszFilename);
    if (osJSONFilename.empty())
        return -1;

    const auto [refFile, osParqFilename] = Load(
        osJSONFilename, STARTS_WITH(pszFilename, JSON_REF_CACHED_FS_PREFIX));
    if (!refFile)
    {
        if (osParqFilename.empty())
            return -1;

        return VSIStatExL(
            CPLFormFilenameSafe(CPLSPrintf("%s{%s}", PARQUET_REF_FS_PREFIX,
                                           osParqFilename.c_str()),
                                osKey.c_str(), nullptr)
                .c_str(),
            pStatBuf, nFlags);
    }

    if (osKey.empty())
    {
        pStatBuf->st_mode = S_IFDIR;
        return 0;
    }

    const auto oIter = refFile->GetMapKeys().find(osKey);
    if (oIter == refFile->GetMapKeys().end())
    {
        if (cpl::contains(refFile->GetMapKeys(), osKey + "/.zgroup") ||
            cpl::contains(refFile->GetMapKeys(), osKey + "/.zarray"))
        {
            pStatBuf->st_mode = S_IFDIR;
            return 0;
        }

        return -1;
    }

    const auto &keyInfo = oIter->second;
    if (!(keyInfo.posURI))
    {
        pStatBuf->st_size = keyInfo.abyValue.size();
    }
    else
    {
        if (keyInfo.nSize)
        {
            pStatBuf->st_size = keyInfo.nSize;
        }
        else
        {
            const std::string osVSIPath = VSIKerchunkMorphURIToVSIPath(
                *(keyInfo.posURI), CPLGetPathSafe(osJSONFilename.c_str()));
            if (osVSIPath.empty())
                return -1;
            return VSIStatExL(osVSIPath.c_str(), pStatBuf, nFlags);
        }
    }
    pStatBuf->st_mode = S_IFREG;

    return 0;
}

/************************************************************************/
/*             VSIKerchunkJSONRefFileSystem::ReadDirEx()                */
/************************************************************************/

char **VSIKerchunkJSONRefFileSystem::ReadDirEx(const char *pszDirname,
                                               int nMaxFiles)
{
    CPLDebugOnly("VSIKerchunkJSONRefFileSystem", "ReadDir(%s)", pszDirname);

    const auto [osJSONFilename, osAskedKey] = SplitFilename(pszDirname);
    if (osJSONFilename.empty())
        return nullptr;

    const auto [refFile, osParqFilename] = Load(
        osJSONFilename, STARTS_WITH(pszDirname, JSON_REF_CACHED_FS_PREFIX));
    if (!refFile)
    {
        if (osParqFilename.empty())
            return nullptr;

        return VSIReadDirEx(
            CPLFormFilenameSafe(CPLSPrintf("%s{%s}", PARQUET_REF_FS_PREFIX,
                                           osParqFilename.c_str()),
                                osAskedKey.c_str(), nullptr)
                .c_str(),
            nMaxFiles);
    }

    std::set<std::string> set;
    for (const auto &[key, value] : refFile->GetMapKeys())
    {
        if (osAskedKey.empty())
        {
            const auto nPos = key.find('/');
            if (nPos == std::string::npos)
                set.insert(key);
            else
                set.insert(key.substr(0, nPos));
        }
        else if (key.size() > osAskedKey.size() &&
                 cpl::starts_with(key, osAskedKey) &&
                 key[osAskedKey.size()] == '/')
        {
            std::string subKey = key.substr(osAskedKey.size() + 1);
            const auto nPos = subKey.find('/');
            if (nPos == std::string::npos)
                set.insert(std::move(subKey));
            else
                set.insert(subKey.substr(0, nPos));
        }
    }

    CPLStringList aosRet;
    for (const std::string &v : set)
    {
        // CPLDebugOnly("VSIKerchunkJSONRefFileSystem", ".. %s", v.c_str());
        aosRet.AddString(v.c_str());
    }
    return aosRet.StealList();
}

/************************************************************************/
/*                VSIInstallKerchunkJSONRefFileSystem()                 */
/************************************************************************/

void VSIInstallKerchunkJSONRefFileSystem()
{
    static std::mutex oMutex;
    std::lock_guard<std::mutex> oLock(oMutex);
    // cppcheck-suppress knownConditionTrueFalse
    if (!VSIKerchunkJSONRefFileSystem::IsFileSystemInstantiated())
    {
        auto fs = std::make_unique<VSIKerchunkJSONRefFileSystem>().release();
        VSIFileManager::InstallHandler(JSON_REF_FS_PREFIX, fs);
        VSIFileManager::InstallHandler(JSON_REF_CACHED_FS_PREFIX, fs);
    }
}
