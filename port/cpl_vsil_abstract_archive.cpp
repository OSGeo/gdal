/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for archive files.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_vsi_virtual.h"

#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

//! @cond Doxygen_Suppress

static bool IsEitherSlash(char c)
{
    return c == '/' || c == '\\';
}

/************************************************************************/
/*                     ~VSIArchiveEntryFileOffset()                     */
/************************************************************************/

VSIArchiveEntryFileOffset::~VSIArchiveEntryFileOffset() = default;

/************************************************************************/
/*                         ~VSIArchiveReader()                          */
/************************************************************************/

VSIArchiveReader::~VSIArchiveReader() = default;

/************************************************************************/
/*                         ~VSIArchiveContent()                         */
/************************************************************************/

VSIArchiveContent::~VSIArchiveContent() = default;

/************************************************************************/
/*                    VSIArchiveFilesystemHandler()                     */
/************************************************************************/

VSIArchiveFilesystemHandler::VSIArchiveFilesystemHandler() = default;

/************************************************************************/
/*                    ~VSIArchiveFilesystemHandler()                    */
/************************************************************************/

VSIArchiveFilesystemHandler::~VSIArchiveFilesystemHandler() = default;

/************************************************************************/
/*                        GetStrippedFilename()                         */
/************************************************************************/

static std::string GetStrippedFilename(const std::string &osFileName,
                                       bool &bIsDir)
{
    bIsDir = false;
    int nStartPos = 0;

    // Remove ./ pattern at the beginning of a filename.
    if (osFileName.size() >= 2 && osFileName[0] == '.' && osFileName[1] == '/')
    {
        nStartPos = 2;
        if (osFileName.size() == 2)
            return std::string();
    }

    std::string ret(osFileName, nStartPos);
    for (char &c : ret)
    {
        if (c == '\\')
            c = '/';
    }

    bIsDir = !ret.empty() && ret.back() == '/';
    if (bIsDir)
    {
        // Remove trailing slash.
        ret.pop_back();
    }
    return ret;
}

/************************************************************************/
/*                        BuildDirectoryIndex()                         */
/************************************************************************/

static void BuildDirectoryIndex(VSIArchiveContent *content)
{
    content->dirIndex.clear();
    const int nEntries = static_cast<int>(content->entries.size());
    for (int i = 0; i < nEntries; i++)
    {
        const char *fileName = content->entries[i].fileName.c_str();
        std::string parentDir = CPLGetPathSafe(fileName);
        content->dirIndex[parentDir].push_back(i);
    }
}

/************************************************************************/
/*                        GetContentOfArchive()                         */
/************************************************************************/

const VSIArchiveContent *
VSIArchiveFilesystemHandler::GetContentOfArchive(const char *archiveFilename,
                                                 VSIArchiveReader *poReader)
{
    std::unique_lock oLock(oMutex);

    VSIStatBufL sStat;
    if (VSIStatL(archiveFilename, &sStat) != 0)
        return nullptr;

    auto oIter = oFileList.find(archiveFilename);
    if (oIter != oFileList.end())
    {
        const VSIArchiveContent *content = oIter->second.get();
        if (static_cast<time_t>(sStat.st_mtime) > content->mTime ||
            static_cast<vsi_l_offset>(sStat.st_size) != content->nFileSize)
        {
            CPLDebug("VSIArchive",
                     "The content of %s has changed since it was cached",
                     archiveFilename);
            oFileList.erase(archiveFilename);
        }
        else
        {
            return content;
        }
    }

    std::unique_ptr<VSIArchiveReader> temporaryReader;  // keep in that scope
    if (poReader == nullptr)
    {
        temporaryReader = CreateReader(archiveFilename);
        poReader = temporaryReader.get();
        if (!poReader)
            return nullptr;
    }

    if (poReader->GotoFirstFile() == FALSE)
    {
        return nullptr;
    }

    auto content = std::make_unique<VSIArchiveContent>();
    content->mTime = sStat.st_mtime;
    content->nFileSize = static_cast<vsi_l_offset>(sStat.st_size);

    std::set<std::string> oSet;

    do
    {
        bool bIsDir = false;
        std::string osStrippedFilename =
            GetStrippedFilename(poReader->GetFileName(), bIsDir);
        if (osStrippedFilename.empty() || osStrippedFilename[0] == '/' ||
            osStrippedFilename.find("//") != std::string::npos)
        {
            continue;
        }

        if (oSet.find(osStrippedFilename) == oSet.end())
        {
            oSet.insert(osStrippedFilename);

            // Add intermediate directory structure.
            for (size_t i = 0; i < osStrippedFilename.size(); ++i)
            {
                if (osStrippedFilename[i] == '/')
                {
                    std::string osSubdirName(osStrippedFilename, 0, i);
                    if (oSet.find(osSubdirName) == oSet.end())
                    {
                        oSet.insert(osSubdirName);

                        VSIArchiveEntry entry;
                        entry.fileName = std::move(osSubdirName);
                        entry.nModifiedTime = poReader->GetModifiedTime();
                        entry.bIsDir = true;
#ifdef DEBUG_VERBOSE
                        CPLDebug(
                            "VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes",
                            static_cast<int>(content->entries.size() + 1),
                            entry.fileName.c_str(), entry.uncompressed_size);
#endif
                        content->entries.push_back(std::move(entry));
                    }
                }
            }

            VSIArchiveEntry entry;
            entry.fileName = std::move(osStrippedFilename);
            entry.nModifiedTime = poReader->GetModifiedTime();
            entry.uncompressed_size = poReader->GetFileSize();
            entry.bIsDir = bIsDir;
            entry.file_pos.reset(poReader->GetFileOffset());
#ifdef DEBUG_VERBOSE
            CPLDebug("VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes",
                     static_cast<int>(content->entries.size() + 1),
                     entry.fileName.c_str(), entry.uncompressed_size);
#endif
            content->entries.push_back(std::move(entry));
        }

    } while (poReader->GotoNextFile());

    // Build directory index for fast lookups
    BuildDirectoryIndex(content.get());

    return oFileList
        .insert(std::pair<CPLString, std::unique_ptr<VSIArchiveContent>>(
            archiveFilename, std::move(content)))
        .first->second.get();
}

