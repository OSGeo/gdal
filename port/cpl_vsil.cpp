/******************************************************************************
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation VSI*L File API and other file system access
 *           methods going through file virtualization.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_vsi.h"

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "cpl_vsil_curl_class.h"

// To avoid aliasing to GetDiskFreeSpace to GetDiskFreeSpaceA on Windows
#ifdef GetDiskFreeSpace
#undef GetDiskFreeSpace
#endif

/************************************************************************/
/*                             VSIReadDir()                             */
/************************************************************************/

/**
 * \brief Read names in a directory.
 *
 * This function abstracts access to directory contains.  It returns a
 * list of strings containing the names of files, and directories in this
 * directory.  The resulting string list becomes the responsibility of the
 * application and should be freed with CSLDestroy() when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * This function used to be known as CPLReadDir(), but the old name is now
 * deprecated.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @return The list of entries in the directory, or NULL if the directory
 * doesn't exist.  Filenames are returned in UTF-8 encoding.
 */

char **VSIReadDir(const char *pszPath)
{
    return VSIReadDirEx(pszPath, 0);
}

/************************************************************************/
/*                             VSIReadDirEx()                           */
/************************************************************************/

/**
 * \brief Read names in a directory.
 *
 * This function abstracts access to directory contains.  It returns a
 * list of strings containing the names of files, and directories in this
 * directory.  The resulting string list becomes the responsibility of the
 * application and should be freed with CSLDestroy() when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * If nMaxFiles is set to a positive number, directory listing will stop after
 * that limit has been reached. Note that to indicate truncate, at least one
 * element more than the nMaxFiles limit will be returned. If CSLCount() on the
 * result is lesser or equal to nMaxFiles, then no truncation occurred.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @param nMaxFiles maximum number of files after which to stop, or 0 for no
 * limit.
 * @return The list of entries in the directory, or NULL if the directory
 * doesn't exist.  Filenames are returned in UTF-8 encoding.
 * @since GDAL 2.1
 */

char **VSIReadDirEx(const char *pszPath, int nMaxFiles)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->ReadDirEx(pszPath, nMaxFiles);
}

/************************************************************************/
/*                             VSISiblingFiles()                        */
/************************************************************************/

/**
 * \brief Return related filenames
 *
 * This function is essentially meant at being used by GDAL internals.
 *
 * @param pszFilename the path of a filename to inspect
 * UTF-8 encoded.
 * @return The list of entries, relative to the directory, of all sidecar
 * files available or NULL if the list is not known.
 * Filenames are returned in UTF-8 encoding.
 * Most implementations will return NULL, and a subsequent ReadDir will
 * list all files available in the file's directory. This function will be
 * overridden by VSI FilesystemHandlers that wish to force e.g. an empty list
 * to avoid opening non-existent files on slow filesystems. The return value
 * shall be destroyed with CSLDestroy()
 * @since GDAL 3.2
 */
char **VSISiblingFiles(const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->SiblingFiles(pszFilename);
}

/************************************************************************/
/*                           VSIFnMatch()                               */
/************************************************************************/

static bool VSIFnMatch(const char *pszPattern, const char *pszStr)
{
    for (; *pszPattern && *pszStr; pszPattern++, pszStr++)
    {
        if (*pszPattern == '*')
        {
            if (pszPattern[1] == 0)
                return true;
            for (; *pszStr; ++pszStr)
            {
                if (VSIFnMatch(pszPattern + 1, pszStr))
                    return true;
            }
            return false;
        }
        else if (*pszPattern == '?')
        {
            // match single any char
        }
        else if (*pszPattern == '[')
        {
            // match character classes and ranges
            // "[abcd]" will match a character that is a, b, c or d
            // "[a-z]" will match a character that is a to z
            // "[!abcd] will match a character that is *not* a, b, c or d
            // "[]]" will match character ]
            // "[]-]" will match character ] or -
            // "[!]a-]" will match a character that is *not* ], a or -

            const char *pszOpenBracket = pszPattern;
            ++pszPattern;
            const bool isNot = (*pszPattern == '!');
            if (isNot)
            {
                ++pszOpenBracket;
                ++pszPattern;
            }
            bool res = false;
            for (; *pszPattern; ++pszPattern)
            {
                if ((*pszPattern == ']' || *pszPattern == '-') &&
                    pszPattern == pszOpenBracket + 1)
                {
                    if (*pszStr == *pszPattern)
                    {
                        res = true;
                    }
                }
                else if (*pszPattern == ']')
                {
                    break;
                }
                else if (pszPattern[1] == '-' && pszPattern[2] != 0 &&
                         pszPattern[2] != ']')
                {
                    if (*pszStr >= pszPattern[0] && *pszStr <= pszPattern[2])
                    {
                        res = true;
                    }
                    pszPattern += 2;
                }
                else if (*pszStr == *pszPattern)
                {
                    res = true;
                }
            }
            if (*pszPattern == 0)
                return false;
            if (!res && !isNot)
                return false;
            if (res && isNot)
                return false;
        }
        else if (*pszPattern != *pszStr)
        {
            return false;
        }
    }
    return *pszPattern == 0 && *pszStr == 0;
}

/************************************************************************/
/*                             VSIGlob()                                */
/************************************************************************/

/**
 \brief Return a list of file and directory names matching
 a pattern that can contain wildcards.

 This function has similar behavior to the POSIX glob() function:
 https://man7.org/linux/man-pages/man7/glob.7.html

 In particular it supports the following wildcards:
 <ul>
 <li>'*': match any string</li>
 <li>'?': match any single character</li>
 <li>'[': match character class or range, with '!' immediately after '['
 to indicate negation.</li>
 </ul>
 Refer to to the above man page for more details.

 It also supports the "**" recursive wildcard, behaving similarly to Python
 glob.glob() with recursive=True. Be careful of the amount of memory and time
 required when using that recursive wildcard on directories with a large
 amount of files and subdirectories.

 Examples, given a file hierarchy:
 - one.tif
 - my_subdir/two.tif
 - my_subdir/subsubdir/three.tif

 \code{.cpp}
 VSIGlob("one.tif",NULL,NULL,NULL) returns ["one.tif", NULL]
 VSIGlob("*.tif",NULL,NULL,NULL) returns ["one.tif", NULL]
 VSIGlob("on?.tif",NULL,NULL,NULL) returns ["one.tif", NULL]
 VSIGlob("on[a-z].tif",NULL,NULL,NULL) returns ["one.tif", NULL]
 VSIGlob("on[ef].tif",NULL,NULL,NULL) returns ["one.tif", NULL]
 VSIGlob("on[!e].tif",NULL,NULL,NULL) returns NULL
 VSIGlob("my_subdir" "/" "*.tif",NULL,NULL,NULL) returns ["my_subdir/two.tif", NULL]
 VSIGlob("**" "/" "*.tif",NULL,NULL,NULL) returns ["one.tif", "my_subdir/two.tif", "my_subdir/subsubdir/three.tif", NULL]
 \endcode

 In the current implementation, matching is done based on the assumption that
 a character fits into a single byte, which will not work properly on
 non-ASCII UTF-8 filenames.

 VSIGlob() works with any virtual file systems supported by GDAL, including
 network file systems such as /vsis3/, /vsigs/, /vsiaz/, etc. But note that
 for those ones, the pattern is not passed to the remote server, and thus large
 amount of filenames can be transferred from the remote server to the host
 where the filtering is done.

 @param pszPattern the relative, or absolute path of a directory to read.
 UTF-8 encoded.
 @param papszOptions NULL-terminate list of options, or NULL. None supported
 currently.
 @param pProgressFunc Progress function, or NULL. This is only used as a way
 for the user to cancel operation if it takes too much time. The percentage
 passed to the callback is not significant (always at 0).
 @param pProgressData User data passed to the progress function, or NULL.
 @return The list of matched filenames, which must be freed with CSLDestroy().
 Filenames are returned in UTF-8 encoding.

 @since GDAL 3.11
*/

char **VSIGlob(const char *pszPattern, const char *const *papszOptions,
               GDALProgressFunc pProgressFunc, void *pProgressData)
{
    CPL_IGNORE_RET_VAL(papszOptions);

    CPLStringList aosRes;
    std::vector<std::pair<std::string, size_t>> candidates;
    candidates.emplace_back(pszPattern, 0);
    while (!candidates.empty())
    {
        auto [osPattern, nPosStart] = candidates.back();
        pszPattern = osPattern.c_str() + nPosStart;
        candidates.pop_back();

        std::string osPath = osPattern.substr(0, nPosStart);
        std::string osCurPath;
        for (;; ++pszPattern)
        {
            if (*pszPattern == 0 || *pszPattern == '/' || *pszPattern == '\\')
            {
                struct VSIDirCloser
                {
                    void operator()(VSIDIR *dir)
                    {
                        VSICloseDir(dir);
                    }
                };

                if (osCurPath == "**")
                {
                    std::unique_ptr<VSIDIR, VSIDirCloser> psDir(
                        VSIOpenDir(osPath.c_str(), -1, nullptr));
                    if (!psDir)
                        return nullptr;
                    while (const VSIDIREntry *psEntry =
                               VSIGetNextDirEntry(psDir.get()))
                    {
                        if (pProgressFunc &&
                            !pProgressFunc(0, "", pProgressData))
                        {
                            return nullptr;
                        }
                        {
                            std::string osCandidate(osPath);
                            osCandidate += psEntry->pszName;
                            nPosStart = osCandidate.size();
                            if (*pszPattern)
                            {
                                osCandidate += pszPattern;
                            }
                            candidates.emplace_back(std::move(osCandidate),
                                                    nPosStart);
                        }
                    }
                    osPath.clear();
                    break;
                }
                else if (osCurPath.find_first_of("*?[") != std::string::npos)
                {
                    std::unique_ptr<VSIDIR, VSIDirCloser> psDir(
                        VSIOpenDir(osPath.c_str(), 0, nullptr));
                    if (!psDir)
                        return nullptr;
                    while (const VSIDIREntry *psEntry =
                               VSIGetNextDirEntry(psDir.get()))
                    {
                        if (pProgressFunc &&
                            !pProgressFunc(0, "", pProgressData))
                        {
                            return nullptr;
                        }
                        if (VSIFnMatch(osCurPath.c_str(), psEntry->pszName))
                        {
                            std::string osCandidate(osPath);
                            osCandidate += psEntry->pszName;
                            nPosStart = osCandidate.size();
                            if (*pszPattern)
                            {
                                osCandidate += pszPattern;
                            }
                            candidates.emplace_back(std::move(osCandidate),
                                                    nPosStart);
                        }
                    }
                    osPath.clear();
                    break;
                }
                else if (*pszPattern == 0)
                {
                    osPath += osCurPath;
                    break;
                }
                else
                {
                    osPath += osCurPath;
                    osPath += *pszPattern;
                    osCurPath.clear();
                }
            }
            else
            {
                osCurPath += *pszPattern;
            }
        }
        if (!osPath.empty())
        {
            VSIStatBufL sStat;
            if (VSIStatL(osPath.c_str(), &sStat) == 0)
                aosRes.AddString(osPath.c_str());
        }
    }

    return aosRes.StealList();
}

/************************************************************************/
/*                      VSIGetDirectorySeparator()                      */
/************************************************************************/

/** Return the directory separator for the specified path.
 *
 * Default is forward slash. The only exception currently is the Windows
 * file system which returns backslash, unless the specified path is of the
 * form "{drive_letter}:/{rest_of_the_path}".
 *
 * @since 3.9
 */
const char *VSIGetDirectorySeparator(const char *pszPath)
{
    if (STARTS_WITH(pszPath, "http://") || STARTS_WITH(pszPath, "https://"))
        return "/";

    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);
    return poFSHandler->GetDirectorySeparator(pszPath);
}

/************************************************************************/
/*                             VSIReadRecursive()                       */
/************************************************************************/

/**
 * \brief Read names in a directory recursively.
 *
 * This function abstracts access to directory contents and subdirectories.
 * It returns a list of strings containing the names of files and directories
 * in this directory and all subdirectories.  The resulting string list becomes
 * the responsibility of the application and should be freed with CSLDestroy()
 * when no longer needed.
 *
 * Note that no error is issued via CPLError() if the directory path is
 * invalid, though NULL is returned.
 *
 * Note: since GDAL 3.9, for recursive mode, the directory separator will no
 * longer be always forward slash, but will be the one returned by
 * VSIGetDirectorySeparator(pszPathIn), so potentially backslash on Windows
 * file systems.
 *
 * @param pszPathIn the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 *
 * @return The list of entries in the directory and subdirectories
 * or NULL if the directory doesn't exist.  Filenames are returned in UTF-8
 * encoding.
 * @since GDAL 1.10.0
 *
 */

char **VSIReadDirRecursive(const char *pszPathIn)
{
    const char SEP = VSIGetDirectorySeparator(pszPathIn)[0];

    const char *const apszOptions[] = {"NAME_AND_TYPE_ONLY=YES", nullptr};
    VSIDIR *psDir = VSIOpenDir(pszPathIn, -1, apszOptions);
    if (!psDir)
        return nullptr;
    CPLStringList oFiles;
    while (auto psEntry = VSIGetNextDirEntry(psDir))
    {
        if (VSI_ISDIR(psEntry->nMode) && psEntry->pszName[0] &&
            psEntry->pszName[strlen(psEntry->pszName) - 1] != SEP)
        {
            oFiles.AddString((std::string(psEntry->pszName) + SEP).c_str());
        }
        else
        {
            oFiles.AddString(psEntry->pszName);
        }
    }
    VSICloseDir(psDir);

    return oFiles.StealList();
}

/************************************************************************/
/*                             CPLReadDir()                             */
/*                                                                      */
/*      This is present only to provide ABI compatibility with older    */
/*      versions.                                                       */
/************************************************************************/
#undef CPLReadDir

CPL_C_START
char CPL_DLL **CPLReadDir(const char *pszPath);
CPL_C_END

char **CPLReadDir(const char *pszPath)
{
    return VSIReadDir(pszPath);
}

/************************************************************************/
/*                             VSIOpenDir()                             */
/************************************************************************/

/**
 * \brief Open a directory to read its entries.
 *
 * This function is close to the POSIX opendir() function.
 *
 * For /vsis3/, /vsigs/, /vsioss/, /vsiaz/ and /vsiadls/, this function has an
 * efficient implementation, minimizing the number of network requests, when
 * invoked with nRecurseDepth <= 0.
 *
 * Entries are read by calling VSIGetNextDirEntry() on the handled returned by
 * that function, until it returns NULL. VSICloseDir() must be called once done
 * with the returned directory handle.
 *
 * @param pszPath the relative, or absolute path of a directory to read.
 * UTF-8 encoded.
 * @param nRecurseDepth 0 means do not recurse in subdirectories, 1 means
 * recurse only in the first level of subdirectories, etc. -1 means unlimited
 * recursion level
 * @param papszOptions NULL terminated list of options, or NULL. The following
 * options are implemented:
 * <ul>
 * <li>PREFIX=string: (GDAL >= 3.4) Filter to select filenames only starting
 *     with the specified prefix. Implemented efficiently for /vsis3/, /vsigs/,
 *     and /vsiaz/ (but not /vsiadls/)
 * </li>
 * <li>NAME_AND_TYPE_ONLY=YES/NO: (GDAL >= 3.4) Defaults to NO. If set to YES,
 *     only the pszName and nMode members of VSIDIR are guaranteed to be set.
 *     This is implemented efficiently for the Unix virtual file system.
 * </li>
 * </ul>
 *
 * @return a handle, or NULL in case of error
 * @since GDAL 2.4
 *
 */

VSIDIR *VSIOpenDir(const char *pszPath, int nRecurseDepth,
                   const char *const *papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->OpenDir(pszPath, nRecurseDepth, papszOptions);
}

/************************************************************************/
/*                          VSIGetNextDirEntry()                        */
/************************************************************************/

