/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for archive files.
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "cpl_vsi_virtual.h"

#include <cstring>
#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include <ctime>
#include <map>
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

CPL_CVSID("$Id$");

static bool IsEitherSlash( char c )
{
    return c == '/' || c == '\\';
}

/************************************************************************/
/*                    ~VSIArchiveEntryFileOffset()                      */
/************************************************************************/

VSIArchiveEntryFileOffset::~VSIArchiveEntryFileOffset() {}

/************************************************************************/
/*                        ~VSIArchiveReader()                           */
/************************************************************************/

VSIArchiveReader::~VSIArchiveReader() {}

/************************************************************************/
/*                        ~VSIArchiveContent()                          */
/************************************************************************/

VSIArchiveContent::~VSIArchiveContent()
{
    for( int i = 0; i < nEntries; i++ )
    {
        delete entries[i].file_pos;
        CPLFree(entries[i].fileName);
    }
    CPLFree(entries);
}

/************************************************************************/
/*                   VSIArchiveFilesystemHandler()                      */
/************************************************************************/

VSIArchiveFilesystemHandler::VSIArchiveFilesystemHandler()
{
    hMutex = NULL;
}

/************************************************************************/
/*                   ~VSIArchiveFilesystemHandler()                     */
/************************************************************************/

VSIArchiveFilesystemHandler::~VSIArchiveFilesystemHandler()

{
    for( std::map<CPLString, VSIArchiveContent*>::const_iterator iter =
             oFileList.begin();
         iter != oFileList.end();
         ++iter )
    {
        delete iter->second;
    }

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}


/************************************************************************/
/*                       GetStrippedFilename()                          */
/************************************************************************/

static CPLString GetStrippedFilename(const CPLString& osFileName,
                                     bool& bIsDir)
{
    bIsDir = false;
    const char* fileName = osFileName.c_str();

    // Remove ./ pattern at the beginning of a filename.
    if( fileName[0] == '.' && fileName[1] == '/' )
    {
        fileName += 2;
        if( fileName[0] == '\0' )
            return CPLString();
    }

    char* pszStrippedFileName = CPLStrdup(fileName);
    char* pszIter = NULL;
    for( pszIter = pszStrippedFileName; *pszIter; pszIter++ )
    {
        if( *pszIter == '\\' )
            *pszIter = '/';
    }

    const size_t nLen = strlen(fileName);
    bIsDir = nLen > 0 && fileName[nLen-1] == '/';
    if( bIsDir )
    {
        // Remove trailing slash.
        pszStrippedFileName[nLen-1] = '\0';
    }
    CPLString osRet(pszStrippedFileName);
    CPLFree(pszStrippedFileName);
    return osRet;
}

/************************************************************************/
/*                       GetContentOfArchive()                          */
/************************************************************************/