/************************************************************************/
/*                         FindFileInArchive()                          */
/************************************************************************/

bool VSIArchiveFilesystemHandler::FindFileInArchive(
    const char *archiveFilename, const char *fileInArchiveName,
    const VSIArchiveEntry **archiveEntry)
{
    CPLAssert(fileInArchiveName);

    const VSIArchiveContent *content = GetContentOfArchive(archiveFilename);
    if (content)
    {
        const std::string parentDir = CPLGetPathSafe(fileInArchiveName);

        // Use directory index to search within parent directory's children
        auto dirIter = content->dirIndex.find(parentDir);
        if (dirIter != content->dirIndex.end())
        {
            const std::vector<int> &childIndices = dirIter->second;
            for (int childIdx : childIndices)
            {
                if (content->entries[childIdx].fileName == fileInArchiveName)
                {
                    if (archiveEntry)
                        *archiveEntry = &content->entries[childIdx];
                    return true;
                }
            }
        }
    }
    return false;
}

/************************************************************************/
/*                          CompactFilename()                           */
/************************************************************************/

static std::string CompactFilename(const char *pszArchiveInFileNameIn)
{
    std::string osRet(pszArchiveInFileNameIn);

    // Replace a/../b by b and foo/a/../b by foo/b.
    while (true)
    {
        size_t nSlashDotDot = osRet.find("/../");
        if (nSlashDotDot == std::string::npos || nSlashDotDot == 0)
            break;
        size_t nPos = nSlashDotDot - 1;
        while (nPos > 0 && osRet[nPos] != '/')
            --nPos;
        if (nPos == 0)
            osRet = osRet.substr(nSlashDotDot + strlen("/../"));
        else
            osRet = osRet.substr(0, nPos + 1) +
                    osRet.substr(nSlashDotDot + strlen("/../"));
    }
    return osRet;
}

/************************************************************************/
/*                           SplitFilename()                            */
/************************************************************************/