/**
 * \brief Return the next entry of the directory
 *
 * This function is close to the POSIX readdir() function. It actually returns
 * more information (file size, last modification time), which on 'real' file
 * systems involve one 'stat' call per file.
 *
 * For filesystems that can have both a regular file and a directory name of
 * the same name (typically /vsis3/), when this situation of duplicate happens,
 * the directory name will be suffixed by a slash character. Otherwise directory
 * names are not suffixed by slash.
 *
 * The returned entry remains valid until the next call to VSINextDirEntry()
 * or VSICloseDir() with the same handle.
 *
 * Note: since GDAL 3.9, for recursive mode, the directory separator will no
 * longer be always forward slash, but will be the one returned by
 * VSIGetDirectorySeparator(pszPathIn), so potentially backslash on Windows
 * file systems.
 *
 * @param dir Directory handled returned by VSIOpenDir(). Must not be NULL.
 *
 * @return a entry, or NULL if there is no more entry in the directory. This
 * return value must not be freed.
 * @since GDAL 2.4
 *
 */

const VSIDIREntry *VSIGetNextDirEntry(VSIDIR *dir)
{
    return dir->NextDirEntry();
}

/************************************************************************/
/*                             VSICloseDir()                            */
/************************************************************************/

/**
 * \brief Close a directory
 *
 * This function is close to the POSIX closedir() function.
 *
 * @param dir Directory handled returned by VSIOpenDir().
 *
 * @since GDAL 2.4
 */

void VSICloseDir(VSIDIR *dir)
{
    delete dir;
}

/************************************************************************/
/*                              VSIMkdir()                              */
/************************************************************************/

/**
 * \brief Create a directory.
 *
 * Create a new directory with the indicated mode. For POSIX-style systems,
 * the mode is modified by the file creation mask (umask). However, some
 * file systems and platforms may not use umask, or they may ignore the mode
 * completely. So a reasonable cross-platform default mode value is 0755.
 *
 * Analog of the POSIX mkdir() function.
 *
 * @param pszPathname the path to the directory to create. UTF-8 encoded.
 * @param mode the permissions mode.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIMkdir(const char *pszPathname, long mode)

{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPathname);

    return poFSHandler->Mkdir(pszPathname, mode);
}

/************************************************************************/
/*                       VSIMkdirRecursive()                            */
/************************************************************************/

/**
 * \brief Create a directory and all its ancestors
 *
 * @param pszPathname the path to the directory to create. UTF-8 encoded.
 * @param mode the permissions mode.
 *
 * @return 0 on success or -1 on an error.
 * @since GDAL 2.3
 */

int VSIMkdirRecursive(const char *pszPathname, long mode)
{
    if (pszPathname == nullptr || pszPathname[0] == '\0' ||
        strncmp("/", pszPathname, 2) == 0)
    {
        return -1;
    }

    const CPLString osPathname(pszPathname);
    VSIStatBufL sStat;
    if (VSIStatL(osPathname, &sStat) == 0)
    {
        return VSI_ISDIR(sStat.st_mode) ? 0 : -1;
    }
    const std::string osParentPath(CPLGetPathSafe(osPathname));

    // Prevent crazy paths from recursing forever.
    if (osParentPath == osPathname ||
        osParentPath.length() >= osPathname.length())
    {
        return -1;
    }

    if (!osParentPath.empty() && VSIStatL(osParentPath.c_str(), &sStat) != 0)
    {
        if (VSIMkdirRecursive(osParentPath.c_str(), mode) != 0)
            return -1;
    }

    return VSIMkdir(osPathname, mode);
}

/************************************************************************/
/*                             VSIUnlink()                              */
/************************************************************************/

/**
 * \brief Delete a file.
 *
 * Deletes a file object from the file system.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX unlink() function.
 *
 * @param pszFilename the path of the file to be deleted. UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIUnlink(const char *pszFilename)

{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->Unlink(pszFilename);
}

/************************************************************************/
/*                           VSIUnlinkBatch()                           */
/************************************************************************/

/**
 * \brief Delete several files, possibly in a batch.
 *
 * All files should belong to the same file system handler.
 *
 * This is implemented efficiently for /vsis3/ and /vsigs/ (provided for /vsigs/
 * that OAuth2 authentication is used).
 *
 * @param papszFiles NULL terminated list of files. UTF-8 encoded.
 *
 * @return an array of size CSLCount(papszFiles), whose values are TRUE or FALSE
 * depending on the success of deletion of the corresponding file. The array
 * should be freed with VSIFree().
 * NULL might be return in case of a more general error (for example,
 * files belonging to different file system handlers)
 *
 * @since GDAL 3.1
 */

int *VSIUnlinkBatch(CSLConstList papszFiles)
{
    VSIFilesystemHandler *poFSHandler = nullptr;
    for (CSLConstList papszIter = papszFiles; papszIter && *papszIter;
         ++papszIter)
    {
        auto poFSHandlerThisFile = VSIFileManager::GetHandler(*papszIter);
        if (poFSHandler == nullptr)
            poFSHandler = poFSHandlerThisFile;
        else if (poFSHandler != poFSHandlerThisFile)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Files belong to different file system handlers");
            poFSHandler = nullptr;
            break;
        }
    }
    if (poFSHandler == nullptr)
        return nullptr;
    return poFSHandler->UnlinkBatch(papszFiles);
}

/************************************************************************/
/*                             VSIRename()                              */
/************************************************************************/

/**
 * \brief Rename a file.
 *
 * Renames a file object in the file system.  It should be possible
 * to rename a file onto a new directory, but it is safest if this
 * function is only used to rename files that remain in the same directory.
 *
 * This function only works if the new path is located on the same VSI
 * virtual file system than the old path. I not, use VSIMove() instead.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory or cloud object storage.
 * Note that for cloud object storage, renaming a directory may involve
 * renaming all files it contains recursively, and is thus not an atomic
 * operation (and could be expensive on directories with many files!)
 *
 * Analog of the POSIX rename() function.
 *
 * @param oldpath the name of the file to be renamed.  UTF-8 encoded.
 * @param newpath the name the file should be given.  UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIRename(const char *oldpath, const char *newpath)

{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(oldpath);

    return poFSHandler->Rename(oldpath, newpath, nullptr, nullptr);
}

/************************************************************************/
/*                             VSIMove()                                */
/************************************************************************/

/**
 * \brief Move (or rename) a file.
 *
 * If the new path is an existing directory, the file will be moved to it.
 *
 * The function can work even if the files are not located on the same VSI
 * virtual file system, but it will involve copying and deletion.
 *
 * Note that for cloud object storage, moving/renaming a directory may involve
 * renaming all files it contains recursively, and is thus not an atomic
 * operation (and could be slow and expensive on directories with many files!)
 *
 * @param oldpath the path of the file to be renamed/moved.  UTF-8 encoded.
 * @param newpath the new path the file should be given.  UTF-8 encoded.
 * @param papszOptions Null terminated list of options, or NULL.
 * @param pProgressFunc Progress callback, or NULL.
 * @param pProgressData User data of progress callback, or NULL.
 *
 * @return 0 on success or -1 on error.
 * @since GDAL 3.11
 */

int VSIMove(const char *oldpath, const char *newpath,
            const char *const *papszOptions, GDALProgressFunc pProgressFunc,
            void *pProgressData)
{

    if (strcmp(oldpath, newpath) == 0)
        return 0;

    VSIFilesystemHandler *poOldFSHandler = VSIFileManager::GetHandler(oldpath);
    VSIFilesystemHandler *poNewFSHandler = VSIFileManager::GetHandler(newpath);

    VSIStatBufL sStat;
    if (VSIStatL(oldpath, &sStat) != 0)
    {
        CPLDebug("VSI", "%s is not a object", oldpath);
        errno = ENOENT;
        return -1;
    }

    std::string sNewpath(newpath);
    VSIStatBufL sStatNew;
    if (VSIStatL(newpath, &sStatNew) == 0 && VSI_ISDIR(sStatNew.st_mode))
    {
        sNewpath =
            CPLFormFilenameSafe(newpath, CPLGetFilename(oldpath), nullptr);
    }

    int ret = 0;

    if (poOldFSHandler == poNewFSHandler)
    {
        ret = poOldFSHandler->Rename(oldpath, sNewpath.c_str(), pProgressFunc,
                                     pProgressData);
        if (ret == 0 && pProgressFunc)
            ret = pProgressFunc(1.0, "", pProgressData) ? 0 : -1;
        return ret;
    }

    if (VSI_ISDIR(sStat.st_mode))
    {
        const CPLStringList aosList(VSIReadDir(oldpath));
        poNewFSHandler->Mkdir(sNewpath.c_str(), 0755);
        bool bFoundFiles = false;
        const int nListSize = aosList.size();
        for (int i = 0; ret == 0 && i < nListSize; i++)
        {
            if (strcmp(aosList[i], ".") != 0 && strcmp(aosList[i], "..") != 0)
            {
                bFoundFiles = true;
                const std::string osSrc =
                    CPLFormFilenameSafe(oldpath, aosList[i], nullptr);
                const std::string osTarget =
                    CPLFormFilenameSafe(sNewpath.c_str(), aosList[i], nullptr);
                void *pScaledProgress = GDALCreateScaledProgress(
                    static_cast<double>(i) / nListSize,
                    static_cast<double>(i + 1) / nListSize, pProgressFunc,
                    pProgressData);
                ret = VSIMove(osSrc.c_str(), osTarget.c_str(), papszOptions,
                              pScaledProgress ? GDALScaledProgress : nullptr,
                              pScaledProgress);
                GDALDestroyScaledProgress(pScaledProgress);
            }
        }
        if (!bFoundFiles)
            ret = VSIStatL(sNewpath.c_str(), &sStat);
        if (ret == 0)
            ret = poOldFSHandler->Rmdir(oldpath);
    }
    else
    {
        ret = VSICopyFile(oldpath, sNewpath.c_str(), nullptr, sStat.st_size,
                          nullptr, pProgressFunc, pProgressData) == 0 &&
                      VSIUnlink(oldpath) == 0
                  ? 0
                  : -1;
    }
    if (ret == 0 && pProgressFunc)
        ret = pProgressFunc(1.0, "", pProgressData) ? 0 : -1;
    return ret;
}

/************************************************************************/
/*                             VSICopyFile()                            */
/************************************************************************/

/**
 * \brief Copy a source file into a target file.
 *
 * For a /vsizip/foo.zip/bar target, the options available are those of
 * CPLAddFileInZip()
 *
 * The following copies are made fully on the target server, without local
 * download from source and upload to target:
 * - /vsis3/ -> /vsis3/
 * - /vsigs/ -> /vsigs/
 * - /vsiaz/ -> /vsiaz/
 * - /vsiadls/ -> /vsiadls/
 * - any of the above or /vsicurl/ -> /vsiaz/ (starting with GDAL 3.8)
 *
 * @param pszSource Source filename. UTF-8 encoded. May be NULL if fpSource is
 * not NULL.
 * @param pszTarget Target filename.  UTF-8 encoded. Must not be NULL
 * @param fpSource File handle on the source file. May be NULL if pszSource is
 * not NULL.
 * @param nSourceSize Size of the source file. Pass -1 if unknown.
 * If set to -1, and progress callback is used, VSIStatL() will be used on
 * pszSource to retrieve the source size.
 * @param papszOptions Null terminated list of options, or NULL.
 * @param pProgressFunc Progress callback, or NULL.
 * @param pProgressData User data of progress callback, or NULL.
 *
 * @return 0 on success.
 * @since GDAL 3.7
 */

int VSICopyFile(const char *pszSource, const char *pszTarget,
                VSILFILE *fpSource, vsi_l_offset nSourceSize,
                const char *const *papszOptions, GDALProgressFunc pProgressFunc,
                void *pProgressData)

{
    if (!pszSource && !fpSource)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pszSource == nullptr && fpSource == nullptr");
        return -1;
    }
    if (!pszTarget || pszTarget[0] == '\0')
    {
        return -1;
    }

    VSIFilesystemHandler *poFSHandlerTarget =
        VSIFileManager::GetHandler(pszTarget);
    return poFSHandlerTarget->CopyFile(pszSource, pszTarget, fpSource,
                                       nSourceSize, papszOptions, pProgressFunc,
                                       pProgressData);
}

/************************************************************************/
/*                       VSICopyFileRestartable()                       */
/************************************************************************/

/**
 \brief Copy a source file into a target file in a way that can (potentially)
 be restarted.

 This function provides the possibility of efficiently restarting upload of
 large files to cloud storage that implements upload in a chunked way,
 such as /vsis3/ and /vsigs/.
 For other destination file systems, this function may fallback to
 VSICopyFile() and not provide any smart restartable implementation.

 Example of a potential workflow:

 @code{.cpp}
 char* pszOutputPayload = NULL;
 int ret = VSICopyFileRestartable(pszSource, pszTarget, NULL,
                                  &pszOutputPayload, NULL, NULL, NULL);
 while( ret == 1 ) // add also a limiting counter to avoid potentiall endless looping
 {
     // TODO: wait for some time

     char* pszOutputPayloadNew = NULL;
     const char* pszInputPayload = pszOutputPayload;
     ret = VSICopyFileRestartable(pszSource, pszTarget, pszInputPayload,
                                  &pszOutputPayloadNew, NULL, NULL, NULL);
     VSIFree(pszOutputPayload);
     pszOutputPayload = pszOutputPayloadNew;
 }
 VSIFree(pszOutputPayload);
 @endcode

 @param pszSource Source filename. UTF-8 encoded. Must not be NULL
 @param pszTarget Target filename. UTF-8 encoded. Must not be NULL
 @param pszInputPayload NULL at the first invocation. When doing a retry,
                        should be the content of *ppszOutputPayload from a
                        previous invocation.
 @param[out] ppszOutputPayload Pointer to an output string that will be set to
                               a value that can be provided as pszInputPayload
                               for a next call to VSICopyFileRestartable().
                               ppszOutputPayload must not be NULL.
                               The string set in *ppszOutputPayload, if not NULL,
                               is JSON-encoded, and can be re-used in another
                               process instance. It must be freed with VSIFree()
                               when no longer needed.
 @param papszOptions Null terminated list of options, or NULL.
 Currently accepted options are:
 <ul>
 <li>NUM_THREADS=integer or ALL_CPUS. Number of threads to use for parallel
 file copying. Only use for when /vsis3/, /vsigs/, /vsiaz/ or /vsiadls/ is in
 source or target. The default is 10.
 </li>
 <li>CHUNK_SIZE=integer. Maximum size of chunk (in bytes) to use
 to split large objects. For upload to /vsis3/, this chunk size must be set at
 least to 5 MB. The default is 50 MB.
 </li>
 </ul>
 @param pProgressFunc Progress callback, or NULL.
 @param pProgressData User data of progress callback, or NULL.
 @return 0 on success,
         -1 on (non-restartable) failure,
         1 if VSICopyFileRestartable() can be called again in a restartable way
 @since GDAL 3.10

 @see VSIAbortPendingUploads()
*/

int VSICopyFileRestartable(const char *pszSource, const char *pszTarget,
                           const char *pszInputPayload,
                           char **ppszOutputPayload,
                           const char *const *papszOptions,
                           GDALProgressFunc pProgressFunc, void *pProgressData)

{
    if (!pszSource)
    {
        return -1;
    }
    if (!pszTarget || pszTarget[0] == '\0')
    {
        return -1;
    }
    if (!ppszOutputPayload)
    {
        return -1;
    }

    VSIFilesystemHandler *poFSHandlerTarget =
        VSIFileManager::GetHandler(pszTarget);
    return poFSHandlerTarget->CopyFileRestartable(
        pszSource, pszTarget, pszInputPayload, ppszOutputPayload, papszOptions,
        pProgressFunc, pProgressData);
}

/************************************************************************/
/*                             VSISync()                                */
/************************************************************************/