const VSIArchiveContent* VSIArchiveFilesystemHandler::GetContentOfArchive(
    const char* archiveFilename, VSIArchiveReader* poReader )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIStatBufL sStat;
    if( VSIStatL(archiveFilename, &sStat) != 0 )
        return NULL;

    if( oFileList.find(archiveFilename) != oFileList.end() )
    {
        VSIArchiveContent* content = oFileList[archiveFilename];
        if( (time_t)sStat.st_mtime > content->mTime ||
            (vsi_l_offset)sStat.st_size != content->nFileSize)
        {
            CPLDebug("VSIArchive",
                     "The content of %s has changed since it was cached",
                     archiveFilename);
            delete content;
            oFileList.erase(archiveFilename);
        }
        else
        {
            return content;
        }
    }

    bool bMustClose = poReader == NULL;
    if( poReader == NULL )
    {
        poReader = CreateReader(archiveFilename);
        if( !poReader )
            return NULL;
    }

    if( poReader->GotoFirstFile() == FALSE )
    {
        if( bMustClose )
            delete(poReader);
        return NULL;
    }

    VSIArchiveContent* content = new VSIArchiveContent;
    content->mTime = sStat.st_mtime;
    content->nFileSize = (vsi_l_offset)sStat.st_size;
    content->nEntries = 0;
    content->entries = NULL;
    oFileList[archiveFilename] = content;

    std::set<CPLString> oSet;

    do
    {
        const CPLString osFileName = poReader->GetFileName();
        bool bIsDir = false;
        const CPLString osStrippedFilename =
                                GetStrippedFilename(osFileName, bIsDir);
        if( osStrippedFilename.empty() )
            continue;

        if( oSet.find(osStrippedFilename) == oSet.end() )
        {
            oSet.insert(osStrippedFilename);

            // Add intermediate directory structure.
            const char* pszBegin = osStrippedFilename.c_str();
            for( const char* pszIter = pszBegin; *pszIter; pszIter++ )
            {
                if( *pszIter == '/' )
                {
                    char* pszStrippedFileName2 = CPLStrdup(osStrippedFilename);
                    pszStrippedFileName2[pszIter - pszBegin] = 0;
                    if( oSet.find(pszStrippedFileName2) == oSet.end() )
                    {
                        oSet.insert(pszStrippedFileName2);

                        content->entries = static_cast<VSIArchiveEntry *>(
                            CPLRealloc(
                                content->entries,
                                sizeof(VSIArchiveEntry) *
                                (content->nEntries + 1)));
                        content->entries[content->nEntries].fileName =
                            pszStrippedFileName2;
                        content->entries[content->nEntries].nModifiedTime =
                            poReader->GetModifiedTime();
                        content->entries[content->nEntries].uncompressed_size =
                            0;
                        content->entries[content->nEntries].bIsDir = TRUE;
                        content->entries[content->nEntries].file_pos = NULL;
#ifdef DEBUG_VERBOSE
                        const int nEntries = content->nEntries;
                        CPLDebug(
                            "VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes",
                            content->nEntries + 1,
                            content->entries[nEntries].fileName,
                            content->entries[nEntries].uncompressed_size);
#endif
                        content->nEntries++;
                    }
                    else
                    {
                        CPLFree(pszStrippedFileName2);
                    }
                }
            }

            content->entries = static_cast<VSIArchiveEntry *>(
                CPLRealloc(content->entries,
                           sizeof(VSIArchiveEntry) * (content->nEntries + 1)));
            content->entries[content->nEntries].fileName =
                CPLStrdup(osStrippedFilename);
            content->entries[content->nEntries].nModifiedTime =
                poReader->GetModifiedTime();
            content->entries[content->nEntries].uncompressed_size =
                poReader->GetFileSize();
            content->entries[content->nEntries].bIsDir = bIsDir;
            content->entries[content->nEntries].file_pos =
                poReader->GetFileOffset();
#ifdef DEBUG_VERBOSE
            CPLDebug("VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes",
                     content->nEntries+1,
                     content->entries[content->nEntries].fileName,
                     content->entries[content->nEntries].uncompressed_size);
#endif
            content->nEntries++;
        }

    } while( poReader->GotoNextFile() );

    if( bMustClose )
        delete(poReader);

    return content;
}

/************************************************************************/
/*                        FindFileInArchive()                           */
/************************************************************************/