std::unique_ptr<char, VSIFreeReleaser>
VSIArchiveFilesystemHandler::SplitFilename(const char *pszFilename,
                                           CPLString &osFileInArchive,
                                           bool bCheckMainFileExists,
                                           bool bSetError) const
{
    // TODO(schwehr): Cleanup redundant calls to GetPrefix and strlen.
    if (strcmp(pszFilename, GetPrefix()) == 0)
        return nullptr;

    int i = 0;

    // Detect extended syntax: /vsiXXX/{archive_filename}/file_in_archive.
    if (pszFilename[strlen(GetPrefix()) + 1] == '{')
    {
        pszFilename += strlen(GetPrefix()) + 1;
        int nCountCurlies = 0;
        while (pszFilename[i])
        {
            if (pszFilename[i] == '{')
                nCountCurlies++;
            else if (pszFilename[i] == '}')
            {
                nCountCurlies--;
                if (nCountCurlies == 0)
                    break;
            }
            i++;
        }
        if (nCountCurlies > 0)
            return nullptr;
        char *archiveFilename = CPLStrdup(pszFilename + 1);
        archiveFilename[i - 1] = 0;

        bool bArchiveFileExists = false;
        if (!bCheckMainFileExists)
        {
            bArchiveFileExists = true;
        }
        else
        {
            std::unique_lock oLock(oMutex);

            if (oFileList.find(archiveFilename) != oFileList.end())
            {
                bArchiveFileExists = true;
            }
        }

        if (!bArchiveFileExists)
        {
            VSIStatBufL statBuf;
            VSIFilesystemHandler *poFSHandler =
                VSIFileManager::GetHandler(archiveFilename);
            int nFlags = VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG;
            if (bSetError)
                nFlags |= VSI_STAT_SET_ERROR_FLAG;
            if (poFSHandler->Stat(archiveFilename, &statBuf, nFlags) == 0 &&
                !VSI_ISDIR(statBuf.st_mode))
            {
                bArchiveFileExists = true;
            }
        }

        if (bArchiveFileExists)
        {
            if (IsEitherSlash(pszFilename[i + 1]))
            {
                osFileInArchive = CompactFilename(pszFilename + i + 2);
            }
            else if (pszFilename[i + 1] == '\0')
            {
                osFileInArchive = "";
            }
            else
            {
                CPLFree(archiveFilename);
                return nullptr;
            }

            // Remove trailing slash.
            if (!osFileInArchive.empty())
            {
                const char lastC = osFileInArchive.back();
                if (IsEitherSlash(lastC))
                    osFileInArchive.pop_back();
            }

            return std::unique_ptr<char, VSIFreeReleaser>(archiveFilename);
        }

        CPLFree(archiveFilename);
        return nullptr;
    }

    // Allow natural chaining of VSI drivers without requiring double slash.

    CPLString osDoubleVsi(GetPrefix());
    osDoubleVsi += "/vsi";

    if (strncmp(pszFilename, osDoubleVsi.c_str(), osDoubleVsi.size()) == 0)
        pszFilename += strlen(GetPrefix());
    else
        pszFilename += strlen(GetPrefix()) + 1;

    // Parsing strings like
    // /vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar/a.tgzb.tgzc.tgzd.tgze.tgzf.tgz.h.tgz.i.tgz
    // takes a huge amount of time, so limit the number of nesting of such
    // file systems.
    int *pnCounter = static_cast<int *>(CPLGetTLS(CTLS_ABSTRACTARCHIVE_SPLIT));
    if (pnCounter == nullptr)
    {
        pnCounter = static_cast<int *>(CPLMalloc(sizeof(int)));
        *pnCounter = 0;
        CPLSetTLS(CTLS_ABSTRACTARCHIVE_SPLIT, pnCounter, TRUE);
    }
    if (*pnCounter == 3)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too deep recursion level in "
                 "VSIArchiveFilesystemHandler::SplitFilename()");
        return nullptr;
    }

    const std::vector<CPLString> oExtensions = GetExtensions();
    int nAttempts = 0;
    while (pszFilename[i])
    {
        int nToSkip = 0;

        for (std::vector<CPLString>::const_iterator iter = oExtensions.begin();
             iter != oExtensions.end(); ++iter)
        {
            const CPLString &osExtension = *iter;
            if (EQUALN(pszFilename + i, osExtension.c_str(),
                       osExtension.size()))
            {
                nToSkip = static_cast<int>(osExtension.size());
                break;
            }
        }

#ifdef DEBUG
        // For AFL, so that .cur_input is detected as the archive filename.
        if (EQUALN(pszFilename + i, ".cur_input", strlen(".cur_input")))
        {
            nToSkip = static_cast<int>(strlen(".cur_input"));
        }
#endif

        if (nToSkip != 0)
        {
            nAttempts++;
            // Arbitrary threshold to avoid DoS with things like
            // /vsitar/my.tar/my.tar/my.tar/my.tar/my.tar/my.tar/my.tar
            if (nAttempts == 5)
            {
                break;
            }
            VSIStatBufL statBuf;
            char *archiveFilename = CPLStrdup(pszFilename);
            bool bArchiveFileExists = false;

            if (IsEitherSlash(archiveFilename[i + nToSkip]))
            {
                archiveFilename[i + nToSkip] = 0;
            }

            if (!bCheckMainFileExists)
            {
                bArchiveFileExists = true;
            }
            else
            {
                std::unique_lock oLock(oMutex);

                if (oFileList.find(archiveFilename) != oFileList.end())
                {
                    bArchiveFileExists = true;
                }
            }

            if (!bArchiveFileExists)
            {
                (*pnCounter)++;

                VSIFilesystemHandler *poFSHandler =
                    VSIFileManager::GetHandler(archiveFilename);
                if (poFSHandler->Stat(archiveFilename, &statBuf,
                                      VSI_STAT_EXISTS_FLAG |
                                          VSI_STAT_NATURE_FLAG) == 0 &&
                    !VSI_ISDIR(statBuf.st_mode))
                {
                    bArchiveFileExists = true;
                }

                (*pnCounter)--;
            }

            if (bArchiveFileExists)
            {
                if (IsEitherSlash(pszFilename[i + nToSkip]))
                {
                    osFileInArchive =
                        CompactFilename(pszFilename + i + nToSkip + 1);
                }
                else
                {
                    osFileInArchive = "";
                }

                // Remove trailing slash.
                if (!osFileInArchive.empty())
                {
                    const char lastC = osFileInArchive.back();
                    if (IsEitherSlash(lastC))
                        osFileInArchive.resize(osFileInArchive.size() - 1);
                }

                return std::unique_ptr<char, VSIFreeReleaser>(archiveFilename);
            }
            CPLFree(archiveFilename);
        }
        i++;
    }
    return nullptr;
}