/**
 * \brief Synchronize a source file/directory with a target file/directory.
 *
 * This is a analog of the 'rsync' utility. In the current implementation,
 * rsync would be more efficient for local file copying, but VSISync() main
 * interest is when the source or target is a remote
 * file system like /vsis3/ or /vsigs/, in which case it can take into account
 * the timestamps of the files (or optionally the ETag/MD5Sum) to avoid
 * unneeded copy operations.
 *
 * This is only implemented efficiently for:
 * <ul>
 * <li> local filesystem <--> remote filesystem.</li>
 * <li> remote filesystem <--> remote filesystem (starting with GDAL 3.1).
 * Where the source and target remote filesystems are the same and one of
 * /vsis3/, /vsigs/ or /vsiaz/. Or when the target is /vsiaz/ and the source
 * is /vsis3/, /vsigs/, /vsiadls/ or /vsicurl/ (starting with GDAL 3.8)</li>
 * </ul>
 *
 * Similarly to rsync behavior, if the source filename ends with a slash,
 * it means that the content of the directory must be copied, but not the
 * directory name. For example, assuming "/home/even/foo" contains a file "bar",
 * VSISync("/home/even/foo/", "/mnt/media", ...) will create a "/mnt/media/bar"
 * file. Whereas VSISync("/home/even/foo", "/mnt/media", ...) will create a
 * "/mnt/media/foo" directory which contains a bar file.
 *
 * @param pszSource Source file or directory.  UTF-8 encoded.
 * @param pszTarget Target file or directory.  UTF-8 encoded.
 * @param papszOptions Null terminated list of options, or NULL.
 * Currently accepted options are:
 * <ul>
 * <li>RECURSIVE=NO (the default is YES)</li>
 * <li>SYNC_STRATEGY=TIMESTAMP/ETAG/OVERWRITE.
 *
 *     Determines which criterion is used to determine if a target file must be
 *     replaced when it already exists and has the same file size as the source.
 *     Only applies for a source or target being a network filesystem.
 *
 *     The default is TIMESTAMP (similarly to how 'aws s3 sync' works), that is
 *     to say that for an upload operation, a remote file is
 *     replaced if it has a different size or if it is older than the source.
 *     For a download operation, a local file is  replaced if it has a different
 *     size or if it is newer than the remote file.
 *
 *     The ETAG strategy assumes that the ETag metadata of the remote file is
 *     the MD5Sum of the file content, which is only true in the case of /vsis3/
 *     for files not using KMS server side encryption and uploaded in a single
 *     PUT operation (so smaller than 50 MB given the default used by GDAL).
 *     Only to be used for /vsis3/, /vsigs/ or other filesystems using a
 *     MD5Sum as ETAG.
 *
 *     The OVERWRITE strategy (GDAL >= 3.2) will always overwrite the target
 *     file with the source one.
 * </li>
 * <li>NUM_THREADS=integer. (GDAL >= 3.1) Number of threads to use for parallel
 * file copying. Only use for when /vsis3/, /vsigs/, /vsiaz/ or /vsiadls/ is in
 * source or target. The default is 10 since GDAL 3.3</li>
 * <li>CHUNK_SIZE=integer. (GDAL >= 3.1) Maximum size of chunk (in bytes) to use
 * to split large objects when downloading them from /vsis3/, /vsigs/, /vsiaz/
 * or /vsiadls/ to local file system, or for upload to /vsis3/, /vsiaz/ or
 * /vsiadls/ from local file system. Only used if NUM_THREADS > 1. For upload to
 * /vsis3/, this chunk size must be set at least to 5 MB. The default is 8 MB
 * since GDAL 3.3</li> <li>x-amz-KEY=value. (GDAL >= 3.5) MIME header to pass
 * during creation of a /vsis3/ object.</li> <li>x-goog-KEY=value. (GDAL >= 3.5)
 * MIME header to pass during creation of a /vsigs/ object.</li>
 * <li>x-ms-KEY=value. (GDAL >= 3.5) MIME header to pass during creation of a
 * /vsiaz/ or /vsiadls/ object.</li>
 * </ul>
 * @param pProgressFunc Progress callback, or NULL.
 * @param pProgressData User data of progress callback, or NULL.
 * @param ppapszOutputs Unused. Should be set to NULL for now.
 *
 * @return TRUE on success or FALSE on an error.
 * @since GDAL 2.4
 */

int VSISync(const char *pszSource, const char *pszTarget,
            const char *const *papszOptions, GDALProgressFunc pProgressFunc,
            void *pProgressData, char ***ppapszOutputs)

{
    if (pszSource[0] == '\0' || pszTarget[0] == '\0')
    {
        return FALSE;
    }

    VSIFilesystemHandler *poFSHandlerSource =
        VSIFileManager::GetHandler(pszSource);
    VSIFilesystemHandler *poFSHandlerTarget =
        VSIFileManager::GetHandler(pszTarget);
    VSIFilesystemHandler *poFSHandlerLocal = VSIFileManager::GetHandler("");
    VSIFilesystemHandler *poFSHandlerMem =
        VSIFileManager::GetHandler("/vsimem/");
    VSIFilesystemHandler *poFSHandler = poFSHandlerSource;
    if (poFSHandlerTarget != poFSHandlerLocal &&
        poFSHandlerTarget != poFSHandlerMem)
    {
        poFSHandler = poFSHandlerTarget;
    }

    return poFSHandler->Sync(pszSource, pszTarget, papszOptions, pProgressFunc,
                             pProgressData, ppapszOutputs)
               ? TRUE
               : FALSE;
}

/************************************************************************/
/*                    VSIMultipartUploadGetCapabilities()               */
/************************************************************************/

/**
 * \brief Return capabilities for multiple part file upload.
 *
 * @param pszFilename Filename, or virtual file system prefix, onto which
 * capabilities should apply.
 * @param[out] pbNonSequentialUploadSupported If not null,
 * the pointed value is set if parts can be uploaded in a non-sequential way.
 * @param[out] pbParallelUploadSupported If not null,
 * the pointed value is set if parts can be uploaded in a parallel way.
 * (implies *pbNonSequentialUploadSupported = true)
 * @param[out] pbAbortSupported If not null,
 * the pointed value is set if VSIMultipartUploadAbort() is implemented.
 * @param[out] pnMinPartSize If not null, the pointed value is set to the minimum
 * size of parts (but the last one), in MiB.
 * @param[out] pnMaxPartSize If not null, the pointed value is set to the maximum
 * size of parts, in MiB.
 * @param[out] pnMaxPartCount  If not null, the pointed value is set to the
 * maximum number of parts that can be uploaded.
 *
 * @return TRUE in case of success, FALSE otherwise.
 *
 * @since 3.10
 */
int VSIMultipartUploadGetCapabilities(
    const char *pszFilename, int *pbNonSequentialUploadSupported,
    int *pbParallelUploadSupported, int *pbAbortSupported,
    size_t *pnMinPartSize, size_t *pnMaxPartSize, int *pnMaxPartCount)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->MultipartUploadGetCapabilities(
        pbNonSequentialUploadSupported, pbParallelUploadSupported,
        pbAbortSupported, pnMinPartSize, pnMaxPartSize, pnMaxPartCount);
}

/************************************************************************/
/*                     VSIMultipartUploadStart()                        */
/************************************************************************/

/**
 * \brief Initiates the upload a (big) file in a piece-wise way.
 *
 * Using this API directly is generally not needed, but in very advanced cases,
 * as VSIFOpenL(..., "wb") + VSIFWriteL(), VSISync(), VSICopyFile() or
 * VSICopyFileRestartable() may be able to leverage it when needed.
 *
 * This is only implemented for the /vsis3/, /vsigs/, /vsiaz/, /vsiadls/ and
 * /vsioss/ virtual file systems.
 *
 * The typical workflow is to do :
 * - VSIMultipartUploadStart()
 * - VSIMultipartUploadAddPart(): several times
 * - VSIMultipartUploadEnd()
 *
 * If VSIMultipartUploadAbort() is supported by the filesystem (VSIMultipartUploadGetCapabilities()
 * can be used to determine it), this function should be called to cancel an
 * upload. This can be needed to avoid extra billing for some cloud storage
 * providers.
 *
 * The following options are supported:
 * <ul>
 * <li>MIME headers such as Content-Type and Content-Encoding
 * are supported for the /vsis3/, /vsigs/, /vsiaz/, /vsiadls/ file systems.</li>
 * </ul>
 *
 * @param pszFilename Filename to create
 * @param papszOptions NULL or null-terminated list of options.
 * @return an upload ID to pass to other VSIMultipartUploadXXXXX() functions,
 * and to free with CPLFree() once done, or nullptr in case of error.
 *
 * @since 3.10
 */
char *VSIMultipartUploadStart(const char *pszFilename,
                              CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->MultipartUploadStart(pszFilename, papszOptions);
}

/************************************************************************/
/*                     VSIMultipartUploadAddPart()                      */
/************************************************************************/

/**
 * \brief Uploads a new part to a multi-part uploaded file.
 *
 * Cf VSIMultipartUploadStart().
 *
 * VSIMultipartUploadGetCapabilities() returns hints on the constraints that
 * apply to the upload, in terms of minimum/maximum size of each part, maximum
 * number of parts, and whether non-sequential or parallel uploads are
 * supported.
 *
 * @param pszFilename Filename to which to append the new part. Should be the
 *                    same as the one used for VSIMultipartUploadStart()
 * @param pszUploadId Value returned by VSIMultipartUploadStart()
 * @param nPartNumber Part number, starting at 1.
 * @param nFileOffset Offset within the file at which (starts at 0) the passed
 *                    data starts.
 * @param pData       Pointer to an array of nDataLength bytes.
 * @param nDataLength Size in bytes of pData.
 * @param papszOptions Unused. Should be nullptr.
 *
 * @return a part identifier that must be passed into the apszPartIds[] array of
 * VSIMultipartUploadEnd(), and to free with CPLFree() once done, or nullptr in
 * case of error.
 *
 * @since 3.10
 */
char *VSIMultipartUploadAddPart(const char *pszFilename,
                                const char *pszUploadId, int nPartNumber,
                                vsi_l_offset nFileOffset, const void *pData,
                                size_t nDataLength, CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->MultipartUploadAddPart(pszFilename, pszUploadId,
                                               nPartNumber, nFileOffset, pData,
                                               nDataLength, papszOptions);
}

/************************************************************************/
/*                       VSIMultipartUploadEnd()                        */
/************************************************************************/

/**
 * \brief Completes a multi-part file upload.
 *
 * Cf VSIMultipartUploadStart().
 *
 * @param pszFilename Filename for which multipart upload should be completed.
 *                    Should be the same as the one used for
 *                    VSIMultipartUploadStart()
 * @param pszUploadId Value returned by VSIMultipartUploadStart()
 * @param nPartIdsCount Number of parts,  andsize of apszPartIds
 * @param apszPartIds Array of part identifiers (as returned by
 *                    VSIMultipartUploadAddPart()), that must be ordered in
 *                    the sequential order of parts, and of size nPartIdsCount.
 * @param nTotalSize  Total size of the file in bytes (must be equal to the sum
 *                    of nDataLength passed to VSIMultipartUploadAddPart())
 * @param papszOptions Unused. Should be nullptr.
 *
 * @return TRUE in case of success, FALSE in case of failure.
 *
 * @since 3.10
 */
int VSIMultipartUploadEnd(const char *pszFilename, const char *pszUploadId,
                          size_t nPartIdsCount, const char *const *apszPartIds,
                          vsi_l_offset nTotalSize, CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->MultipartUploadEnd(pszFilename, pszUploadId,
                                           nPartIdsCount, apszPartIds,
                                           nTotalSize, papszOptions);
}

/************************************************************************/
/*                       VSIMultipartUploadAbort()                      */
/************************************************************************/

/**
 * \brief Aborts a multi-part file upload.
 *
 * Cf VSIMultipartUploadStart().
 *
 * This function is not implemented for all virtual file systems.
 * Use VSIMultipartUploadGetCapabilities() to determine if it is supported.
 *
 * This can be needed to avoid extra billing for some cloud storage providers.
 *
 * @param pszFilename Filename for which multipart upload should be completed.
 *                    Should be the same as the one used for
 *                    VSIMultipartUploadStart()
 * @param pszUploadId Value returned by VSIMultipartUploadStart()
 * @param papszOptions Unused. Should be nullptr.
 *
 * @return TRUE in case of success, FALSE in case of failure.
 *
 * @since 3.10
 */
int VSIMultipartUploadAbort(const char *pszFilename, const char *pszUploadId,
                            CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->MultipartUploadAbort(pszFilename, pszUploadId,
                                             papszOptions);
}

#ifndef DOXYGEN_SKIP

/************************************************************************/
/*                     MultipartUploadGetCapabilities()                 */
/************************************************************************/

bool VSIFilesystemHandler::MultipartUploadGetCapabilities(int *, int *, int *,
                                                          size_t *, size_t *,
                                                          int *)
{
    CPLError(
        CE_Failure, CPLE_NotSupported,
        "MultipartUploadGetCapabilities() not supported by this file system");
    return false;
}

/************************************************************************/
/*                         MultipartUploadStart()                       */
/************************************************************************/

char *VSIFilesystemHandler::MultipartUploadStart(const char *, CSLConstList)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "MultipartUploadStart() not supported by this file system");
    return nullptr;
}

/************************************************************************/
/*                       MultipartUploadAddPart()                       */
/************************************************************************/

char *VSIFilesystemHandler::MultipartUploadAddPart(const char *, const char *,
                                                   int, vsi_l_offset,
                                                   const void *, size_t,
                                                   CSLConstList)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "MultipartUploadAddPart() not supported by this file system");
    return nullptr;
}

/************************************************************************/
/*                         MultipartUploadEnd()                         */
/************************************************************************/

bool VSIFilesystemHandler::MultipartUploadEnd(const char *, const char *,
                                              size_t, const char *const *,
                                              vsi_l_offset, CSLConstList)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "MultipartUploadEnd() not supported by this file system");
    return FALSE;
}

/************************************************************************/
/*                         MultipartUploadAbort()                       */
/************************************************************************/

bool VSIFilesystemHandler::MultipartUploadAbort(const char *, const char *,
                                                CSLConstList)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "MultipartUploadAbort() not supported by this file system");
    return FALSE;
}

#endif

/************************************************************************/
/*                         VSIAbortPendingUploads()                     */
/************************************************************************/

/**
 * \brief Abort all ongoing multi-part uploads.
 *
 * Abort ongoing multi-part uploads on AWS S3 and Google Cloud Storage. This
 * can be used in case a process doing such uploads was killed in a unclean way.
 *
 * This can be needed to avoid extra billing for some cloud storage providers.
 *
 * Without effect on other virtual file systems.
 *
 * VSIMultipartUploadAbort() can also be used to cancel a given upload, if the
 * upload ID is known.
 *
 * @param pszFilename filename or prefix of a directory into which multipart
 * uploads must be aborted. This can be the root directory of a bucket.  UTF-8
 * encoded.
 *
 * @return TRUE on success or FALSE on an error.
 * @since GDAL 3.4
 */

int VSIAbortPendingUploads(const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->AbortPendingUploads(pszFilename);
}

/************************************************************************/
/*                              VSIRmdir()                              */
/************************************************************************/

/**
 * \brief Delete a directory.
 *
 * Deletes a directory object from the file system.  On some systems
 * the directory must be empty before it can be deleted.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX rmdir() function.
 *
 * @param pszDirname the path of the directory to be deleted.  UTF-8 encoded.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIRmdir(const char *pszDirname)

{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszDirname);

    return poFSHandler->Rmdir(pszDirname);
}

/************************************************************************/
/*                         VSIRmdirRecursive()                          */
/************************************************************************/