int
VSIArchiveFilesystemHandler::FindFileInArchive(
    const char* archiveFilename,
    const char* fileInArchiveName,
    const VSIArchiveEntry** archiveEntry )
{
    if( fileInArchiveName == NULL )
        return FALSE;

    const VSIArchiveContent* content = GetContentOfArchive(archiveFilename);
    if( content )
    {
        for( int i = 0; i < content->nEntries; i++ )
        {
            if( strcmp(fileInArchiveName, content->entries[i].fileName) == 0)
            {
                if( archiveEntry )
                    *archiveEntry = &content->entries[i];
                return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************/
/*                           CompactFilename()                          */
/************************************************************************/

static CPLString CompactFilename( const char* pszArchiveInFileNameIn )
{
    char* pszArchiveInFileName = CPLStrdup(pszArchiveInFileNameIn);

    // Replace a/../b by b and foo/a/../b by foo/b.
    while( true )
    {
        char* pszPrevDir = strstr(pszArchiveInFileName, "/../");
        if( pszPrevDir == NULL || pszPrevDir == pszArchiveInFileName )
            break;

        char* pszPrevSlash = pszPrevDir - 1;
        while( pszPrevSlash != pszArchiveInFileName &&
               *pszPrevSlash != '/' )
            pszPrevSlash--;
        if( pszPrevSlash == pszArchiveInFileName )
            memmove(pszArchiveInFileName,
                    pszPrevDir + 4,
                    strlen(pszPrevDir + 4) + 1);
        else
            memmove(pszPrevSlash + 1,
                    pszPrevDir + 4,
                    strlen(pszPrevDir + 4) + 1);
    }

    CPLString osFileInArchive = pszArchiveInFileName;
    CPLFree(pszArchiveInFileName);
    return osFileInArchive;
}

/************************************************************************/
/*                           SplitFilename()                            */
/************************************************************************/

char* VSIArchiveFilesystemHandler::SplitFilename( const char *pszFilename,
                                                  CPLString &osFileInArchive,
                                                  int bCheckMainFileExists )
{
    // TODO(schwehr): Cleanup redundant calls to GetPrefix and strlen.
    if( strcmp(pszFilename, GetPrefix()) == 0 )
        return NULL;

    int i = 0;

    // Detect extended syntax: /vsiXXX/{archive_filename}/file_in_archive.
    if( pszFilename[strlen(GetPrefix()) + 1] == '{' )
    {
        pszFilename += strlen(GetPrefix()) + 1;
        int nCountCurlies = 0;
        while( pszFilename[i] )
        {
            if( pszFilename[i] == '{' )
                nCountCurlies++;
            else if( pszFilename[i] == '}' )
            {
                nCountCurlies--;
                if( nCountCurlies == 0 )
                    break;
            }
            i++;
        }
        if( nCountCurlies > 0 )
            return NULL;
        char* archiveFilename = CPLStrdup(pszFilename + 1);
        archiveFilename[i - 1] = 0;

        bool bArchiveFileExists = false;
        if( !bCheckMainFileExists )
        {
            bArchiveFileExists = true;
        }
        else
        {
            CPLMutexHolder oHolder( &hMutex );

            if( oFileList.find(archiveFilename) != oFileList.end() )
            {
                bArchiveFileExists = true;
            }
        }

        if( !bArchiveFileExists )
        {
            VSIStatBufL statBuf;
            VSIFilesystemHandler *poFSHandler =
                VSIFileManager::GetHandler( archiveFilename );
            if( poFSHandler->Stat(
                    archiveFilename, &statBuf,
                    VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                !VSI_ISDIR(statBuf.st_mode) )
            {
                bArchiveFileExists = true;
            }
        }

        if( bArchiveFileExists )
        {
            if( IsEitherSlash(pszFilename[i + 1]) )
            {
                osFileInArchive = CompactFilename(pszFilename + i + 2);
            }
            else if( pszFilename[i+1] == '\0' )
            {
                osFileInArchive = "";
            }
            else
            {
                CPLFree(archiveFilename);
                return NULL;
            }

            // Remove trailing slash.
            if( !osFileInArchive.empty() )
            {
                const char lastC = osFileInArchive.back();
                if( IsEitherSlash(lastC) )
                    osFileInArchive.resize(osFileInArchive.size() - 1);
            }

            return archiveFilename;
        }

        CPLFree(archiveFilename);
        return NULL;
    }

    // Allow natural chaining of VSI drivers without requiring double slash.

    CPLString osDoubleVsi(GetPrefix());
    osDoubleVsi += "/vsi";

    if( strncmp(pszFilename, osDoubleVsi.c_str(), osDoubleVsi.size()) == 0 )
        pszFilename += strlen(GetPrefix());
    else
        pszFilename += strlen(GetPrefix()) + 1;

    // Parsing strings like /vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar//vsitar/a.tgzb.tgzc.tgzd.tgze.tgzf.tgz.h.tgz.i.tgz
    // takes a huge amount of time, so limit the number of nesting of such
    // file systems.
    int* pnCounter = static_cast<int*>(CPLGetTLS(CTLS_ABSTRACTARCHIVE_SPLIT));
    if( pnCounter == NULL )
    {
        pnCounter = static_cast<int*>(CPLMalloc(sizeof(int)));
        *pnCounter = 0;
        CPLSetTLS(CTLS_ABSTRACTARCHIVE_SPLIT, pnCounter, TRUE);
    }
    if( *pnCounter == 3 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too deep recursion level in "
                 "VSIArchiveFilesystemHandler::SplitFilename()");
        return NULL;
    }

    const std::vector<CPLString> oExtensions = GetExtensions();
    while( pszFilename[i] )
    {
        int nToSkip = 0;

        for( std::vector<CPLString>::const_iterator iter = oExtensions.begin();
             iter != oExtensions.end();
             ++iter )
        {
            const CPLString& osExtension = *iter;
            if( EQUALN(pszFilename + i,
                       osExtension.c_str(),
                       osExtension.size()) )
            {
                nToSkip = static_cast<int>(osExtension.size());
                break;
            }
        }

#ifdef DEBUG
        // For AFL, so that .cur_input is detected as the archive filename.
        if( EQUALN( pszFilename + i, ".cur_input", strlen(".cur_input") ) )
        {
            nToSkip = static_cast<int>(strlen(".cur_input"));
        }
#endif

        if( nToSkip != 0 )
        {
            VSIStatBufL statBuf;
            char* archiveFilename = CPLStrdup(pszFilename);
            bool bArchiveFileExists = false;

            if( IsEitherSlash(archiveFilename[i + nToSkip]) )
            {
                archiveFilename[i + nToSkip] = 0;
            }

            if( !bCheckMainFileExists )
            {
                bArchiveFileExists = true;
            }
            else
            {
                CPLMutexHolder oHolder( &hMutex );

                if( oFileList.find(archiveFilename) != oFileList.end() )
                {
                    bArchiveFileExists = true;
                }
            }

            if( !bArchiveFileExists )
            {
                (*pnCounter) ++;

                VSIFilesystemHandler *poFSHandler =
                    VSIFileManager::GetHandler( archiveFilename );
                if( poFSHandler->Stat(
                        archiveFilename, &statBuf,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                    !VSI_ISDIR(statBuf.st_mode))
                {
                    bArchiveFileExists = true;
                }

                (*pnCounter) --;
            }

            if( bArchiveFileExists )
            {
                if( IsEitherSlash(pszFilename[i + nToSkip]) )
                {
                    osFileInArchive =
                        CompactFilename(pszFilename + i + nToSkip + 1);
                }
                else
                {
                    osFileInArchive = "";
                }

                // Remove trailing slash.
                if( !osFileInArchive.empty() )
                {
                    const char lastC = osFileInArchive.back();
                    if( IsEitherSlash(lastC) )
                        osFileInArchive.resize(osFileInArchive.size() - 1);
                }

                return archiveFilename;
            }
            CPLFree(archiveFilename);
        }
        i++;
    }
    return NULL;
}

/************************************************************************/
/*                           OpenArchiveFile()                          */
/************************************************************************/

VSIArchiveReader *
VSIArchiveFilesystemHandler::OpenArchiveFile( const char* archiveFilename,
                                              const char* fileInArchiveName )
{
    VSIArchiveReader* poReader = CreateReader(archiveFilename);

    if( poReader == NULL )
    {
        return NULL;
    }

    if( fileInArchiveName == NULL || strlen(fileInArchiveName) == 0 )
    {
        if( poReader->GotoFirstFile() == FALSE )
        {
            delete(poReader);
            return NULL;
        }

        // Skip optional leading subdir.
        const CPLString osFileName = poReader->GetFileName();
        if( osFileName.empty() || IsEitherSlash(osFileName.back()) )
        {
            if( poReader->GotoNextFile() == FALSE )
            {
                delete(poReader);
                return NULL;
            }
        }

        if( poReader->GotoNextFile() )
        {
            CPLString msg;
            msg.Printf("Support only 1 file in archive file %s when "
                       "no explicit in-archive filename is specified",
                       archiveFilename);
            const VSIArchiveContent* content =
                GetContentOfArchive(archiveFilename, poReader);
            if( content )
            {
                msg += "\nYou could try one of the following :\n";
                for( int i=0; i < content->nEntries; i++ )
                {
                    msg += CPLString().Printf(
                        "  %s/%s/%s\n", GetPrefix(),
                        archiveFilename, content->entries[i].fileName);
                }
            }

            CPLError(CE_Failure, CPLE_NotSupported, "%s", msg.c_str());

            delete(poReader);
            return NULL;
        }
    }
    else
    {
        // Optimization: instead of iterating over all files which can be
        // slow on .tar.gz files, try reading the first one first.
        // This can help if it is really huge.
        {
            CPLMutexHolder oHolder( &hMutex );

            if( oFileList.find(archiveFilename) == oFileList.end() )
            {
                if( poReader->GotoFirstFile() == FALSE )
                {
                    delete(poReader);
                    return NULL;
                }

                const CPLString osFileName = poReader->GetFileName();
                bool bIsDir = false;
                const CPLString osStrippedFilename =
                            GetStrippedFilename(osFileName, bIsDir);
                if( !osStrippedFilename.empty() )
                {
                    const bool bMatch = strcmp(osStrippedFilename,
                                               fileInArchiveName) == 0;
                    if( bMatch )
                    {
                        if( bIsDir )
                        {
                            delete(poReader);
                            return NULL;
                        }
                        return poReader;
                    }
                }
            }
        }

        const VSIArchiveEntry* archiveEntry = NULL;
        if( FindFileInArchive(archiveFilename, fileInArchiveName,
                              &archiveEntry) == FALSE ||
            archiveEntry->bIsDir )
        {
            delete(poReader);
            return NULL;
        }
        if( !poReader->GotoFileOffset(archiveEntry->file_pos) )
        {
            delete poReader;
            return NULL;
        }
    }
    return poReader;
}

/************************************************************************/
/*                                 Stat()                               */
/************************************************************************/

int VSIArchiveFilesystemHandler::Stat( const char *pszFilename,
                                       VSIStatBufL *pStatBuf,
                                       int /* nFlags */ )
{
    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    CPLString osFileInArchive;
    char* archiveFilename = SplitFilename(pszFilename, osFileInArchive, TRUE);
    if( archiveFilename == NULL )
        return -1;

    int ret = -1;
    if( !osFileInArchive.empty() )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("VSIArchive", "Looking for %s %s",
                 archiveFilename, osFileInArchive.c_str());
#endif

        const VSIArchiveEntry* archiveEntry = NULL;
        if( FindFileInArchive(archiveFilename, osFileInArchive, &archiveEntry) )
        {
            // Patching st_size with uncompressed file size.
            pStatBuf->st_size = archiveEntry->uncompressed_size;
            pStatBuf->st_mtime = (time_t)archiveEntry->nModifiedTime;
            if( archiveEntry->bIsDir )
                pStatBuf->st_mode = S_IFDIR;
            else
                pStatBuf->st_mode = S_IFREG;
            ret = 0;
        }
    }
    else
    {
        VSIArchiveReader* poReader = CreateReader(archiveFilename);
        CPLFree(archiveFilename);
        archiveFilename = NULL;

        if( poReader != NULL && poReader->GotoFirstFile() )
        {
            // Skip optional leading subdir.
            const CPLString osFileName = poReader->GetFileName();
            if( IsEitherSlash(osFileName.back()) )
            {
                if( poReader->GotoNextFile() == FALSE )
                {
                    delete(poReader);
                    return -1;
                }
            }

            if( poReader->GotoNextFile() )
            {
                // Several files in archive --> treat as dir.
                pStatBuf->st_size = 0;
                pStatBuf->st_mode = S_IFDIR;
            }
            else
            {
                // Patching st_size with uncompressed file size.
                pStatBuf->st_size = poReader->GetFileSize();
                pStatBuf->st_mtime = (time_t)poReader->GetModifiedTime();
                pStatBuf->st_mode = S_IFREG;
            }

            ret = 0;
        }

        delete(poReader);
    }

    CPLFree(archiveFilename);
    return ret;
}

/************************************************************************/
/*                              Unlink()                                */
/************************************************************************/

int VSIArchiveFilesystemHandler::Unlink( const char * /* pszFilename */ )
{
    return -1;
}

/************************************************************************/
/*                             Rename()                                 */
/************************************************************************/

int VSIArchiveFilesystemHandler::Rename( const char * /* oldpath */,
                                         const char * /* newpath */ )
{
    return -1;
}

/************************************************************************/
/*                             Mkdir()                                  */
/************************************************************************/

int VSIArchiveFilesystemHandler::Mkdir( const char * /* pszDirname */,
                                        long /* nMode */ )
{
    return -1;
}

/************************************************************************/
/*                             Rmdir()                                  */
/************************************************************************/

int VSIArchiveFilesystemHandler::Rmdir( const char * /* pszDirname */ )
{
    return -1;
}

/************************************************************************/
/*                             ReadDirEx()                              */
/************************************************************************/

char** VSIArchiveFilesystemHandler::ReadDirEx( const char *pszDirname,
                                               int nMaxFiles )
{
    CPLString osInArchiveSubDir;
    char* archiveFilename = SplitFilename(pszDirname, osInArchiveSubDir, TRUE);
    if( archiveFilename == NULL )
        return NULL;

    const int lenInArchiveSubDir = static_cast<int>(osInArchiveSubDir.size());

    CPLStringList oDir;

    const VSIArchiveContent* content = GetContentOfArchive(archiveFilename);
    if( !content )
    {
        CPLFree(archiveFilename);
        return NULL;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("VSIArchive", "Read dir %s", pszDirname);
#endif
    for( int i = 0; i < content->nEntries; i++ )
    {
        const char* fileName = content->entries[i].fileName;
        /* Only list entries at the same level of inArchiveSubDir */
        if( lenInArchiveSubDir != 0 &&
            strncmp(fileName, osInArchiveSubDir, lenInArchiveSubDir) == 0 &&
            IsEitherSlash(fileName[lenInArchiveSubDir]) &&
            fileName[lenInArchiveSubDir + 1] != 0 )
        {
            const char* slash = strchr(fileName + lenInArchiveSubDir + 1, '/');
            if( slash == NULL )
                slash = strchr(fileName + lenInArchiveSubDir + 1, '\\');
            if( slash == NULL || slash[1] == 0 )
            {
                char* tmpFileName = CPLStrdup(fileName);
                if( slash != NULL )
                {
                    tmpFileName[strlen(tmpFileName)-1] = 0;
                }
#ifdef DEBUG_VERBOSE
                CPLDebug("VSIArchive", "Add %s as in directory %s",
                         tmpFileName + lenInArchiveSubDir + 1, pszDirname);
#endif
                oDir.AddString(tmpFileName + lenInArchiveSubDir + 1);
                CPLFree(tmpFileName);
            }
        }
        else if( lenInArchiveSubDir == 0 &&
                 strchr(fileName, '/') == NULL &&
                 strchr(fileName, '\\') == NULL )
        {
            // Only list toplevel files and directories.
#ifdef DEBUG_VERBOSE
            CPLDebug("VSIArchive", "Add %s as in directory %s",
                     fileName, pszDirname);
#endif
            oDir.AddString(fileName);
        }

        if( nMaxFiles > 0 && oDir.Count() > nMaxFiles )
            break;
    }

    CPLFree(archiveFilename);
    return oDir.StealList();
}

//! @endcond
