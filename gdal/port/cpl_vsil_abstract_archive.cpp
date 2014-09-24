/******************************************************************************
 * $Id$
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

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include <map>
#include <set>

#define ENABLE_DEBUG 0

CPL_CVSID("$Id$");

/************************************************************************/
/*                    ~VSIArchiveEntryFileOffset()                      */
/************************************************************************/

VSIArchiveEntryFileOffset::~VSIArchiveEntryFileOffset()
{
}

/************************************************************************/
/*                        ~VSIArchiveReader()                           */
/************************************************************************/

VSIArchiveReader::~VSIArchiveReader()
{
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
    std::map<CPLString,VSIArchiveContent*>::const_iterator iter;

    for( iter = oFileList.begin(); iter != oFileList.end(); ++iter )
    {
        VSIArchiveContent* content = iter->second;
        int i;
        for(i=0;i<content->nEntries;i++)
        {
            delete content->entries[i].file_pos;
            CPLFree(content->entries[i].fileName);
        }
        CPLFree(content->entries);
        delete content;
    }

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}

/************************************************************************/
/*                       GetContentOfArchive()                          */
/************************************************************************/

const VSIArchiveContent* VSIArchiveFilesystemHandler::GetContentOfArchive
        (const char* archiveFilename, VSIArchiveReader* poReader)
{
    CPLMutexHolder oHolder( &hMutex );

    if (oFileList.find(archiveFilename) != oFileList.end() )
    {
        return oFileList[archiveFilename];
    }

    int bMustClose = (poReader == NULL);
    if (poReader == NULL)
    {
        poReader = CreateReader(archiveFilename);
        if (!poReader)
            return NULL;
    }

    if (poReader->GotoFirstFile() == FALSE)
    {
        if (bMustClose)
            delete(poReader);
        return NULL;
    }

    VSIArchiveContent* content = new VSIArchiveContent;
    content->nEntries = 0;
    content->entries = NULL;
    oFileList[archiveFilename] = content;

    std::set<CPLString> oSet;

    do
    {
        CPLString osFileName = poReader->GetFileName();
        const char* fileName = osFileName.c_str();

        /* Remove ./ pattern at the beginning of a filename */
        if (fileName[0] == '.' && fileName[1] == '/')
        {
            fileName += 2;
            if (fileName[0] == '\0')
                continue;
        }

        char* pszStrippedFileName = CPLStrdup(fileName);
        char* pszIter;
        for(pszIter = pszStrippedFileName;*pszIter;pszIter++)
        {
            if (*pszIter == '\\')
                *pszIter = '/';
        }

        int bIsDir = strlen(fileName) > 0 &&
                      fileName[strlen(fileName)-1] == '/';
        if (bIsDir)
        {
            /* Remove trailing slash */
            pszStrippedFileName[strlen(fileName)-1] = 0;
        }

        if (oSet.find(pszStrippedFileName) == oSet.end())
        {
            oSet.insert(pszStrippedFileName);

            /* Add intermediate directory structure */
            for(pszIter = pszStrippedFileName;*pszIter;pszIter++)
            {
                if (*pszIter == '/')
                {
                    char* pszStrippedFileName2 = CPLStrdup(pszStrippedFileName);
                    pszStrippedFileName2[pszIter - pszStrippedFileName] = 0;
                    if (oSet.find(pszStrippedFileName2) == oSet.end())
                    {
                        oSet.insert(pszStrippedFileName2);

                        content->entries = (VSIArchiveEntry*)CPLRealloc(content->entries,
                                sizeof(VSIArchiveEntry) * (content->nEntries + 1));
                        content->entries[content->nEntries].fileName = pszStrippedFileName2;
                        content->entries[content->nEntries].nModifiedTime = poReader->GetModifiedTime();
                        content->entries[content->nEntries].uncompressed_size = 0;
                        content->entries[content->nEntries].bIsDir = TRUE;
                        content->entries[content->nEntries].file_pos = NULL;
                        if (ENABLE_DEBUG)
                            CPLDebug("VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes", content->nEntries+1,
                                content->entries[content->nEntries].fileName,
                                content->entries[content->nEntries].uncompressed_size);
                        content->nEntries++;
                    }
                    else
                    {
                        CPLFree(pszStrippedFileName2);
                    }
                }
            }

            content->entries = (VSIArchiveEntry*)CPLRealloc(content->entries,
                                sizeof(VSIArchiveEntry) * (content->nEntries + 1));
            content->entries[content->nEntries].fileName = pszStrippedFileName;
            content->entries[content->nEntries].nModifiedTime = poReader->GetModifiedTime();
            content->entries[content->nEntries].uncompressed_size = poReader->GetFileSize();
            content->entries[content->nEntries].bIsDir = bIsDir;
            content->entries[content->nEntries].file_pos = poReader->GetFileOffset();
            if (ENABLE_DEBUG)
                CPLDebug("VSIArchive", "[%d] %s : " CPL_FRMT_GUIB " bytes", content->nEntries+1,
                    content->entries[content->nEntries].fileName,
                    content->entries[content->nEntries].uncompressed_size);
            content->nEntries++;
        }
        else
        {
            CPLFree(pszStrippedFileName);
        }
    } while(poReader->GotoNextFile());

    if (bMustClose)
        delete(poReader);

    return content;
}