/**
 * \brief Delete a directory recursively
 *
 * Deletes a directory object and its content from the file system.
 *
 * Starting with GDAL 3.1, /vsis3/ has an efficient implementation of this
 * function.
 * Starting with GDAL 3.4, /vsigs/ has an efficient implementation of this
 * function, provided that OAuth2 authentication is used.
 *
 * @return 0 on success or -1 on an error.
 * @since GDAL 2.3
 */

int VSIRmdirRecursive(const char *pszDirname)
{
    if (pszDirname == nullptr || pszDirname[0] == '\0' ||
        strncmp("/", pszDirname, 2) == 0)
    {
        return -1;
    }
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszDirname);
    return poFSHandler->RmdirRecursive(pszDirname);
}

/************************************************************************/
/*                              VSIStatL()                              */
/************************************************************************/

/**
 * \brief Get filesystem object info.
 *
 * Fetches status information about a filesystem object (file, directory, etc).
 * The returned information is placed in the VSIStatBufL structure.   For
 * portability, only use the st_size (size in bytes) and st_mode (file type).
 * This method is similar to VSIStat(), but will work on large files on
 * systems where this requires special calls.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function.
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param psStatBuf the structure to load with information.
 *
 * @return 0 on success or -1 on an error.
 */

int VSIStatL(const char *pszFilename, VSIStatBufL *psStatBuf)

{
    return VSIStatExL(pszFilename, psStatBuf, 0);
}

/************************************************************************/
/*                            VSIStatExL()                              */
/************************************************************************/

/**
 * \brief Get filesystem object info.
 *
 * Fetches status information about a filesystem object (file, directory, etc).
 * The returned information is placed in the VSIStatBufL structure.   For
 * portability, only use the st_size (size in bytes) and st_mode (file type).
 * This method is similar to VSIStat(), but will work on large files on
 * systems where this requires special calls.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX stat() function, with an extra parameter to
 * specify which information is needed, which offers a potential for
 * speed optimizations on specialized and potentially slow virtual
 * filesystem objects (/vsigzip/, /vsicurl/)
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param psStatBuf the structure to load with information.
 * @param nFlags 0 to get all information, or VSI_STAT_EXISTS_FLAG,
 *                 VSI_STAT_NATURE_FLAG, VSI_STAT_SIZE_FLAG,
 * VSI_STAT_SET_ERROR_FLAG, VSI_STAT_CACHE_ONLY or a combination of those to get
 * partial info.
 *
 * @return 0 on success or -1 on an error.
 *
 * @since GDAL 1.8.0
 */

int VSIStatExL(const char *pszFilename, VSIStatBufL *psStatBuf, int nFlags)

{
    char szAltPath[4] = {'\0'};

    // Enable to work on "C:" as if it were "C:\".
    if (pszFilename[0] != '\0' && pszFilename[1] == ':' &&
        pszFilename[2] == '\0')
    {
        szAltPath[0] = pszFilename[0];
        szAltPath[1] = pszFilename[1];
        szAltPath[2] = '\\';
        szAltPath[3] = '\0';

        pszFilename = szAltPath;
    }

    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    if (nFlags == 0)
        nFlags =
            VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG | VSI_STAT_SIZE_FLAG;

    return poFSHandler->Stat(pszFilename, psStatBuf, nFlags);
}

/************************************************************************/
/*                       VSIGetFileMetadata()                           */
/************************************************************************/

/**
 * \brief Get metadata on files.
 *
 * Implemented currently only for network-like filesystems, or starting
 * with GDAL 3.7 for /vsizip/
 *
 * Starting with GDAL 3.11, calling it with pszFilename being the root of a
 * /vsigs/ bucket and pszDomain == nullptr, and when authenticated through
 * OAuth2, will result in returning the result of a "Buckets: get"
 * operation (https://cloud.google.com/storage/docs/json_api/v1/buckets/get),
 * with the keys of the top-level JSON document as keys of the key=value pairs
 * returned by this function.
 *
 * @param pszFilename the path of the filesystem object to be queried.
 * UTF-8 encoded.
 * @param pszDomain Metadata domain to query. Depends on the file system.
 * The following ones are supported:
 * <ul>
 * <li>HEADERS: to get HTTP headers for network-like filesystems (/vsicurl/,
 * /vsis3/, /vsgis/, etc)</li>
 * <li>TAGS:
 *   <ul>
 *     <li>/vsis3/: to get S3 Object tagging information</li>
 *     <li>/vsiaz/: to get blob tags. Refer to
 *     https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob-tags
 *     </li>
 *   </ul>
 * </li>
 * <li>STATUS: specific to /vsiadls/: returns all system defined properties for
 * a path (seems in practice to be a subset of HEADERS)</li> <li>ACL: specific
 * to /vsiadls/ and /vsigs/: returns the access control list for a path. For
 * /vsigs/, a single XML=xml_content string is returned. Refer to
 * https://cloud.google.com/storage/docs/xml-api/get-object-acls
 * </li>
 * <li>METADATA: specific to /vsiaz/: to get blob metadata. Refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/get-blob-metadata.
 * Note: this will be a subset of what pszDomain=HEADERS returns</li>
 * <li>ZIP: specific to /vsizip/: to obtain ZIP specific metadata, in particular
 * if a file is SOZIP-enabled (SOZIP_VALID=YES)</li>
 * </ul>
 * @param papszOptions Unused. Should be set to NULL.
 *
 * @return a NULL-terminated list of key=value strings, to be freed with
 * CSLDestroy() or NULL in case of error / empty list.
 *
 * @since GDAL 3.1.0
 */

char **VSIGetFileMetadata(const char *pszFilename, const char *pszDomain,
                          CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);
    return poFSHandler->GetFileMetadata(pszFilename, pszDomain, papszOptions);
}

/************************************************************************/
/*                       VSISetFileMetadata()                           */
/************************************************************************/

/**
 * \brief Set metadata on files.
 *
 * Implemented currently only for /vsis3/, /vsigs/, /vsiaz/ and /vsiadls/
 *
 * @param pszFilename the path of the filesystem object to be set.
 * UTF-8 encoded.
 * @param papszMetadata NULL-terminated list of key=value strings.
 * @param pszDomain Metadata domain to set. Depends on the file system.
 * The following are supported:
 * <ul>
 * <li>HEADERS: specific to /vsis3/ and /vsigs/: to set HTTP headers, such as
 * "Content-Type", or other file system specific header.
 * For /vsigs/, this also includes: x-goog-meta-{key}={value}. Note that you
 * should specify all metadata to be set, as existing metadata will be
 * overridden.
 * </li>
 * <li>TAGS: Content of papszMetadata should be KEY=VALUE pairs.
 *    <ul>
 *      <li>/vsis3/: to set S3 Object tagging information</li>
 *      <li>/vsiaz/: to set blob tags. Refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-tags.
 * Note: storageV2 must be enabled on the account</li>
 *    </ul>
 * </li>
 * <li>PROPERTIES:
 *    <ul>
 *      <li>to /vsiaz/: to set properties. Refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-properties.</li>
 *      <li>to /vsiadls/: to set properties. Refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/datalakestoragegen2/path/update
 * for headers valid for action=setProperties.</li>
 *    </ul>
 * </li>
 * <li>ACL: specific to /vsiadls/ and /vsigs/: to set access control list.
 * For /vsiadls/, refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/datalakestoragegen2/path/update
 * for headers valid for action=setAccessControl or setAccessControlRecursive.
 * In setAccessControlRecursive, x-ms-acl must be specified in papszMetadata.
 * For /vsigs/, refer to
 * https://cloud.google.com/storage/docs/xml-api/put-object-acls. A single
 * XML=xml_content string should be specified as in papszMetadata.
 * </li>
 * <li>METADATA: specific to /vsiaz/: to set blob metadata. Refer to
 * https://docs.microsoft.com/en-us/rest/api/storageservices/set-blob-metadata.
 * Content of papszMetadata should be strings in the form
 * x-ms-meta-name=value</li>
 * </ul>
 * @param papszOptions NULL or NULL terminated list of options.
 *                     For /vsiadls/ and pszDomain=ACL, "RECURSIVE=TRUE" can be
 *                     set to set the access control list recursively. When
 *                     RECURSIVE=TRUE is set, MODE should also be set to one of
 *                     "set", "modify" or "remove".
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.1.0
 */

int VSISetFileMetadata(const char *pszFilename, CSLConstList papszMetadata,
                       const char *pszDomain, CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);
    return poFSHandler->SetFileMetadata(pszFilename, papszMetadata, pszDomain,
                                        papszOptions)
               ? 1
               : 0;
}

/************************************************************************/
/*                       VSIIsCaseSensitiveFS()                         */
/************************************************************************/

/**
 * \brief Returns if the filenames of the filesystem are case sensitive.
 *
 * This method retrieves to which filesystem belongs the passed filename
 * and return TRUE if the filenames of that filesystem are case sensitive.
 *
 * Currently, this will return FALSE only for Windows real filenames. Other
 * VSI virtual filesystems are case sensitive.
 *
 * This methods avoid ugly \#ifndef _WIN32 / \#endif code, that is wrong when
 * dealing with virtual filenames.
 *
 * @param pszFilename the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the filenames of the filesystem are case sensitive.
 *
 * @since GDAL 1.8.0
 */

int VSIIsCaseSensitiveFS(const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->IsCaseSensitive(pszFilename);
}

/************************************************************************/
/*                       VSISupportsSparseFiles()                       */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports sparse files.
 *
 * Only supported on Linux (and no other Unix derivatives) and
 * Windows.  On Linux, the answer depends on a few hardcoded
 * signatures for common filesystems. Other filesystems will be
 * considered as not supporting sparse files.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the file system is known to support sparse files. FALSE may
 *              be returned both in cases where it is known to not support them,
 *              or when it is unknown.
 *
 * @since GDAL 2.2
 */

int VSISupportsSparseFiles(const char *pszPath)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->SupportsSparseFiles(pszPath);
}

/************************************************************************/
/*                           VSIIsLocal()                               */
/************************************************************************/

/**
 * \brief Returns if the file/filesystem is "local".
 *
 * The concept of local is mostly by opposition with a network / remote
 * file system whose access time can be long.
 *
 * /vsimem/ is considered to be a local file system, although a non-persistent
 * one.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE or FALSE
 *
 * @since GDAL 3.6
 */

bool VSIIsLocal(const char *pszPath)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->IsLocal(pszPath);
}

/************************************************************************/
/*                       VSIGetCanonicalFilename()                      */
/************************************************************************/

/**
 * \brief Returns the canonical filename.
 *
 * May be implemented by case-insensitive filesystems
 * (currently Win32 and MacOSX) to return the filename with its actual case
 * (i.e. the one that would be used when listing the content of the directory).
 *
 * @param pszPath UTF-8 encoded path
 *
 * @return UTF-8 encoded string, to free with VSIFree()
 *
 * @since GDAL 3.8
 */

char *VSIGetCanonicalFilename(const char *pszPath)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return CPLStrdup(poFSHandler->GetCanonicalFilename(pszPath).c_str());
}

/************************************************************************/
/*                      VSISupportsSequentialWrite()                    */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports sequential write.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 * @param bAllowLocalTempFile whether the file system is allowed to use a
 * local temporary file before uploading to the target location.
 *
 * @return TRUE or FALSE
 *
 * @since GDAL 3.6
 */

bool VSISupportsSequentialWrite(const char *pszPath, bool bAllowLocalTempFile)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->SupportsSequentialWrite(pszPath, bAllowLocalTempFile);
}

/************************************************************************/
/*                      VSISupportsRandomWrite()                        */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports random write.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 * @param bAllowLocalTempFile whether the file system is allowed to use a
 * local temporary file before uploading to the target location.
 *
 * @return TRUE or FALSE
 *
 * @since GDAL 3.6
 */

bool VSISupportsRandomWrite(const char *pszPath, bool bAllowLocalTempFile)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->SupportsRandomWrite(pszPath, bAllowLocalTempFile);
}

/************************************************************************/
/*                     VSIHasOptimizedReadMultiRange()                  */
/************************************************************************/

/**
 * \brief Returns if the filesystem supports efficient multi-range reading.
 *
 * Currently only returns TRUE for /vsicurl/ and derived file systems.
 *
 * @param pszPath the path of the filesystem object to be tested.
 * UTF-8 encoded.
 *
 * @return TRUE if the file system is known to have an efficient multi-range
 * reading.
 *
 * @since GDAL 2.3
 */

int VSIHasOptimizedReadMultiRange(const char *pszPath)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszPath);

    return poFSHandler->HasOptimizedReadMultiRange(pszPath);
}

/************************************************************************/
/*                        VSIGetActualURL()                             */
/************************************************************************/

/**
 * \brief Returns the actual URL of a supplied filename.
 *
 * Currently only returns a non-NULL value for network-based virtual file
 * systems. For example "/vsis3/bucket/filename" will be expanded as
 * "https://bucket.s3.amazon.com/filename"
 *
 * Note that the lifetime of the returned string, is short, and may be
 * invalidated by any following GDAL functions.
 *
 * @param pszFilename the path of the filesystem object. UTF-8 encoded.
 *
 * @return the actual URL corresponding to the supplied filename, or NULL.
 * Should not be freed.
 *
 * @since GDAL 2.3
 */

const char *VSIGetActualURL(const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->GetActualURL(pszFilename);
}

/************************************************************************/
/*                        VSIGetSignedURL()                             */
/************************************************************************/

/**
 * \brief Returns a signed URL of a supplied filename.
 *
 * Currently only returns a non-NULL value for /vsis3/, /vsigs/, /vsiaz/ and
 * /vsioss/ For example "/vsis3/bucket/filename" will be expanded as
 * "https://bucket.s3.amazon.com/filename?X-Amz-Algorithm=AWS4-HMAC-SHA256..."
 * Configuration options that apply for file opening (typically to provide
 * credentials), and are returned by VSIGetFileSystemOptions(), are also valid
 * in that context.
 *
 * @param pszFilename the path of the filesystem object. UTF-8 encoded.
 * @param papszOptions list of options, or NULL. Depend on file system handler.
 * For /vsis3/, /vsigs/, /vsiaz/ and /vsioss/, the following options are
 * supported: <ul> <li>START_DATE=YYMMDDTHHMMSSZ: date and time in UTC following
 * ISO 8601 standard, corresponding to the start of validity of the URL. If not
 * specified, current date time.</li> <li>EXPIRATION_DELAY=number_of_seconds:
 * number between 1 and 604800 (seven days) for the validity of the signed URL.
 * Defaults to 3600 (one hour)</li> <li>VERB=GET/HEAD/DELETE/PUT/POST: HTTP VERB
 * for which the request will be used. Default to GET.</li>
 * </ul>
 *
 * /vsiaz/ supports additional options:
 * <ul>
 * <li>SIGNEDIDENTIFIER=value: to relate the given shared access signature
 * to a corresponding stored access policy.</li>
 * <li>SIGNEDPERMISSIONS=r|w: permissions associated with the shared access
 * signature. Normally deduced from VERB.</li>
 * </ul>
 *
 * @return a signed URL, or NULL. Should be freed with CPLFree().
 * @since GDAL 2.3
 */

char *VSIGetSignedURL(const char *pszFilename, CSLConstList papszOptions)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->GetSignedURL(pszFilename, papszOptions);
}

/************************************************************************/
/*                             VSIFOpenL()                              */
/************************************************************************/

/**
 * \brief Open file.
 *
 * This function opens a file with the desired access.  Large files (larger
 * than 2GB) should be supported.  Binary access is always implied and
 * the "b" does not need to be included in the pszAccess string.
 *
 * Note that the "VSILFILE *" returned since GDAL 1.8.0 by this function is
 * *NOT* a standard C library FILE *, and cannot be used with any functions
 * other than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * On windows it is possible to define the configuration option
 * GDAL_FILE_IS_UTF8 to have pszFilename treated as being in the local
 * encoding instead of UTF-8, restoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (i.e. "r", "r+", "w")
 *
 * @return NULL on failure, or the file handle.
 */

VSILFILE *VSIFOpenL(const char *pszFilename, const char *pszAccess)