/************************************************************************/
/*                          OpenArchiveFile()                           */
/************************************************************************/

std::unique_ptr<VSIArchiveReader>
VSIArchiveFilesystemHandler::OpenArchiveFile(const char *archiveFilename,
                                             const char *fileInArchiveName)
{
    auto poReader = CreateReader(archiveFilename);

    if (poReader == nullptr)
    {
        return nullptr;
    }

    if (fileInArchiveName == nullptr || strlen(fileInArchiveName) == 0)
    {
        if (poReader->GotoFirstFile() == FALSE)
        {
            return nullptr;
        }

        // Skip optional leading subdir.
        const CPLString osFileName = poReader->GetFileName();
        if (osFileName.empty() || IsEitherSlash(osFileName.back()))
        {
            if (poReader->GotoNextFile() == FALSE)
            {
                return nullptr;
            }
        }

        if (poReader->GotoNextFile())
        {
            CPLString msg;
            msg.Printf("Support only 1 file in archive file %s when "
                       "no explicit in-archive filename is specified",
                       archiveFilename);
            const VSIArchiveContent *content =
                GetContentOfArchive(archiveFilename, poReader.get());
            if (content)
            {
                msg += "\nYou could try one of the following :\n";
                for (const auto &entry : content->entries)
                {
                    msg += CPLString().Printf("  %s/{%s}/%s\n", GetPrefix(),
                                              archiveFilename,
                                              entry.fileName.c_str());
                }
            }

            CPLError(CE_Failure, CPLE_NotSupported, "%s", msg.c_str());

            return nullptr;
        }
    }
    else
    {
        // Optimization: instead of iterating over all files which can be
        // slow on .tar.gz files, try reading the first one first.
        // This can help if it is really huge.
        {
            std::unique_lock oLock(oMutex);

            if (oFileList.find(archiveFilename) == oFileList.end())
            {
                if (poReader->GotoFirstFile() == FALSE)
                {
                    return nullptr;
                }

                const CPLString osFileName = poReader->GetFileName();
                bool bIsDir = false;
                const CPLString osStrippedFilename =
                    GetStrippedFilename(osFileName, bIsDir);
                if (!osStrippedFilename.empty())
                {
                    const bool bMatch =
                        strcmp(osStrippedFilename, fileInArchiveName) == 0;
                    if (bMatch)
                    {
                        if (bIsDir)
                        {
                            return nullptr;
                        }
                        return poReader;
                    }
                }
            }
        }

        const VSIArchiveEntry *archiveEntry = nullptr;
        if (FindFileInArchive(archiveFilename, fileInArchiveName,
                              &archiveEntry) == FALSE ||
            archiveEntry->bIsDir)
        {
            return nullptr;
        }
        if (!poReader->GotoFileOffset(archiveEntry->file_pos.get()))
        {
            return nullptr;
        }
    }
    return poReader;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIArchiveFilesystemHandler::Stat(const char *pszFilename,
                                      VSIStatBufL *pStatBuf, int nFlags)
{
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    CPLString osFileInArchive;
    auto archiveFilename =
        SplitFilename(pszFilename, osFileInArchive, true,
                      (nFlags & VSI_STAT_SET_ERROR_FLAG) != 0);
    if (archiveFilename == nullptr)
        return -1;

    int ret = -1;
    if (!osFileInArchive.empty())
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("VSIArchive", "Looking for %s %s", archiveFilename.get(),
                 osFileInArchive.c_str());
#endif

        const VSIArchiveEntry *archiveEntry = nullptr;
        if (FindFileInArchive(archiveFilename.get(), osFileInArchive,
                              &archiveEntry))
        {
            // Patching st_size with uncompressed file size.
            pStatBuf->st_size = archiveEntry->uncompressed_size;
            pStatBuf->st_mtime =
                static_cast<time_t>(archiveEntry->nModifiedTime);
            if (archiveEntry->bIsDir)
                pStatBuf->st_mode = S_IFDIR;
            else
                pStatBuf->st_mode = S_IFREG;
            ret = 0;
        }
    }
    else
    {
        auto poReader = CreateReader(archiveFilename.get());

        if (poReader != nullptr && poReader->GotoFirstFile())
        {
            // Skip optional leading subdir.
            const CPLString osFileName = poReader->GetFileName();
            if (IsEitherSlash(osFileName.back()))
            {
                if (poReader->GotoNextFile() == FALSE)
                {
                    return -1;
                }
            }

            if (poReader->GotoNextFile())
            {
                // Several files in archive --> treat as dir.
                pStatBuf->st_size = 0;
                pStatBuf->st_mode = S_IFDIR;
            }
            else
            {
                // Patching st_size with uncompressed file size.
                pStatBuf->st_size = poReader->GetFileSize();
                pStatBuf->st_mtime =
                    static_cast<time_t>(poReader->GetModifiedTime());
                pStatBuf->st_mode = S_IFREG;
            }

            ret = 0;
        }
    }

    return ret;
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char **VSIArchiveFilesystemHandler::ReadDirEx(const char *pszDirname,
                                              int nMaxFiles)
{
    CPLString osInArchiveSubDir;
    auto archiveFilename =
        SplitFilename(pszDirname, osInArchiveSubDir, true, true);
    if (archiveFilename == nullptr)
        return nullptr;

    const size_t lenInArchiveSubDir = osInArchiveSubDir.size();

    CPLStringList oDir;

    const VSIArchiveContent *content =
        GetContentOfArchive(archiveFilename.get());
    if (!content)
    {
        return nullptr;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("VSIArchive", "Read dir %s", pszDirname);
#endif

    std::string searchDir;
    if (lenInArchiveSubDir != 0)
        searchDir = std::move(osInArchiveSubDir);

    // Use directory index to find the list of children for this directory
    auto dirIter = content->dirIndex.find(searchDir);
    if (dirIter == content->dirIndex.end())
    {
        // Directory not found in index - no children
        return oDir.StealList();
    }
    const std::vector<int> &childIndices = dirIter->second;

    // Scan the children of this directory
    for (int childIdx : childIndices)
    {
        const char *fileName = content->entries[childIdx].fileName.c_str();

        const char *baseName = fileName;
        if (lenInArchiveSubDir != 0)
        {
            // Skip the directory prefix and slash to get just the child name
            baseName = fileName + lenInArchiveSubDir + 1;
        }
        oDir.AddStringDirectly(CPLStrdup(baseName));

        if (nMaxFiles > 0 && oDir.Count() > nMaxFiles)
            break;
    }
    return oDir.StealList();
}

/************************************************************************/
/*                              IsLocal()                               */
/************************************************************************/

bool VSIArchiveFilesystemHandler::IsLocal(const char *pszPath) const
{
    if (!STARTS_WITH(pszPath, GetPrefix()))
        return false;
    const char *pszBaseFileName = pszPath + strlen(GetPrefix());
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(pszBaseFileName);
    return poFSHandler->IsLocal(pszPath);
}

/************************************************************************/
/*                             IsArchive()                              */
/************************************************************************/

bool VSIArchiveFilesystemHandler::IsArchive(const char *pszPath) const
{
    if (!STARTS_WITH(pszPath, GetPrefix()))
        return false;
    CPLString osFileInArchive;
    return SplitFilename(pszPath, osFileInArchive, false, false) != nullptr &&
           osFileInArchive.empty();
}

//! @endcond