/************************************************************************/
/*                        FindFileInArchive()                           */
/************************************************************************/

int VSIArchiveFilesystemHandler::FindFileInArchive(const char* archiveFilename,
                                           const char* fileInArchiveName,
                                           const VSIArchiveEntry** archiveEntry)
{
    if (fileInArchiveName == NULL)
        return FALSE;

    const VSIArchiveContent* content = GetContentOfArchive(archiveFilename);
    if (content)
    {
        int i;
        for(i=0;i<content->nEntries;i++)
        {
            if (strcmp(fileInArchiveName, content->entries[i].fileName) == 0)
            {
                if (archiveEntry)
                    *archiveEntry = &content->entries[i];
                return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************/
/*                           SplitFilename()                            */
/************************************************************************/

char* VSIArchiveFilesystemHandler::SplitFilename(const char *pszFilename,
                                                 CPLString &osFileInArchive,
                                                 int bCheckMainFileExists)
{
    int i = 0;

    if (strcmp(pszFilename, GetPrefix()) == 0)
        return NULL;

    /* Allow natural chaining of VSI drivers without requiring double slash */
    
    CPLString osDoubleVsi(GetPrefix());
    osDoubleVsi += "/vsi";
    
    if (strncmp(pszFilename, osDoubleVsi.c_str(), osDoubleVsi.size()) == 0)
        pszFilename += strlen(GetPrefix());
    else
        pszFilename += strlen(GetPrefix()) + 1;

    while(pszFilename[i])
    {
        std::vector<CPLString> oExtensions = GetExtensions();
        std::vector<CPLString>::const_iterator iter;
        int nToSkip = 0;

        for( iter = oExtensions.begin(); iter != oExtensions.end(); ++iter )
        {
            const CPLString& osExtension = *iter;
            if (EQUALN(pszFilename + i, osExtension.c_str(), strlen(osExtension.c_str())))
            {
                nToSkip = strlen(osExtension.c_str());
                break;
            }
        }

        if (nToSkip != 0)
        {
            VSIStatBufL statBuf;
            char* archiveFilename = CPLStrdup(pszFilename);
            int bArchiveFileExists = FALSE;

            if (archiveFilename[i + nToSkip] == '/' ||
                archiveFilename[i + nToSkip] == '\\')
            {
                archiveFilename[i + nToSkip] = 0;
            }

            if (!bCheckMainFileExists)
            {
                bArchiveFileExists = TRUE;
            }
            else
            {
                CPLMutexHolder oHolder( &hMutex );

                if (oFileList.find(archiveFilename) != oFileList.end() )
                {
                    bArchiveFileExists = TRUE;
                }
            }

            if (!bArchiveFileExists)
            {
                VSIFilesystemHandler *poFSHandler = 
                    VSIFileManager::GetHandler( archiveFilename );
                if (poFSHandler->Stat(archiveFilename, &statBuf,
                                      VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                    !VSI_ISDIR(statBuf.st_mode))
                {
                    bArchiveFileExists = TRUE;
                }
            }

            if (bArchiveFileExists)
            {
                if (pszFilename[i + nToSkip] == '/' ||
                    pszFilename[i + nToSkip] == '\\')
                {
                    char* pszArchiveInFileName = CPLStrdup(pszFilename + i + nToSkip + 1);

                    /* Replace a/../b by b and foo/a/../b by foo/b */
                    while(TRUE)
                    {
                        char* pszPrevDir = strstr(pszArchiveInFileName, "/../");
                        if (pszPrevDir == NULL || pszPrevDir == pszArchiveInFileName)
                            break;

                        char* pszPrevSlash = pszPrevDir - 1;
                        while(pszPrevSlash != pszArchiveInFileName &&
                                *pszPrevSlash != '/')
                            pszPrevSlash --;
                        if (pszPrevSlash == pszArchiveInFileName)
                            memmove(pszArchiveInFileName, pszPrevDir + nToSkip, strlen(pszPrevDir + nToSkip) + 1);
                        else
                            memmove(pszPrevSlash + 1, pszPrevDir + nToSkip, strlen(pszPrevDir + nToSkip) + 1);
                    }

                    osFileInArchive = pszArchiveInFileName;
                    CPLFree(pszArchiveInFileName);
                }
                else
                    osFileInArchive = "";

                /* Remove trailing slash */
                if (osFileInArchive.size())
                {
                    char lastC = osFileInArchive[strlen(osFileInArchive) - 1];
                    if (lastC == '\\' || lastC == '/')
                        osFileInArchive.resize(strlen(osFileInArchive) - 1);
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

VSIArchiveReader* VSIArchiveFilesystemHandler::OpenArchiveFile(const char* archiveFilename, 
                                                               const char* fileInArchiveName)
{
    VSIArchiveReader* poReader = CreateReader(archiveFilename);

    if (poReader == NULL)
    {
        return NULL;
    }

    if (fileInArchiveName == NULL || strlen(fileInArchiveName) == 0)
    {
        if (poReader->GotoFirstFile() == FALSE)
        {
            delete(poReader);
            return NULL;
        }

        /* Skip optionnal leading subdir */
        CPLString osFileName = poReader->GetFileName();
        const char* fileName = osFileName.c_str();
        if (fileName[strlen(fileName)-1] == '/' || fileName[strlen(fileName)-1] == '\\')
        {
            if (poReader->GotoNextFile() == FALSE)
            {
                delete(poReader);
                return NULL;
            }
        }

        if (poReader->GotoNextFile())
        {
            CPLString msg;
            msg.Printf("Support only 1 file in archive file %s when no explicit in-archive filename is specified",
                       archiveFilename);
            const VSIArchiveContent* content = GetContentOfArchive(archiveFilename, poReader);
            if (content)
            {
                int i;
                msg += "\nYou could try one of the following :\n";
                for(i=0;i<content->nEntries;i++)
                {
                    msg += CPLString().Printf("  %s/%s/%s\n", GetPrefix(), archiveFilename, content->entries[i].fileName);
                }
            }

            CPLError(CE_Failure, CPLE_NotSupported, "%s", msg.c_str());

            delete(poReader);
            return NULL;
        }
    }
    else
    {
        const VSIArchiveEntry* archiveEntry = NULL;
        if (FindFileInArchive(archiveFilename, fileInArchiveName, &archiveEntry) == FALSE ||
            archiveEntry->bIsDir)
        {
            delete(poReader);
            return NULL;
        }
        if (!poReader->GotoFileOffset(archiveEntry->file_pos))
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

int VSIArchiveFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf, CPL_UNUSED int nFlags )
{
    int ret = -1;
    CPLString osFileInArchive;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    char* archiveFilename = SplitFilename(pszFilename, osFileInArchive, TRUE);
    if (archiveFilename == NULL)
        return -1;

    if (strlen(osFileInArchive) != 0)
    {
        if (ENABLE_DEBUG) CPLDebug("VSIArchive", "Looking for %s %s\n",
                                    archiveFilename, osFileInArchive.c_str());

        const VSIArchiveEntry* archiveEntry = NULL;
        if (FindFileInArchive(archiveFilename, osFileInArchive, &archiveEntry))
        {
            /* Patching st_size with uncompressed file size */
            pStatBuf->st_size = archiveEntry->uncompressed_size;
            pStatBuf->st_mtime = (time_t)archiveEntry->nModifiedTime;
            if (archiveEntry->bIsDir)
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

        if (poReader != NULL && poReader->GotoFirstFile())
        {
            /* Skip optionnal leading subdir */
            CPLString osFileName = poReader->GetFileName();
            const char* fileName = osFileName.c_str();
            if (fileName[strlen(fileName)-1] == '/' || fileName[strlen(fileName)-1] == '\\')
            {
                if (poReader->GotoNextFile() == FALSE)
                {
                    delete(poReader);
                    return -1;
                }
            }

            if (poReader->GotoNextFile())
            {
                /* Several files in archive --> treat as dir */
                pStatBuf->st_size = 0;
                pStatBuf->st_mode = S_IFDIR;
            }
            else
            {
                /* Patching st_size with uncompressed file size */
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

int VSIArchiveFilesystemHandler::Unlink( CPL_UNUSED const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                             Rename()                                 */
/************************************************************************/

int VSIArchiveFilesystemHandler::Rename( CPL_UNUSED const char *oldpath, CPL_UNUSED const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                             Mkdir()                                  */
/************************************************************************/

int VSIArchiveFilesystemHandler::Mkdir( CPL_UNUSED const char *pszDirname, CPL_UNUSED long nMode )
{
    return -1;
}

/************************************************************************/
/*                             Rmdir()                                  */
/************************************************************************/

int VSIArchiveFilesystemHandler::Rmdir( CPL_UNUSED const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSIArchiveFilesystemHandler::ReadDir( const char *pszDirname )
{
    CPLString osInArchiveSubDir;
    char* archiveFilename = SplitFilename(pszDirname, osInArchiveSubDir, TRUE);
    if (archiveFilename == NULL)
        return NULL;
    int lenInArchiveSubDir = strlen(osInArchiveSubDir);

    char **papszDir = NULL;
    
    const VSIArchiveContent* content = GetContentOfArchive(archiveFilename);
    if (!content)
    {
        CPLFree(archiveFilename);
        return NULL;
    }

    if (ENABLE_DEBUG) CPLDebug("VSIArchive", "Read dir %s", pszDirname);
    int i;
    for(i=0;i<content->nEntries;i++)
    {
        const char* fileName = content->entries[i].fileName;
        /* Only list entries at the same level of inArchiveSubDir */
        if (lenInArchiveSubDir != 0 &&
            strncmp(fileName, osInArchiveSubDir, lenInArchiveSubDir) == 0 &&
            (fileName[lenInArchiveSubDir] == '/' || fileName[lenInArchiveSubDir] == '\\') &&
            fileName[lenInArchiveSubDir + 1] != 0)
        {
            const char* slash = strchr(fileName + lenInArchiveSubDir + 1, '/');
            if (slash == NULL)
                slash = strchr(fileName + lenInArchiveSubDir + 1, '\\');
            if (slash == NULL || slash[1] == 0)
            {
                char* tmpFileName = CPLStrdup(fileName);
                if (slash != NULL)
                {
                    tmpFileName[strlen(tmpFileName)-1] = 0;
                }
                if (ENABLE_DEBUG)
                    CPLDebug("VSIArchive", "Add %s as in directory %s\n",
                            tmpFileName + lenInArchiveSubDir + 1, pszDirname);
                papszDir = CSLAddString(papszDir, tmpFileName + lenInArchiveSubDir + 1);
                CPLFree(tmpFileName);
            }
        }
        else if (lenInArchiveSubDir == 0 &&
                 strchr(fileName, '/') == NULL && strchr(fileName, '\\') == NULL)
        {
            /* Only list toplevel files and directories */
            if (ENABLE_DEBUG) CPLDebug("VSIArchive", "Add %s as in directory %s\n", fileName, pszDirname);
            papszDir = CSLAddString(papszDir, fileName);
        }
    }

    CPLFree(archiveFilename);
    return papszDir;
}