{
    return VSIFOpenExL(pszFilename, pszAccess, false);
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

#ifndef DOXYGEN_SKIP

VSIVirtualHandle *VSIFilesystemHandler::Open(const char *pszFilename,
                                             const char *pszAccess)
{
    return Open(pszFilename, pszAccess, false, nullptr);
}

/************************************************************************/
/*                             CopyFile()                               */
/************************************************************************/

int VSIFilesystemHandler::CopyFile(const char *pszSource, const char *pszTarget,
                                   VSILFILE *fpSource, vsi_l_offset nSourceSize,
                                   CSLConstList papszOptions,
                                   GDALProgressFunc pProgressFunc,
                                   void *pProgressData)
{
    VSIVirtualHandleUniquePtr poFileHandleAutoClose;
    if (!fpSource)
    {
        CPLAssert(pszSource);
        fpSource = VSIFOpenExL(pszSource, "rb", TRUE);
        if (!fpSource)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSource);
            return -1;
        }
        poFileHandleAutoClose.reset(fpSource);
    }
    if (nSourceSize == static_cast<vsi_l_offset>(-1) &&
        pProgressFunc != nullptr && pszSource != nullptr)
    {
        VSIStatBufL sStat;
        if (VSIStatL(pszSource, &sStat) == 0)
        {
            nSourceSize = sStat.st_size;
        }
    }

    VSILFILE *fpOut = VSIFOpenEx2L(pszTarget, "wb", TRUE, papszOptions);
    if (!fpOut)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszTarget);
        return -1;
    }

    CPLString osMsg;
    if (pszSource)
        osMsg.Printf("Copying of %s", pszSource);
    else
        pszSource = "(unknown filename)";

    int ret = 0;
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);
    GUIntBig nOffset = 0;
    while (true)
    {
        const size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fpSource);
        if (nRead < nBufferSize && VSIFErrorL(fpSource))
        {
            CPLError(
                CE_Failure, CPLE_FileIO,
                "Copying of %s to %s failed: error while reading source file",
                pszSource, pszTarget);
            ret = -1;
            break;
        }
        if (nRead > 0)
        {
            const size_t nWritten = VSIFWriteL(&abyBuffer[0], 1, nRead, fpOut);
            if (nWritten != nRead)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Copying of %s to %s failed: error while writing into "
                         "target file",
                         pszSource, pszTarget);
                ret = -1;
                break;
            }
            nOffset += nRead;
            if (pProgressFunc &&
                !pProgressFunc(
                    nSourceSize == 0 ? 1.0
                    : nSourceSize > 0 &&
                            nSourceSize != static_cast<vsi_l_offset>(-1)
                        ? double(nOffset) / nSourceSize
                        : 0.0,
                    !osMsg.empty() ? osMsg.c_str() : nullptr, pProgressData))
            {
                ret = -1;
                break;
            }
        }
        if (nRead < nBufferSize)
        {
            break;
        }
    }

    if (nSourceSize != static_cast<vsi_l_offset>(-1) && nOffset != nSourceSize)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Copying of %s to %s failed: %" PRIu64 " bytes were copied "
                 "whereas %" PRIu64 " were expected",
                 pszSource, pszTarget, static_cast<uint64_t>(nOffset),
                 static_cast<uint64_t>(nSourceSize));
        ret = -1;
    }

    if (VSIFCloseL(fpOut) != 0)
    {
        ret = -1;
    }

    if (ret != 0)
        VSIUnlink(pszTarget);

    return ret;
}

/************************************************************************/
/*                       CopyFileRestartable()                          */
/************************************************************************/

int VSIFilesystemHandler::CopyFileRestartable(
    const char *pszSource, const char *pszTarget,
    const char * /* pszInputPayload */, char **ppszOutputPayload,
    CSLConstList papszOptions, GDALProgressFunc pProgressFunc,
    void *pProgressData)
{
    *ppszOutputPayload = nullptr;
    return CopyFile(pszSource, pszTarget, nullptr,
                    static_cast<vsi_l_offset>(-1), papszOptions, pProgressFunc,
                    pProgressData);
}

/************************************************************************/
/*                               Sync()                                 */
/************************************************************************/

bool VSIFilesystemHandler::Sync(const char *pszSource, const char *pszTarget,
                                const char *const *papszOptions,
                                GDALProgressFunc pProgressFunc,
                                void *pProgressData, char ***ppapszOutputs)
{
    const char SOURCE_SEP = VSIGetDirectorySeparator(pszSource)[0];

    if (ppapszOutputs)
    {
        *ppapszOutputs = nullptr;
    }

    VSIStatBufL sSource;
    CPLString osSource(pszSource);
    CPLString osSourceWithoutSlash(pszSource);
    if (osSourceWithoutSlash.back() == '/' ||
        osSourceWithoutSlash.back() == '\\')
    {
        osSourceWithoutSlash.pop_back();
    }
    if (VSIStatL(osSourceWithoutSlash, &sSource) < 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "%s does not exist", pszSource);
        return false;
    }

    if (VSI_ISDIR(sSource.st_mode))
    {
        std::string osTargetDir(pszTarget);
        if (osSource.back() != '/' && osSource.back() != '\\')
        {
            osTargetDir = CPLFormFilenameSafe(
                osTargetDir.c_str(), CPLGetFilename(pszSource), nullptr);
        }

        VSIStatBufL sTarget;
        bool ret = true;
        if (VSIStatL(osTargetDir.c_str(), &sTarget) < 0)
        {
            if (VSIMkdirRecursive(osTargetDir.c_str(), 0755) < 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s",
                         osTargetDir.c_str());
                return false;
            }
        }

        if (!CPLFetchBool(papszOptions, "STOP_ON_DIR", false))
        {
            CPLStringList aosChildOptions(CSLDuplicate(papszOptions));
            if (!CPLFetchBool(papszOptions, "RECURSIVE", true))
            {
                aosChildOptions.SetNameValue("RECURSIVE", nullptr);
                aosChildOptions.AddString("STOP_ON_DIR=TRUE");
            }

            char **papszSrcFiles = VSIReadDir(osSourceWithoutSlash);
            int nFileCount = 0;
            for (auto iter = papszSrcFiles; iter && *iter; ++iter)
            {
                if (strcmp(*iter, ".") != 0 && strcmp(*iter, "..") != 0)
                {
                    nFileCount++;
                }
            }
            int iFile = 0;
            for (auto iter = papszSrcFiles; iter && *iter; ++iter, ++iFile)
            {
                if (strcmp(*iter, ".") == 0 || strcmp(*iter, "..") == 0)
                {
                    continue;
                }
                const std::string osSubSource(CPLFormFilenameSafe(
                    osSourceWithoutSlash.c_str(), *iter, nullptr));
                const std::string osSubTarget(
                    CPLFormFilenameSafe(osTargetDir.c_str(), *iter, nullptr));
                // coverity[divide_by_zero]
                void *pScaledProgress = GDALCreateScaledProgress(
                    double(iFile) / nFileCount, double(iFile + 1) / nFileCount,
                    pProgressFunc, pProgressData);
                ret = Sync((osSubSource + SOURCE_SEP).c_str(),
                           osSubTarget.c_str(), aosChildOptions.List(),
                           GDALScaledProgress, pScaledProgress, nullptr);
                GDALDestroyScaledProgress(pScaledProgress);
                if (!ret)
                {
                    break;
                }
            }
            CSLDestroy(papszSrcFiles);
        }
        return ret;
    }

    VSIStatBufL sTarget;
    std::string osTarget(pszTarget);
    if (VSIStatL(osTarget.c_str(), &sTarget) == 0)
    {
        bool bTargetIsFile = true;
        if (VSI_ISDIR(sTarget.st_mode))
        {
            osTarget = CPLFormFilenameSafe(osTarget.c_str(),
                                           CPLGetFilename(pszSource), nullptr);
            bTargetIsFile = VSIStatL(osTarget.c_str(), &sTarget) == 0 &&
                            !CPL_TO_BOOL(VSI_ISDIR(sTarget.st_mode));
        }
        if (bTargetIsFile)
        {
            if (sSource.st_size == sTarget.st_size &&
                sSource.st_mtime == sTarget.st_mtime && sSource.st_mtime != 0)
            {
                CPLDebug("VSI",
                         "%s and %s have same size and modification "
                         "date. Skipping copying",
                         osSourceWithoutSlash.c_str(), osTarget.c_str());
                return true;
            }
        }
    }

    VSILFILE *fpIn = VSIFOpenExL(osSourceWithoutSlash, "rb", TRUE);
    if (fpIn == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 osSourceWithoutSlash.c_str());
        return false;
    }

    VSILFILE *fpOut = VSIFOpenExL(osTarget.c_str(), "wb", TRUE);
    if (fpOut == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", osTarget.c_str());
        VSIFCloseL(fpIn);
        return false;
    }

    bool ret = true;
    constexpr size_t nBufferSize = 10 * 4096;
    std::vector<GByte> abyBuffer(nBufferSize, 0);
    GUIntBig nOffset = 0;
    CPLString osMsg;
    osMsg.Printf("Copying of %s", osSourceWithoutSlash.c_str());
    while (true)
    {
        size_t nRead = VSIFReadL(&abyBuffer[0], 1, nBufferSize, fpIn);
        size_t nWritten = VSIFWriteL(&abyBuffer[0], 1, nRead, fpOut);
        if (nWritten != nRead)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Copying of %s to %s failed",
                     osSourceWithoutSlash.c_str(), osTarget.c_str());
            ret = false;
            break;
        }
        nOffset += nRead;
        if (pProgressFunc && !pProgressFunc(double(nOffset) / sSource.st_size,
                                            osMsg.c_str(), pProgressData))
        {
            ret = false;
            break;
        }
        if (nRead < nBufferSize)
        {
            break;
        }
    }

    VSIFCloseL(fpIn);
    if (VSIFCloseL(fpOut) != 0)
    {
        ret = false;
    }
    return ret;
}

/************************************************************************/
/*                            VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::VSIDIREntry()
    : pszName(nullptr), nMode(0), nSize(0), nMTime(0), bModeKnown(false),
      bSizeKnown(false), bMTimeKnown(false), papszExtra(nullptr)
{
}

/************************************************************************/
/*                            VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::VSIDIREntry(const VSIDIREntry &other)
    : pszName(VSIStrdup(other.pszName)), nMode(other.nMode), nSize(other.nSize),
      nMTime(other.nMTime), bModeKnown(other.bModeKnown),
      bSizeKnown(other.bSizeKnown), bMTimeKnown(other.bMTimeKnown),
      papszExtra(CSLDuplicate(other.papszExtra))
{
}

/************************************************************************/
/*                           ~VSIDIREntry()                             */
/************************************************************************/

VSIDIREntry::~VSIDIREntry()
{
    CPLFree(pszName);
    CSLDestroy(papszExtra);
}

/************************************************************************/
/*                              ~VSIDIR()                               */
/************************************************************************/

VSIDIR::~VSIDIR()
{
}

/************************************************************************/
/*                            VSIDIRGeneric                             */
/************************************************************************/

namespace
{
struct VSIDIRGeneric : public VSIDIR
{
    CPLString osRootPath{};
    CPLString osBasePath{};
    char **papszContent = nullptr;
    int nRecurseDepth = 0;
    int nPos = 0;
    VSIDIREntry entry{};
    std::vector<VSIDIRGeneric *> aoStackSubDir{};
    VSIFilesystemHandler *poFS = nullptr;
    std::string m_osFilterPrefix{};

    explicit VSIDIRGeneric(VSIFilesystemHandler *poFSIn) : poFS(poFSIn)
    {
    }

    ~VSIDIRGeneric();

    const VSIDIREntry *NextDirEntry() override;

    VSIDIRGeneric(const VSIDIRGeneric &) = delete;
    VSIDIRGeneric &operator=(const VSIDIRGeneric &) = delete;
};

/************************************************************************/
/*                         ~VSIDIRGeneric()                             */
/************************************************************************/

VSIDIRGeneric::~VSIDIRGeneric()
{
    while (!aoStackSubDir.empty())
    {
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }
    CSLDestroy(papszContent);
}

}  // namespace

/************************************************************************/
/*                            OpenDir()                                 */
/************************************************************************/

VSIDIR *VSIFilesystemHandler::OpenDir(const char *pszPath, int nRecurseDepth,
                                      const char *const *papszOptions)
{
    char **papszContent = VSIReadDir(pszPath);
    VSIStatBufL sStatL;
    if (papszContent == nullptr &&
        (VSIStatL(pszPath, &sStatL) != 0 || !VSI_ISDIR(sStatL.st_mode)))
    {
        return nullptr;
    }
    VSIDIRGeneric *dir = new VSIDIRGeneric(this);
    dir->osRootPath = pszPath;
    if (!dir->osRootPath.empty() &&
        (dir->osRootPath.back() == '/' || dir->osRootPath.back() == '\\'))
        dir->osRootPath.pop_back();
    dir->nRecurseDepth = nRecurseDepth;
    dir->papszContent = papszContent;
    dir->m_osFilterPrefix = CSLFetchNameValueDef(papszOptions, "PREFIX", "");
    return dir;
}

/************************************************************************/
/*                           NextDirEntry()                             */
/************************************************************************/

const VSIDIREntry *VSIDIRGeneric::NextDirEntry()
{
    const char SEP = VSIGetDirectorySeparator(osRootPath.c_str())[0];

begin:
    if (VSI_ISDIR(entry.nMode) && nRecurseDepth != 0)
    {
        CPLString osCurFile(osRootPath);
        if (!osCurFile.empty())
            osCurFile += SEP;
        osCurFile += entry.pszName;
        auto subdir =
            static_cast<VSIDIRGeneric *>(poFS->VSIFilesystemHandler::OpenDir(
                osCurFile, nRecurseDepth - 1, nullptr));
        if (subdir)
        {
            subdir->osRootPath = osRootPath;
            subdir->osBasePath = entry.pszName;
            subdir->m_osFilterPrefix = m_osFilterPrefix;
            aoStackSubDir.push_back(subdir);
        }
        entry.nMode = 0;
    }

    while (!aoStackSubDir.empty())
    {
        auto l_entry = aoStackSubDir.back()->NextDirEntry();
        if (l_entry)
        {
            return l_entry;
        }
        delete aoStackSubDir.back();
        aoStackSubDir.pop_back();
    }

    if (papszContent == nullptr)
    {
        return nullptr;
    }

    while (true)
    {
        if (!papszContent[nPos])
        {
            return nullptr;
        }
        // Skip . and ..entries
        if (papszContent[nPos][0] == '.' &&
            (papszContent[nPos][1] == '\0' ||
             (papszContent[nPos][1] == '.' && papszContent[nPos][2] == '\0')))
        {
            nPos++;
        }
        else
        {
            CPLFree(entry.pszName);
            CPLString osName(osBasePath);
            if (!osName.empty())
                osName += SEP;
            osName += papszContent[nPos];
            nPos++;

            entry.pszName = CPLStrdup(osName);
            entry.nMode = 0;
            CPLString osCurFile(osRootPath);
            if (!osCurFile.empty())
                osCurFile += SEP;
            osCurFile += entry.pszName;

            const auto StatFile = [&osCurFile, this]()
            {
                VSIStatBufL sStatL;
                if (VSIStatL(osCurFile, &sStatL) == 0)
                {
                    entry.nMode = sStatL.st_mode;
                    entry.nSize = sStatL.st_size;
                    entry.nMTime = sStatL.st_mtime;
                    entry.bModeKnown = true;
                    entry.bSizeKnown = true;
                    entry.bMTimeKnown = true;
                }
                else
                {
                    entry.nMode = 0;
                    entry.nSize = 0;
                    entry.nMTime = 0;
                    entry.bModeKnown = false;
                    entry.bSizeKnown = false;
                    entry.bMTimeKnown = false;
                }
            };

            if (!m_osFilterPrefix.empty() &&
                m_osFilterPrefix.size() > osName.size())
            {
                if (STARTS_WITH(m_osFilterPrefix.c_str(), osName.c_str()) &&
                    m_osFilterPrefix[osName.size()] == SEP)
                {
                    StatFile();
                    if (VSI_ISDIR(entry.nMode))
                    {
                        goto begin;
                    }
                }
                continue;
            }
            if (!m_osFilterPrefix.empty() &&
                !STARTS_WITH(osName.c_str(), m_osFilterPrefix.c_str()))
            {
                continue;
            }

            StatFile();

            break;
        }
    }

    return &(entry);
}

/************************************************************************/
/*                           UnlinkBatch()                              */
/************************************************************************/

int *VSIFilesystemHandler::UnlinkBatch(CSLConstList papszFiles)
{
    int *panRet =
        static_cast<int *>(CPLMalloc(sizeof(int) * CSLCount(papszFiles)));
    for (int i = 0; papszFiles && papszFiles[i]; ++i)
    {
        panRet[i] = VSIUnlink(papszFiles[i]) == 0;
    }
    return panRet;
}

/************************************************************************/
/*                          RmdirRecursive()                            */
/************************************************************************/

int VSIFilesystemHandler::RmdirRecursive(const char *pszDirname)
{
    CPLString osDirnameWithoutEndSlash(pszDirname);
    if (!osDirnameWithoutEndSlash.empty() &&
        (osDirnameWithoutEndSlash.back() == '/' ||
         osDirnameWithoutEndSlash.back() == '\\'))
    {
        osDirnameWithoutEndSlash.pop_back();
    }

    const char SEP = VSIGetDirectorySeparator(pszDirname)[0];

    CPLStringList aosOptions;
    auto poDir =
        std::unique_ptr<VSIDIR>(OpenDir(pszDirname, -1, aosOptions.List()));
    if (!poDir)
        return -1;
    std::vector<std::string> aosDirs;
    while (true)
    {
        auto entry = poDir->NextDirEntry();
        if (!entry)
            break;

        const CPLString osFilename(osDirnameWithoutEndSlash + SEP +
                                   entry->pszName);
        if ((entry->nMode & S_IFDIR))
        {
            aosDirs.push_back(osFilename);
        }
        else
        {
            if (VSIUnlink(osFilename) != 0)
                return -1;
        }
    }

    // Sort in reverse order, so that inner-most directories are deleted first
    std::sort(aosDirs.begin(), aosDirs.end(),
              [](const std::string &a, const std::string &b) { return a > b; });

    for (const auto &osDir : aosDirs)
    {
        if (VSIRmdir(osDir.c_str()) != 0)
            return -1;
    }

    return VSIRmdir(pszDirname);
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char **VSIFilesystemHandler::GetFileMetadata(const char * /* pszFilename*/,
                                             const char * /*pszDomain*/,
                                             CSLConstList /*papszOptions*/)
{
    return nullptr;
}

/************************************************************************/
/*                          SetFileMetadata()                           */
/************************************************************************/

bool VSIFilesystemHandler::SetFileMetadata(const char * /* pszFilename*/,
                                           CSLConstList /* papszMetadata */,
                                           const char * /* pszDomain */,
                                           CSLConstList /* papszOptions */)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetFileMetadata() not supported");
    return false;
}

#endif

/************************************************************************/
/*                             VSIFOpenExL()                            */
/************************************************************************/

/**
 * \brief Open/create file.
 *
 * This function opens (or creates) a file with the desired access.
 * Binary access is always implied and
 * the "b" does not need to be included in the pszAccess string.
 *
 * Note that the "VSILFILE *" returned by this function is
 * *NOT* a standard C library FILE *, and cannot be used with any functions
 * other than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * On windows it is possible to define the configuration option
 * GDAL_FILE_IS_UTF8 to have pszFilename treated as being in the local
 * encoding instead of UTF-8, restoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (i.e. "r", "r+", "w")
 * @param bSetError flag determining whether or not this open call
 * should set VSIErrors on failure.
 *
 * @return NULL on failure, or the file handle.
 *
 * @since GDAL 2.1
 */

VSILFILE *VSIFOpenExL(const char *pszFilename, const char *pszAccess,
                      int bSetError)

{
    return VSIFOpenEx2L(pszFilename, pszAccess, bSetError, nullptr);
}

/************************************************************************/
/*                            VSIFOpenEx2L()                            */
/************************************************************************/

/**
 * \brief Open/create file.
 *
 * This function opens (or creates) a file with the desired access.
 * Binary access is always implied and
 * the "b" does not need to be included in the pszAccess string.
 *
 * Note that the "VSILFILE *" returned by this function is
 * *NOT* a standard C library FILE *, and cannot be used with any functions
 * other than the "VSI*L" family of functions.  They aren't "real" FILE objects.
 *
 * On windows it is possible to define the configuration option
 * GDAL_FILE_IS_UTF8 to have pszFilename treated as being in the local
 * encoding instead of UTF-8, restoring the pre-1.8.0 behavior of VSIFOpenL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * The following options are supported:
 * <ul>
 * <li>MIME headers such as Content-Type and Content-Encoding
 * are supported for the /vsis3/, /vsigs/, /vsiaz/, /vsiadls/ file systems.</li>
 * <li>DISABLE_READDIR_ON_OPEN=YES/NO (GDAL >= 3.6) for /vsicurl/ and other
 * network-based file systems. By default, directory file listing is done,
 * unless YES is specified.</li>
 * <li>WRITE_THROUGH=YES (GDAL >= 3.8) for the Windows regular files to
 * set the FILE_FLAG_WRITE_THROUGH flag to the CreateFile() function. In that
 * mode, the data is written to the system cache but is flushed to disk without
 * delay.</li>
 * </ul>
 *
 * Options specifics to /vsis3/, /vsigs/, /vsioss/ and /vsiaz/ in "w" mode:
 * <ul>
 * <li>CHUNK_SIZE=val in MiB. (GDAL >= 3.10) Size of a block. Default is 50 MiB.
 * For /vsis3/, /vsigz/, /vsioss/, it can be up to 5000 MiB.
 * For /vsiaz/, only taken into account when BLOB_TYPE=BLOCK. It can be up to 4000 MiB.
 * </li>
 * </ul>
 *
 * Options specifics to /vsiaz/ in "w" mode:
 * <ul>
 * <li>BLOB_TYPE=APPEND/BLOCK. (GDAL >= 3.10) Type of blob. Defaults to APPEND.
 * Append blocks are limited to 195 GiB
 * (however if the file size is below 4 MiB, a block blob will be created in a
 * single PUT operation)
 * </li>
 * </ul>
 *
 * Analog of the POSIX fopen() function.
 *
 * @param pszFilename the file to open.  UTF-8 encoded.
 * @param pszAccess access requested (i.e. "r", "r+", "w")
 * @param bSetError flag determining whether or not this open call
 * should set VSIErrors on failure.
 * @param papszOptions NULL or NULL-terminated list of strings. The content is
 *                     highly file system dependent.
 *
 *
 * @return NULL on failure, or the file handle.
 *
 * @since GDAL 3.3
 */

VSILFILE *VSIFOpenEx2L(const char *pszFilename, const char *pszAccess,
                       int bSetError, CSLConstList papszOptions)

{
    // Too long filenames can cause excessive memory allocation due to
    // recursion in some filesystem handlers
    constexpr size_t knMaxPath = 8192;
    if (CPLStrnlen(pszFilename, knMaxPath) == knMaxPath)
        return nullptr;

    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    VSILFILE *fp = poFSHandler->Open(pszFilename, pszAccess,
                                     CPL_TO_BOOL(bSetError), papszOptions);

    VSIDebug4("VSIFOpenEx2L(%s,%s,%d) = %p", pszFilename, pszAccess, bSetError,
              fp);

    return fp;
}

/************************************************************************/
/*                             VSIFCloseL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Close()
 * \brief Close file.
 *
 * This function closes the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fclose() function.
 *
 * @return 0 on success or -1 on failure.
 */

/**
 * \brief Close file.
 *
 * This function closes the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fclose() function.
 *
 * @param fp file handle opened with VSIFOpenL().  Passing a nullptr produces
 * undefined behavior.
 *
 * @return 0 on success or -1 on failure.
 */

int VSIFCloseL(VSILFILE *fp)

{
    VSIDebug1("VSIFCloseL(%p)", fp);

    const int nResult = fp->Close();

    delete fp;

    return nResult;
}

/************************************************************************/
/*                             VSIFSeekL()                              */
/************************************************************************/

/**
 * \fn int VSIVirtualHandle::Seek( vsi_l_offset nOffset, int nWhence )
 * \brief Seek to requested offset.
 *
 * Seek to the desired offset (nOffset) in the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fseek() call.
 *
 * Caution: vsi_l_offset is a unsigned type, so SEEK_CUR can only be used
 * for positive seek. If negative seek is needed, use
 * handle->Seek( handle->Tell() + negative_offset, SEEK_SET ).
 *
 * @param nOffset offset in bytes.
 * @param nWhence one of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * @return 0 on success or -1 one failure.
 */

/**
 * \brief Seek to requested offset.
 *
 * Seek to the desired offset (nOffset) in the indicated file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fseek() call.
 *
 * Caution: vsi_l_offset is a unsigned type, so SEEK_CUR can only be used
 * for positive seek. If negative seek is needed, use
 * VSIFSeekL( fp, VSIFTellL(fp) + negative_offset, SEEK_SET ).
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param nOffset offset in bytes.
 * @param nWhence one of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * @return 0 on success or -1 one failure.
 */

int VSIFSeekL(VSILFILE *fp, vsi_l_offset nOffset, int nWhence)

{
    return fp->Seek(nOffset, nWhence);
}

/************************************************************************/
/*                             VSIFTellL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Tell()
 * \brief Tell current file offset.
 *
 * Returns the current file read/write offset in bytes from the beginning of
 * the file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftell() call.
 *
 * @return file offset in bytes.
 */

/**
 * \brief Tell current file offset.
 *
 * Returns the current file read/write offset in bytes from the beginning of
 * the file.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftell() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return file offset in bytes.
 */

vsi_l_offset VSIFTellL(VSILFILE *fp)

{
    return fp->Tell();
}

/************************************************************************/
/*                             VSIRewindL()                             */
/************************************************************************/

/**
 * \brief Rewind the file pointer to the beginning of the file.
 *
 * This is equivalent to VSIFSeekL( fp, 0, SEEK_SET )
 *
 * Analog of the POSIX rewind() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 */

void VSIRewindL(VSILFILE *fp)

{
    CPL_IGNORE_RET_VAL(VSIFSeekL(fp, 0, SEEK_SET));
}

/************************************************************************/
/*                             VSIFFlushL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Flush()
 * \brief Flush pending writes to disk.
 *
 * For files in write or update mode and on filesystem types where it is
 * applicable, all pending output on the file is flushed to the physical disk.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fflush() call.
 *
 * On Windows regular files, this method does nothing, unless the
 * VSI_FLUSH configuration option is set to YES (and only when the file has
 * *not* been opened with the WRITE_THROUGH option).
 *
 * @return 0 on success or -1 on error.
 */

/**
 * \brief Flush pending writes to disk.
 *
 * For files in write or update mode and on filesystem types where it is
 * applicable, all pending output on the file is flushed to the physical disk.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fflush() call.
 *
 * On Windows regular files, this method does nothing, unless the
 * VSI_FLUSH configuration option is set to YES (and only when the file has
 * *not* been opened with the WRITE_THROUGH option).
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 0 on success or -1 on error.
 */

int VSIFFlushL(VSILFILE *fp)

{
    return fp->Flush();
}

/************************************************************************/
/*                             VSIFReadL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Read( void *pBuffer, size_t nSize, size_t nCount )
 * \brief Read bytes from file.
 *
 * Reads nCount objects of nSize bytes from the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fread() call.
 *
 * @param pBuffer the buffer into which the data should be read (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 *
 * @return number of objects successfully read. If that number is less than
 * nCount, VSIFEofL() or VSIFErrorL() can be used to determine the reason for
 * the short read.
 */

/**
 * \brief Read bytes from file.
 *
 * Reads nCount objects of nSize bytes from the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fread() call.
 *
 * @param pBuffer the buffer into which the data should be read (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to read in bytes.
 * @param nCount number of objects to read.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return number of objects successfully read. If that number is less than
 * nCount, VSIFEofL() or VSIFErrorL() can be used to determine the reason for
 * the short read.
 */

size_t VSIFReadL(void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp)

{
    return fp->Read(pBuffer, nSize, nCount);
}

/************************************************************************/
/*                       VSIFReadMultiRangeL()                          */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::ReadMultiRange( int nRanges, void ** ppData,
 *                                       const vsi_l_offset* panOffsets,
 *                                       const size_t* panSizes )
 * \brief Read several ranges of bytes from file.
 *
 * Reads nRanges objects of panSizes[i] bytes from the indicated file at the
 * offset panOffsets[i] into the buffer ppData[i].
 *
 * Ranges must be sorted in ascending start offset, and must not overlap each
 * other.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory or /vsicurl/.
 *
 * @param nRanges number of ranges to read.
 * @param ppData array of nRanges buffer into which the data should be read
 *               (ppData[i] must be at list panSizes[i] bytes).
 * @param panOffsets array of nRanges offsets at which the data should be read.
 * @param panSizes array of nRanges sizes of objects to read (in bytes).
 *
 * @return 0 in case of success, -1 otherwise.
 * @since GDAL 1.9.0
 */

/**
 * \brief Read several ranges of bytes from file.
 *
 * Reads nRanges objects of panSizes[i] bytes from the indicated file at the
 * offset panOffsets[i] into the buffer ppData[i].
 *
 * Ranges must be sorted in ascending start offset, and must not overlap each
 * other.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory or /vsicurl/.
 *
 * @param nRanges number of ranges to read.
 * @param ppData array of nRanges buffer into which the data should be read
 *               (ppData[i] must be at list panSizes[i] bytes).
 * @param panOffsets array of nRanges offsets at which the data should be read.
 * @param panSizes array of nRanges sizes of objects to read (in bytes).
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 0 in case of success, -1 otherwise.
 * @since GDAL 1.9.0
 */

int VSIFReadMultiRangeL(int nRanges, void **ppData,
                        const vsi_l_offset *panOffsets, const size_t *panSizes,
                        VSILFILE *fp)
{
    return fp->ReadMultiRange(nRanges, ppData, panOffsets, panSizes);
}

/************************************************************************/
/*                             VSIFWriteL()                             */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Write( const void *pBuffer,
 *                              size_t nSize, size_t nCount )
 * \brief Write bytes to file.
 *
 * Writes nCount objects of nSize bytes to the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fwrite() call.
 *
 * @param pBuffer the buffer from which the data should be written (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to write in bytes.
 * @param nCount number of objects to write.
 *
 * @return number of objects successfully written.
 */

/**
 * \brief Write bytes to file.
 *
 * Writes nCount objects of nSize bytes to the indicated file at the
 * current offset into the indicated buffer.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX fwrite() call.
 *
 * @param pBuffer the buffer from which the data should be written (at least
 * nCount * nSize bytes in size.
 * @param nSize size of objects to write in bytes.
 * @param nCount number of objects to write.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return number of objects successfully written.
 */

size_t VSIFWriteL(const void *pBuffer, size_t nSize, size_t nCount,
                  VSILFILE *fp)

{
    return fp->Write(pBuffer, nSize, nCount);
}

/************************************************************************/
/*                              VSIFEofL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Eof()
 * \brief Test for end of file.
 *
 * Returns TRUE (non-zero) if an end-of-file condition occurred during the
 * previous read operation. The end-of-file flag is cleared by a successful
 * VSIFSeekL() call, or a call to VSIFClearErrL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX feof() call.
 *
 * @return TRUE if at EOF, else FALSE.
 */

/**
 * \brief Test for end of file.
 *
 * Returns TRUE (non-zero) if an end-of-file condition occurred during the
 * previous read operation. The end-of-file flag is cleared by a successful
 * VSIFSeekL() call, or a call to VSIFClearErrL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX feof() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return TRUE if at EOF, else FALSE.
 */

int VSIFEofL(VSILFILE *fp)

{
    return fp->Eof();
}

/************************************************************************/
/*                            VSIFErrorL()                              */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Error()
 * \brief Test the error indicator.
 *
 * Returns TRUE (non-zero) if an error condition occurred during the
 * previous read operation. The error indicator is cleared by a call to
 * VSIFClearErrL(). Note that a end-of-file situation, reported by VSIFEofL(),
 * is *not* an error reported by VSIFErrorL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ferror() call.
 *
 * @return TRUE if the error indicator is set, else FALSE.
 * @since 3.10
 */

/**
 * \brief Test the error indicator.
 *
 * Returns TRUE (non-zero) if an error condition occurred during the
 * previous read operation. The error indicator is cleared by a call to
 * VSIFClearErrL(). Note that a end-of-file situation, reported by VSIFEofL(),
 * is *not* an error reported by VSIFErrorL().
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX feof() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return TRUE if the error indicator is set, else FALSE.
 * @since 3.10
 */

int VSIFErrorL(VSILFILE *fp)

{
    return fp->Error();
}

/************************************************************************/
/*                           VSIFClearErrL()                            */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::ClearErr()
 * \brief Reset the error and end-of-file indicators.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX clearerr() call.
 *
 * @since 3.10
 */

/**
 * \brief Reset the error and end-of-file indicators.
 *
 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX clearerr() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @since 3.10
 */

void VSIFClearErrL(VSILFILE *fp)

{
    fp->ClearErr();
}

/************************************************************************/
/*                            VSIFTruncateL()                           */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::Truncate( vsi_l_offset nNewSize )
 * \brief Truncate/expand the file to the specified size

 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftruncate() call.
 *
 * @param nNewSize new size in bytes.
 *
 * @return 0 on success
 * @since GDAL 1.9.0
 */

/**
 * \brief Truncate/expand the file to the specified size

 * This method goes through the VSIFileHandler virtualization and may
 * work on unusual filesystems such as in memory.
 *
 * Analog of the POSIX ftruncate() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param nNewSize new size in bytes.
 *
 * @return 0 on success
 * @since GDAL 1.9.0
 */

int VSIFTruncateL(VSILFILE *fp, vsi_l_offset nNewSize)

{
    return fp->Truncate(nNewSize);
}

/************************************************************************/
/*                            VSIFPrintfL()                             */
/************************************************************************/

/**
 * \brief Formatted write to file.
 *
 * Provides fprintf() style formatted output to a VSI*L file.  This formats
 * an internal buffer which is written using VSIFWriteL().
 *
 * Analog of the POSIX fprintf() call.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param pszFormat the printf() style format string.
 *
 * @return the number of bytes written or -1 on an error.
 */

int VSIFPrintfL(VSILFILE *fp, CPL_FORMAT_STRING(const char *pszFormat), ...)

{
    va_list args;

    va_start(args, pszFormat);
    CPLString osResult;
    osResult.vPrintf(pszFormat, args);
    va_end(args);

    return static_cast<int>(
        VSIFWriteL(osResult.c_str(), 1, osResult.length(), fp));
}

/************************************************************************/
/*                 VSIVirtualHandle::Printf()                           */
/************************************************************************/

/**
 * \brief Formatted write to file.
 *
 * Provides fprintf() style formatted output to a VSI*L file.  This formats
 * an internal buffer which is written using VSIFWriteL().
 *
 * Analog of the POSIX fprintf() call.
 *
 * @param pszFormat the printf() style format string.
 *
 * @return the number of bytes written or -1 on an error.
 */

int VSIVirtualHandle::Printf(CPL_FORMAT_STRING(const char *pszFormat), ...)
{
    va_list args;

    va_start(args, pszFormat);
    CPLString osResult;
    osResult.vPrintf(pszFormat, args);
    va_end(args);

    return static_cast<int>(Write(osResult.c_str(), 1, osResult.length()));
}

/************************************************************************/
/*                              VSIFPutcL()                              */
/************************************************************************/

// TODO: should we put in conformance with POSIX regarding the return
// value. As of today (2015-08-29), no code in GDAL sources actually
// check the return value.

/**
 * \brief Write a single byte to the file
 *
 * Writes the character nChar, cast to an unsigned char, to file.
 *
 * Almost an analog of the POSIX  fputc() call, except that it returns
 * the  number of  character  written (1  or 0),  and  not the  (cast)
 * character itself or EOF.
 *
 * @param nChar character to write.
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return 1 in case of success, 0 on error.
 */

int VSIFPutcL(int nChar, VSILFILE *fp)

{
    const unsigned char cChar = static_cast<unsigned char>(nChar);
    return static_cast<int>(VSIFWriteL(&cChar, 1, 1, fp));
}

/************************************************************************/
/*                        VSIFGetRangeStatusL()                        */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::GetRangeStatus( vsi_l_offset nOffset,
 *                                       vsi_l_offset nLength )
 * \brief Return if a given file range contains data or holes filled with zeroes
 *
 * This uses the filesystem capabilities of querying which regions of
 * a sparse file are allocated or not. This is currently only
 * implemented for Linux (and no other Unix derivatives) and Windows.
 *
 * Note: A return of VSI_RANGE_STATUS_DATA doesn't exclude that the
 * extent is filled with zeroes! It must be interpreted as "may
 * contain non-zero data".
 *
 * @param nOffset offset of the start of the extent.
 * @param nLength extent length.
 *
 * @return extent status: VSI_RANGE_STATUS_UNKNOWN, VSI_RANGE_STATUS_DATA or
 *         VSI_RANGE_STATUS_HOLE
 * @since GDAL 2.2
 */

/**
 * \brief Return if a given file range contains data or holes filled with zeroes
 *
 * This uses the filesystem capabilities of querying which regions of
 * a sparse file are allocated or not. This is currently only
 * implemented for Linux (and no other Unix derivatives) and Windows.
 *
 * Note: A return of VSI_RANGE_STATUS_DATA doesn't exclude that the
 * extent is filled with zeroes! It must be interpreted as "may
 * contain non-zero data".
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param nOffset offset of the start of the extent.
 * @param nLength extent length.
 *
 * @return extent status: VSI_RANGE_STATUS_UNKNOWN, VSI_RANGE_STATUS_DATA or
 *         VSI_RANGE_STATUS_HOLE
 * @since GDAL 2.2
 */

VSIRangeStatus VSIFGetRangeStatusL(VSILFILE *fp, vsi_l_offset nOffset,
                                   vsi_l_offset nLength)
{
    return fp->GetRangeStatus(nOffset, nLength);
}

/************************************************************************/
/*                           VSIIngestFile()                            */
/************************************************************************/

/**
 * \brief Ingest a file into memory.
 *
 * Read the whole content of a file into a memory buffer.
 *
 * Either fp or pszFilename can be NULL, but not both at the same time.
 *
 * If fp is passed non-NULL, it is the responsibility of the caller to
 * close it.
 *
 * If non-NULL, the returned buffer is guaranteed to be NUL-terminated.
 *
 * @param fp file handle opened with VSIFOpenL().
 * @param pszFilename filename.
 * @param ppabyRet pointer to the target buffer. *ppabyRet must be freed with
 *                 VSIFree()
 * @param pnSize pointer to variable to store the file size. May be NULL.
 * @param nMaxSize maximum size of file allowed. If no limit, set to a negative
 *                 value.
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 1.11
 */

int VSIIngestFile(VSILFILE *fp, const char *pszFilename, GByte **ppabyRet,
                  vsi_l_offset *pnSize, GIntBig nMaxSize)
{
    if (fp == nullptr && pszFilename == nullptr)
        return FALSE;
    if (ppabyRet == nullptr)
        return FALSE;

    *ppabyRet = nullptr;
    if (pnSize != nullptr)
        *pnSize = 0;

    bool bFreeFP = false;
    if (nullptr == fp)
    {
        fp = VSIFOpenL(pszFilename, "rb");
        if (nullptr == fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open file '%s'",
                     pszFilename);
            return FALSE;
        }
        bFreeFP = true;
    }
    else
    {
        if (VSIFSeekL(fp, 0, SEEK_SET) != 0)
            return FALSE;
    }

    vsi_l_offset nDataLen = 0;

    if (pszFilename == nullptr || strcmp(pszFilename, "/vsistdin/") == 0)
    {
        vsi_l_offset nDataAlloc = 0;
        if (VSIFSeekL(fp, 0, SEEK_SET) != 0)
        {
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }
        while (true)
        {
            if (nDataLen + 8192 + 1 > nDataAlloc)
            {
                nDataAlloc = (nDataAlloc * 4) / 3 + 8192 + 1;
                if (nDataAlloc >
                    static_cast<vsi_l_offset>(static_cast<size_t>(nDataAlloc)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Input file too large to be opened");
                    VSIFree(*ppabyRet);
                    *ppabyRet = nullptr;
                    if (bFreeFP)
                        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                    return FALSE;
                }
                GByte *pabyNew = static_cast<GByte *>(
                    VSIRealloc(*ppabyRet, static_cast<size_t>(nDataAlloc)));
                if (pabyNew == nullptr)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate " CPL_FRMT_GIB " bytes",
                             nDataAlloc);
                    VSIFree(*ppabyRet);
                    *ppabyRet = nullptr;
                    if (bFreeFP)
                        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                    return FALSE;
                }
                *ppabyRet = pabyNew;
            }
            const int nRead =
                static_cast<int>(VSIFReadL(*ppabyRet + nDataLen, 1, 8192, fp));
            nDataLen += nRead;

            if (nMaxSize >= 0 && nDataLen > static_cast<vsi_l_offset>(nMaxSize))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Input file too large to be opened");
                VSIFree(*ppabyRet);
                *ppabyRet = nullptr;
                if (pnSize != nullptr)
                    *pnSize = 0;
                if (bFreeFP)
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return FALSE;
            }

            if (pnSize != nullptr)
                *pnSize += nRead;
            (*ppabyRet)[nDataLen] = '\0';
            if (nRead == 0)
                break;
        }
    }
    else
    {
        if (VSIFSeekL(fp, 0, SEEK_END) != 0)
        {
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }
        nDataLen = VSIFTellL(fp);

        // With "large" VSI I/O API we can read data chunks larger than
        // VSIMalloc could allocate. Catch it here.
        if (nDataLen !=
                static_cast<vsi_l_offset>(static_cast<size_t>(nDataLen)) ||
            nDataLen + 1 < nDataLen
            // opening a directory returns nDataLen = INT_MAX (on 32bit) or
            // INT64_MAX (on 64bit)
            || nDataLen + 1 > std::numeric_limits<size_t>::max() / 2 ||
            (nMaxSize >= 0 && nDataLen > static_cast<vsi_l_offset>(nMaxSize)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Input file too large to be opened");
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }

        if (VSIFSeekL(fp, 0, SEEK_SET) != 0)
        {
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }

        *ppabyRet =
            static_cast<GByte *>(VSIMalloc(static_cast<size_t>(nDataLen + 1)));
        if (nullptr == *ppabyRet)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate " CPL_FRMT_GIB " bytes", nDataLen + 1);
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }

        (*ppabyRet)[nDataLen] = '\0';
        if (nDataLen !=
            VSIFReadL(*ppabyRet, 1, static_cast<size_t>(nDataLen), fp))
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot read " CPL_FRMT_GIB " bytes", nDataLen);
            VSIFree(*ppabyRet);
            *ppabyRet = nullptr;
            if (bFreeFP)
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return FALSE;
        }
        if (pnSize != nullptr)
            *pnSize = nDataLen;
    }
    if (bFreeFP)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    return TRUE;
}

/************************************************************************/
/*                         VSIOverwriteFile()                           */
/************************************************************************/

/**
 * \brief Overwrite an existing file with content from another one
 *
 * @param fpTarget file handle opened with VSIFOpenL() with "rb+" flag.
 * @param pszSourceFilename source filename
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 3.1
 */

int VSIOverwriteFile(VSILFILE *fpTarget, const char *pszSourceFilename)
{
    VSILFILE *fpSource = VSIFOpenL(pszSourceFilename, "rb");
    if (fpSource == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszSourceFilename);
        return false;
    }

    const size_t nBufferSize = 4096;
    void *pBuffer = CPLMalloc(nBufferSize);
    VSIFSeekL(fpTarget, 0, SEEK_SET);
    bool bRet = true;
    while (true)
    {
        size_t nRead = VSIFReadL(pBuffer, 1, nBufferSize, fpSource);
        size_t nWritten = VSIFWriteL(pBuffer, 1, nRead, fpTarget);
        if (nWritten != nRead)
        {
            bRet = false;
            break;
        }
        if (nRead < nBufferSize)
            break;
    }

    if (bRet)
    {
        bRet = VSIFTruncateL(fpTarget, VSIFTellL(fpTarget)) == 0;
        if (!bRet)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Truncation failed");
        }
    }

    CPLFree(pBuffer);
    VSIFCloseL(fpSource);
    return bRet;
}

/************************************************************************/
/*                        VSIFGetNativeFileDescriptorL()                */
/************************************************************************/

/**
 * \fn VSIVirtualHandle::GetNativeFileDescriptor()
 * \brief Returns the "native" file descriptor for the virtual handle.
 *
 * This will only return a non-NULL value for "real" files handled by the
 * operating system (to be opposed to GDAL virtual file systems).
 *
 * On POSIX systems, this will be a integer value ("fd") cast as a void*.
 * On Windows systems, this will be the HANDLE.
 *
 * @return the native file descriptor, or NULL.
 */

/**
 * \brief Returns the "native" file descriptor for the virtual handle.
 *
 * This will only return a non-NULL value for "real" files handled by the
 * operating system (to be opposed to GDAL virtual file systems).
 *
 * On POSIX systems, this will be a integer value ("fd") cast as a void*.
 * On Windows systems, this will be the HANDLE.
 *
 * @param fp file handle opened with VSIFOpenL().
 *
 * @return the native file descriptor, or NULL.
 */

void *VSIFGetNativeFileDescriptorL(VSILFILE *fp)
{
    return fp->GetNativeFileDescriptor();
}

/************************************************************************/
/*                      VSIGetDiskFreeSpace()                           */
/************************************************************************/

/**
 * \brief Return free disk space available on the filesystem.
 *
 * This function returns the free disk space available on the filesystem.
 *
 * @param pszDirname a directory of the filesystem to query.
 * @return The free space in bytes. Or -1 in case of error.
 * @since GDAL 2.1
 */

GIntBig VSIGetDiskFreeSpace(const char *pszDirname)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszDirname);

    return poFSHandler->GetDiskFreeSpace(pszDirname);
}

/************************************************************************/
/*                    VSIGetFileSystemsPrefixes()                       */
/************************************************************************/

/**
 * \brief Return the list of prefixes for virtual file system handlers
 * currently registered.
 *
 * Typically: "", "/vsimem/", "/vsicurl/", etc
 *
 * @return a NULL terminated list of prefixes. Must be freed with CSLDestroy()
 * @since GDAL 2.3
 */

char **VSIGetFileSystemsPrefixes(void)
{
    return VSIFileManager::GetPrefixes();
}

/************************************************************************/
/*                     VSIGetFileSystemOptions()                        */
/************************************************************************/

/**
 * \brief Return the list of options associated with a virtual file system
 * handler as a serialized XML string.
 *
 * Those options may be set as configuration options with CPLSetConfigOption().
 *
 * @param pszFilename a filename, or prefix of a virtual file system handler.
 * @return a string, which must not be freed, or NULL if no options is declared.
 * @since GDAL 2.3
 */

const char *VSIGetFileSystemOptions(const char *pszFilename)
{
    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(pszFilename);

    return poFSHandler->GetOptions();
}

/************************************************************************/
/*                       VSISetPathSpecificOption()                     */
/************************************************************************/

static std::mutex oMutexPathSpecificOptions;

// key is a path prefix
// value is a map of key, value pair
static std::map<std::string, std::map<std::string, std::string>>
    oMapPathSpecificOptions;

/**
 * \brief Set a credential (or more generally an option related to a
 *        virtual file system) for a given path prefix.
 * @deprecated in GDAL 3.6 for the better named VSISetPathSpecificOption()
 * @see VSISetPathSpecificOption()
 */
void VSISetCredential(const char *pszPathPrefix, const char *pszKey,
                      const char *pszValue)
{
    VSISetPathSpecificOption(pszPathPrefix, pszKey, pszValue);
}

/**
 * \brief Set a path specific option for a given path prefix.
 *
 * Such option is typically, but not limited to, a credential setting for a
 * virtual file system.
 *
 * That option may also be set as a configuration option with
 * CPLSetConfigOption(), but this function allows to specify them with a
 * granularity at the level of a file path, which makes it easier if using the
 * same virtual file system but with different credentials (e.g. different
 * credentials for bucket "/vsis3/foo" and "/vsis3/bar")
 *
 * This is supported for the following virtual file systems:
 * /vsis3/, /vsigs/, /vsiaz/, /vsioss/, /vsiwebhdfs, /vsiswift.
 * Note: setting them for a path starting with /vsiXXX/ will also apply for
 * /vsiXXX_streaming/ requests.
 *
 * Note that no particular care is taken to store them in RAM in a secure way.
 * So they might accidentally hit persistent storage if swapping occurs, or
 * someone with access to the memory allocated by the process may be able to
 * read them.
 *
 * @param pszPathPrefix a path prefix of a virtual file system handler.
 *                      Typically of the form "/vsiXXX/bucket". Must NOT be
 * NULL. Should not include trailing slashes.
 * @param pszKey        Option name. Must NOT be NULL.
 * @param pszValue      Option value. May be NULL to erase it.
 *
 * @since GDAL 3.6
 */

void VSISetPathSpecificOption(const char *pszPathPrefix, const char *pszKey,
                              const char *pszValue)
{
    std::lock_guard<std::mutex> oLock(oMutexPathSpecificOptions);
    auto oIter = oMapPathSpecificOptions.find(pszPathPrefix);
    CPLString osKey(pszKey);
    osKey.toupper();
    if (oIter == oMapPathSpecificOptions.end())
    {
        if (pszValue != nullptr)
            oMapPathSpecificOptions[pszPathPrefix][osKey] = pszValue;
    }
    else if (pszValue != nullptr)
        oIter->second[osKey] = pszValue;
    else
        oIter->second.erase(osKey);
}

/************************************************************************/
/*                       VSIClearPathSpecificOptions()                  */
/************************************************************************/

/**
 * \brief Clear path specific options set with VSISetPathSpecificOption()
 * @deprecated in GDAL 3.6 for the better named VSIClearPathSpecificOptions()
 * @see VSIClearPathSpecificOptions()
 */
void VSIClearCredentials(const char *pszPathPrefix)
{
    return VSIClearPathSpecificOptions(pszPathPrefix);
}

/**
 * \brief Clear path specific options set with VSISetPathSpecificOption()
 *
 * Note that no particular care is taken to remove them from RAM in a secure
 * way.
 *
 * @param pszPathPrefix If set to NULL, all path specific options are cleared.
 *                      If set to not-NULL, only those set with
 *                      VSISetPathSpecificOption(pszPathPrefix, ...) will be
 * cleared.
 *
 * @since GDAL 3.6
 */
void VSIClearPathSpecificOptions(const char *pszPathPrefix)
{
    std::lock_guard<std::mutex> oLock(oMutexPathSpecificOptions);
    if (pszPathPrefix == nullptr)
    {
        oMapPathSpecificOptions.clear();
    }
    else
    {
        oMapPathSpecificOptions.erase(pszPathPrefix);
    }
}

/************************************************************************/
/*                        VSIGetPathSpecificOption()                    */
/************************************************************************/

/**
 * \brief Get the value of a credential (or more generally an option related to
 * a virtual file system) for a given path.
 * @deprecated in GDAL 3.6 for the better named VSIGetPathSpecificOption()
 * @see VSIGetPathSpecificOption()
 */
const char *VSIGetCredential(const char *pszPath, const char *pszKey,
                             const char *pszDefault)
{
    return VSIGetPathSpecificOption(pszPath, pszKey, pszDefault);
}

/**
 * \brief Get the value a path specific option.
 *
 * Such option is typically, but not limited to, a credential setting for a
 * virtual file system.
 *
 * If no match occurs, CPLGetConfigOption(pszKey, pszDefault) is returned.
 *
 * Mostly to be used by virtual file system implementations.
 *
 * @since GDAL 3.6
 * @see VSISetPathSpecificOption()
 */
const char *VSIGetPathSpecificOption(const char *pszPath, const char *pszKey,
                                     const char *pszDefault)
{
    {
        std::lock_guard<std::mutex> oLock(oMutexPathSpecificOptions);
        for (auto it = oMapPathSpecificOptions.rbegin();
             it != oMapPathSpecificOptions.rend(); ++it)
        {
            if (STARTS_WITH(pszPath, it->first.c_str()))
            {
                auto oIter = it->second.find(CPLString(pszKey).toupper());
                if (oIter != it->second.end())
                    return oIter->second.c_str();
            }
        }
    }
    return CPLGetConfigOption(pszKey, pszDefault);
}

/************************************************************************/
/*                      VSIDuplicateFileSystemHandler()                 */
/************************************************************************/

/**
 * \brief Duplicate an existing file system handler.
 *
 * A number of virtual file system for remote object stores use protocols
 * identical or close to popular ones (typically AWS S3), but with slightly
 * different settings (at the very least the endpoint).
 *
 * This functions allows to duplicate the source virtual file system handler
 * as a new one with a different prefix (when the source virtual file system
 * handler supports the duplication operation).
 *
 * VSISetPathSpecificOption() will typically be called afterwards to change
 * configurable settings on the cloned file system handler (e.g. AWS_S3_ENDPOINT
 * for a clone of /vsis3/).
 *
 * @since GDAL 3.7
 */
bool VSIDuplicateFileSystemHandler(const char *pszSourceFSName,
                                   const char *pszNewFSName)
{
    VSIFilesystemHandler *poTargetFSHandler =
        VSIFileManager::GetHandler(pszNewFSName);
    if (poTargetFSHandler != VSIFileManager::GetHandler("/"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is already a known virtual file system", pszNewFSName);
        return false;
    }

    VSIFilesystemHandler *poSourceFSHandler =
        VSIFileManager::GetHandler(pszSourceFSName);
    if (!poSourceFSHandler)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is not a known virtual file system", pszSourceFSName);
        return false;
    }

    poTargetFSHandler = poSourceFSHandler->Duplicate(pszNewFSName);
    if (!poTargetFSHandler)
        return false;

    VSIFileManager::InstallHandler(pszNewFSName, poTargetFSHandler);
    return true;
}

/************************************************************************/
/* ==================================================================== */
/*                           VSIFileManager()                           */
/* ==================================================================== */
/************************************************************************/

#ifndef DOXYGEN_SKIP

/*
** Notes on Multithreading:
**
** The VSIFileManager maintains a list of file type handlers (mem, large
** file, etc).  It should be thread safe.
**/

/************************************************************************/
/*                           VSIFileManager()                           */
/************************************************************************/

VSIFileManager::VSIFileManager() : poDefaultHandler(nullptr)
{
}

/************************************************************************/
/*                          ~VSIFileManager()                           */
/************************************************************************/

VSIFileManager::~VSIFileManager()
{
    std::set<VSIFilesystemHandler *> oSetAlreadyDeleted;
    for (std::map<std::string, VSIFilesystemHandler *>::const_iterator iter =
             oHandlers.begin();
         iter != oHandlers.end(); ++iter)
    {
        if (oSetAlreadyDeleted.find(iter->second) == oSetAlreadyDeleted.end())
        {
            oSetAlreadyDeleted.insert(iter->second);
            delete iter->second;
        }
    }

    delete poDefaultHandler;
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

static VSIFileManager *poManager = nullptr;
static CPLMutex *hVSIFileManagerMutex = nullptr;

VSIFileManager *VSIFileManager::Get()
{
    CPLMutexHolder oHolder(&hVSIFileManagerMutex);
    if (poManager != nullptr)
    {
        return poManager;
    }

    poManager = new VSIFileManager;
    VSIInstallLargeFileHandler();
    VSIInstallSubFileHandler();
    VSIInstallMemFileHandler();
#ifdef HAVE_LIBZ
    VSIInstallGZipFileHandler();
    VSIInstallZipFileHandler();
#endif
#ifdef HAVE_LIBARCHIVE
    VSIInstall7zFileHandler();
    VSIInstallRarFileHandler();
#endif
#ifdef HAVE_CURL
    VSIInstallCurlFileHandler();
    VSIInstallCurlStreamingFileHandler();
    VSIInstallS3FileHandler();
    VSIInstallS3StreamingFileHandler();
    VSIInstallGSFileHandler();
    VSIInstallGSStreamingFileHandler();
    VSIInstallAzureFileHandler();
    VSIInstallAzureStreamingFileHandler();
    VSIInstallADLSFileHandler();
    VSIInstallOSSFileHandler();
    VSIInstallOSSStreamingFileHandler();
    VSIInstallSwiftFileHandler();
    VSIInstallSwiftStreamingFileHandler();
    VSIInstallWebHdfsHandler();
#endif
    VSIInstallStdinHandler();
    VSIInstallHdfsHandler();
    VSIInstallStdoutHandler();
    VSIInstallSparseFileHandler();
    VSIInstallTarFileHandler();
    VSIInstallCachedFileHandler();
    VSIInstallCryptFileHandler();

    return poManager;
}

/************************************************************************/
/*                           GetPrefixes()                              */
/************************************************************************/

char **VSIFileManager::GetPrefixes()
{
    CPLMutexHolder oHolder(&hVSIFileManagerMutex);
    CPLStringList aosList;
    for (const auto &oIter : Get()->oHandlers)
    {
        if (oIter.first != "/vsicurl?")
        {
            aosList.AddString(oIter.first.c_str());
        }
    }
    return aosList.StealList();
}

/************************************************************************/
/*                             GetHandler()                             */
/************************************************************************/

VSIFilesystemHandler *VSIFileManager::GetHandler(const char *pszPath)

{
    VSIFileManager *poThis = Get();
    const size_t nPathLen = strlen(pszPath);

    for (std::map<std::string, VSIFilesystemHandler *>::const_iterator iter =
             poThis->oHandlers.begin();
         iter != poThis->oHandlers.end(); ++iter)
    {
        const char *pszIterKey = iter->first.c_str();
        const size_t nIterKeyLen = iter->first.size();
        if (strncmp(pszPath, pszIterKey, nIterKeyLen) == 0)
            return iter->second;

        // "/vsimem\foo" should be handled as "/vsimem/foo".
        if (nIterKeyLen && nPathLen > nIterKeyLen &&
            pszIterKey[nIterKeyLen - 1] == '/' &&
            pszPath[nIterKeyLen - 1] == '\\' &&
            strncmp(pszPath, pszIterKey, nIterKeyLen - 1) == 0)
            return iter->second;

        // /vsimem should be treated as a match for /vsimem/.
        if (nPathLen + 1 == nIterKeyLen &&
            strncmp(pszPath, pszIterKey, nPathLen) == 0)
            return iter->second;
    }

    return poThis->poDefaultHandler;
}

/************************************************************************/
/*                           InstallHandler()                           */
/************************************************************************/

void VSIFileManager::InstallHandler(const std::string &osPrefix,
                                    VSIFilesystemHandler *poHandler)

{
    if (osPrefix == "")
        Get()->poDefaultHandler = poHandler;
    else
        Get()->oHandlers[osPrefix] = poHandler;
}

/************************************************************************/
/*                          RemoveHandler()                             */
/************************************************************************/

void VSIFileManager::RemoveHandler(const std::string &osPrefix)
{
    if (osPrefix == "")
        Get()->poDefaultHandler = nullptr;
    else
        Get()->oHandlers.erase(osPrefix);
}

/************************************************************************/
/*                       VSICleanupFileManager()                        */
/************************************************************************/

void VSICleanupFileManager()

{
    if (poManager)
    {
        delete poManager;
        poManager = nullptr;
    }

    if (hVSIFileManagerMutex != nullptr)
    {
        CPLDestroyMutex(hVSIFileManagerMutex);
        hVSIFileManagerMutex = nullptr;
    }

#ifdef HAVE_CURL
    VSICURLDestroyCacheFileProp();
#endif
}

/************************************************************************/
/*                            Truncate()                                */
/************************************************************************/

int VSIVirtualHandle::Truncate(vsi_l_offset nNewSize)
{
    const vsi_l_offset nOriginalPos = Tell();
    if (Seek(0, SEEK_END) == 0 && nNewSize >= Tell())
    {
        // Fill with zeroes
        std::vector<GByte> aoBytes(4096, 0);
        vsi_l_offset nCurOffset = nOriginalPos;
        while (nCurOffset < nNewSize)
        {
            constexpr vsi_l_offset nMaxOffset = 4096;
            const int nSize =
                static_cast<int>(std::min(nMaxOffset, nNewSize - nCurOffset));
            if (Write(&aoBytes[0], nSize, 1) != 1)
            {
                Seek(nOriginalPos, SEEK_SET);
                return -1;
            }
            nCurOffset += nSize;
        }
        return Seek(nOriginalPos, SEEK_SET) == 0 ? 0 : -1;
    }

    CPLDebug("VSI", "Truncation is not supported in generic implementation "
                    "of Truncate()");
    Seek(nOriginalPos, SEEK_SET);
    return -1;
}

/************************************************************************/
/*                           ReadMultiRange()                           */
/************************************************************************/

int VSIVirtualHandle::ReadMultiRange(int nRanges, void **ppData,
                                     const vsi_l_offset *panOffsets,
                                     const size_t *panSizes)
{
    int nRet = 0;
    const vsi_l_offset nCurOffset = Tell();
    for (int i = 0; i < nRanges; i++)
    {
        if (Seek(panOffsets[i], SEEK_SET) < 0)
        {
            nRet = -1;
            break;
        }

        const size_t nRead = Read(ppData[i], 1, panSizes[i]);
        if (panSizes[i] != nRead)
        {
            nRet = -1;
            break;
        }
    }

    Seek(nCurOffset, SEEK_SET);

    return nRet;
}

#endif  // #ifndef DOXYGEN_SKIP

/************************************************************************/
/*                            HasPRead()                                */
/************************************************************************/

/** Returns whether this file handle supports the PRead() method.
 *
 * @since GDAL 3.6
 */
bool VSIVirtualHandle::HasPRead() const
{
    return false;
}

/************************************************************************/
/*                             PRead()                                  */
/************************************************************************/

/** Do a parallel-compatible read operation.
 *
 * This methods reads into pBuffer up to nSize bytes starting at offset nOffset
 * in the file. The current file offset is not affected by this method.
 *
 * The implementation is thread-safe: several threads can issue PRead()
 * concurrently on the same VSIVirtualHandle object.
 *
 * This method has the same semantics as pread() Linux operation. It is only
 * available if HasPRead() returns true.
 *
 * @param pBuffer output buffer (must be at least nSize bytes large).
 * @param nSize   number of bytes to read in the file.
 * @param nOffset file offset from which to read.
 * @return number of bytes read.
 * @since GDAL 3.6
 */
size_t VSIVirtualHandle::PRead(CPL_UNUSED void *pBuffer,
                               CPL_UNUSED size_t nSize,
                               CPL_UNUSED vsi_l_offset nOffset) const
{
    return 0;
}
