/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for tar files (.tar).
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

//! @cond Doxygen_Suppress

#include "cpl_port.h"
#include "cpl_vsi.h"

#include <cstring>

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$");

#if (defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)) && !defined(HAVE_FUZZER_FRIENDLY_ARCHIVE)
/* This is a completely custom archive format that is rather inefficient */
/* but supports random insertions or deletions, since it doesn't record */
/* explicit file size or rely on files starting on a particular boundary */
#define HAVE_FUZZER_FRIENDLY_ARCHIVE 1
#endif

/************************************************************************/
/* ==================================================================== */
/*                       VSITarEntryFileOffset                          */
/* ==================================================================== */
/************************************************************************/

class VSITarEntryFileOffset CPL_FINAL : public VSIArchiveEntryFileOffset
{
public:
        GUIntBig m_nOffset;
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
        GUIntBig m_nFileSize;
        CPLString m_osFileName;
#endif

        explicit VSITarEntryFileOffset(GUIntBig nOffset)
        {
            m_nOffset = nOffset;
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
            m_nFileSize = 0;
#endif
        }

#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
        VSITarEntryFileOffset(GUIntBig nOffset, GUIntBig nFileSize, const CPLString& osFileName) :
            m_nOffset(nOffset),
            m_nFileSize(nFileSize),
            m_osFileName(osFileName)
        {
        }
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                             VSITarReader                             */
/* ==================================================================== */
/************************************************************************/

class VSITarReader CPL_FINAL : public VSIArchiveReader
{
    private:
        VSILFILE* fp;
        GUIntBig nCurOffset;
        GUIntBig nNextFileSize;
        CPLString osNextFileName;
        GIntBig nModifiedTime;
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
        bool m_bIsFuzzerFriendly;
        GByte m_abyBuffer[2048];
        int m_abyBufferIdx;
        int m_abyBufferSize;
        GUIntBig m_nCurOffsetOld;
#endif

    public:
        explicit VSITarReader(const char* pszTarFileName);
        virtual ~VSITarReader();

        int IsValid() { return fp != NULL; }

        virtual int GotoFirstFile() override;
        virtual int GotoNextFile() override;
        virtual VSIArchiveEntryFileOffset* GetFileOffset() override;
        virtual GUIntBig GetFileSize() override { return nNextFileSize; }
        virtual CPLString GetFileName() override { return osNextFileName; }
        virtual GIntBig GetModifiedTime() override { return nModifiedTime; }
        virtual int GotoFileOffset(VSIArchiveEntryFileOffset* pOffset) override;
};

/************************************************************************/
/*                               VSIIsTGZ()                             */
/************************************************************************/

static bool VSIIsTGZ(const char* pszFilename)
{
    return (!STARTS_WITH_CI(pszFilename, "/vsigzip/") &&
            ((strlen(pszFilename) > 4 &&
            STARTS_WITH_CI(pszFilename + strlen(pszFilename) - 4, ".tgz")) ||
            (strlen(pszFilename) > 7 &&
            STARTS_WITH_CI(pszFilename + strlen(pszFilename) - 7, ".tar.gz"))));
}

/************************************************************************/
/*                           VSITarReader()                             */
/************************************************************************/

// TODO(schwehr): What is this ***NEWFILE*** thing?
// And make it a symbolic constant.

VSITarReader::VSITarReader(const char* pszTarFileName) :
    nCurOffset(0),
    nNextFileSize(0),
    nModifiedTime(0)
{
    fp = VSIFOpenL(pszTarFileName, "rb");
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
    m_bIsFuzzerFriendly = false;
    m_abyBuffer[0] = '\0';
    m_abyBufferIdx = 0;
    m_abyBufferSize = 0;
    m_nCurOffsetOld = 0;
    if( fp != NULL )
    {
        GByte abySignature[24] = {};
        m_bIsFuzzerFriendly =
            (VSIFReadL(abySignature, 1, 24, fp) == 24) &&
            (memcmp(abySignature, "FUZZER_FRIENDLY_ARCHIVE\n", 24) == 0 ||
             memcmp(abySignature, "***NEWFILE***:",
                    strlen("***NEWFILE***:")) == 0);
        CPL_IGNORE_RET_VAL(VSIFSeekL(fp, 0, SEEK_SET));
    }
#endif
}

/************************************************************************/
/*                          ~VSITarReader()                             */
/************************************************************************/

VSITarReader::~VSITarReader()
{
    if (fp)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
}

/************************************************************************/
/*                          GetFileOffset()                             */
/************************************************************************/

VSIArchiveEntryFileOffset* VSITarReader::GetFileOffset()
{
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
    if( m_bIsFuzzerFriendly )
    {
        return new VSITarEntryFileOffset(nCurOffset, nNextFileSize,
                                         osNextFileName);
    }
#endif
    return new VSITarEntryFileOffset(nCurOffset);
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSITarReader::GotoNextFile()
{
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
    if( m_bIsFuzzerFriendly )
    {
        while( true )
        {
            if( m_abyBufferIdx >= m_abyBufferSize )
            {
                if( m_abyBufferSize == 0 )
                {
                    m_abyBufferSize = static_cast<int>(
                        VSIFReadL(m_abyBuffer, 1, 2048, fp));
                    if( m_abyBufferSize == 0 )
                        return FALSE;
                }
                else
                {
                    if( m_abyBufferSize < 2048 )
                    {
                        if( nCurOffset > 0 && nCurOffset != m_nCurOffsetOld )
                        {
                            nNextFileSize = VSIFTellL(fp);
                            nNextFileSize -= m_abyBufferSize;
                            nNextFileSize += m_abyBufferIdx;
                            if( nNextFileSize >= nCurOffset )
                            {
                                nNextFileSize -= nCurOffset;
                                m_nCurOffsetOld = nCurOffset;
                                return TRUE;
                            }
                        }
                        return FALSE;
                    }
                    memcpy(m_abyBuffer, m_abyBuffer + 1024, 1024);
                    m_abyBufferSize = static_cast<int>(
                         VSIFReadL(m_abyBuffer + 1024, 1, 1024, fp));
                    if( m_abyBufferSize == 0 )
                        return FALSE;
                    m_abyBufferIdx = 0;
                    m_abyBufferSize += 1024;
                }
            }
            if( ((m_abyBufferSize == 2048 &&
                  m_abyBufferIdx <
                  m_abyBufferSize -
                  (static_cast<int>(strlen("***NEWFILE***:")) + 64)) ||
                 (m_abyBufferSize < 2048 &&
                  m_abyBufferIdx < m_abyBufferSize -
                  (static_cast<int>(strlen("***NEWFILE***:"))+2))) &&
                m_abyBufferIdx >= 0 &&  // Make CSA happy, but useless.
                memcmp(m_abyBuffer + m_abyBufferIdx,
                       "***NEWFILE***:",
                       strlen("***NEWFILE***:")) == 0 )
            {
                if( nCurOffset > 0 && nCurOffset != m_nCurOffsetOld )
                {
                    nNextFileSize = VSIFTellL(fp);
                    nNextFileSize -= m_abyBufferSize;
                    nNextFileSize += m_abyBufferIdx;
                    if( nNextFileSize >= nCurOffset )
                    {
                        nNextFileSize -= nCurOffset;
                        m_nCurOffsetOld = nCurOffset;
                        return TRUE;
                    }
                }
                m_abyBufferIdx += static_cast<int>(strlen("***NEWFILE***:"));
                const int nFilenameStartIdx = m_abyBufferIdx;
                for( ; m_abyBufferIdx < m_abyBufferSize &&
                       m_abyBuffer[m_abyBufferIdx] != '\n';
                     ++m_abyBufferIdx)
                {
                    // Do nothing.
                }
                if( m_abyBufferIdx < m_abyBufferSize )
                {
                    osNextFileName.assign(
                        (const char*)(m_abyBuffer + nFilenameStartIdx),
                        m_abyBufferIdx - nFilenameStartIdx);
                    nCurOffset = VSIFTellL(fp);
                    nCurOffset -= m_abyBufferSize;
                    nCurOffset += m_abyBufferIdx + 1;
                }
            }
            else
            {
                m_abyBufferIdx++;
            }
        }
    }
#endif
    char abyHeader[512] = {};
    if (VSIFReadL(abyHeader, 512, 1, fp) != 1)
        return FALSE;

    if (abyHeader[99] != '\0' ||
        abyHeader[107] != '\0' ||
        abyHeader[115] != '\0' ||
        abyHeader[123] != '\0' ||
        (abyHeader[135] != '\0' && abyHeader[135] != ' ') ||
        (abyHeader[147] != '\0' && abyHeader[147] != ' '))
    {
        return FALSE;
    }
    if( abyHeader[124] < '0' || abyHeader[124] > '7' )
        return FALSE;

    osNextFileName = abyHeader;
    nNextFileSize = 0;
    for(int i=0;i<11;i++)
        nNextFileSize = nNextFileSize * 8 + (abyHeader[124+i] - '0');

    nModifiedTime = 0;
    for(int i=0;i<11;i++)
        nModifiedTime = nModifiedTime * 8 + (abyHeader[136+i] - '0');

    nCurOffset = VSIFTellL(fp);

    const GUIntBig nBytesToSkip = ((nNextFileSize + 511) / 512) * 512;
    if( nBytesToSkip > (~(static_cast<GUIntBig>(0))) - nCurOffset )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Bad .tar structure");
        return FALSE;
    }

    if( VSIFSeekL(fp, nBytesToSkip, SEEK_CUR) < 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSITarReader::GotoFirstFile()
{
    if( VSIFSeekL(fp, 0, SEEK_SET) < 0 )
        return FALSE;
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
    m_abyBufferIdx = 0;
    m_abyBufferSize = 0;
    nCurOffset = 0;
    m_nCurOffsetOld = 0;
    osNextFileName = "";
    nNextFileSize = 0;
#endif
    return GotoNextFile();
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSITarReader::GotoFileOffset( VSIArchiveEntryFileOffset* pOffset )
{
    VSITarEntryFileOffset* pTarEntryOffset =
        static_cast<VSITarEntryFileOffset*>(pOffset);
#ifdef HAVE_FUZZER_FRIENDLY_ARCHIVE
    if( m_bIsFuzzerFriendly )
    {
        if( VSIFSeekL(fp, pTarEntryOffset->m_nOffset + pTarEntryOffset->m_nFileSize, SEEK_SET) < 0 )
            return FALSE;
        m_abyBufferIdx = 0;
        m_abyBufferSize = 0;
        nCurOffset = pTarEntryOffset->m_nOffset;
        m_nCurOffsetOld = pTarEntryOffset->m_nOffset;
        osNextFileName = pTarEntryOffset->m_osFileName;
        nNextFileSize = pTarEntryOffset->m_nFileSize;
        return TRUE;
    }
#endif
    if( VSIFSeekL(fp, pTarEntryOffset->m_nOffset - 512, SEEK_SET) < 0 )
        return FALSE;
    return GotoNextFile();
}

/************************************************************************/
/* ==================================================================== */
/*                        VSITarFilesystemHandler                      */
/* ==================================================================== */
/************************************************************************/

class VSITarFilesystemHandler CPL_FINAL : public VSIArchiveFilesystemHandler
{
public:
    virtual const char* GetPrefix() override { return "/vsitar"; }
    virtual std::vector<CPLString> GetExtensions() override;
    virtual VSIArchiveReader* CreateReader(const char* pszTarFileName) override;

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;
};

/************************************************************************/
/*                          GetExtensions()                             */
/************************************************************************/

std::vector<CPLString> VSITarFilesystemHandler::GetExtensions()
{
    std::vector<CPLString> oList;
    oList.push_back(".tar.gz");
    oList.push_back(".tar");
    oList.push_back(".tgz");
    return oList;
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

VSIArchiveReader* VSITarFilesystemHandler::CreateReader(const char* pszTarFileName)
{
    CPLString osTarInFileName;

    if (VSIIsTGZ(pszTarFileName))
    {
        osTarInFileName = "/vsigzip/";
        osTarInFileName += pszTarFileName;
    }
    else
        osTarInFileName = pszTarFileName;

    VSITarReader* poReader = new VSITarReader(osTarInFileName);

    if (!poReader->IsValid())
    {
        delete poReader;
        return NULL;
    }

    if (!poReader->GotoFirstFile())
    {
        delete poReader;
        return NULL;
    }

    return poReader;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle* VSITarFilesystemHandler::Open( const char *pszFilename,
                                                 const char *pszAccess,
                                                 bool /* bSetError */ )
{

    if (strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsitar");
        return NULL;
    }

    CPLString osTarInFileName;
    char* tarFilename
        = SplitFilename(pszFilename, osTarInFileName, TRUE);
    if (tarFilename == NULL)
        return NULL;

    VSIArchiveReader* poReader = OpenArchiveFile(tarFilename, osTarInFileName);
    if (poReader == NULL)
    {
        CPLFree(tarFilename);
        return NULL;
    }

    CPLString osSubFileName("/vsisubfile/");
    VSITarEntryFileOffset* pOffset = reinterpret_cast<VSITarEntryFileOffset*>(
                                                    poReader->GetFileOffset());
    osSubFileName += CPLString().Printf(CPL_FRMT_GUIB, pOffset->m_nOffset);
    osSubFileName += "_";
    osSubFileName += CPLString().Printf(CPL_FRMT_GUIB, poReader->GetFileSize());
    osSubFileName += ",";
    delete pOffset;

    if (VSIIsTGZ(tarFilename))
    {
        osSubFileName += "/vsigzip/";
        osSubFileName += tarFilename;
    }
    else
        osSubFileName += tarFilename;

    delete(poReader);

    CPLFree(tarFilename);
    tarFilename = NULL;

    return reinterpret_cast<VSIVirtualHandle*>(VSIFOpenL(osSubFileName, "rb"));
}

//! @endcond

/************************************************************************/
/*                    VSIInstallTarFileHandler()                        */
/************************************************************************/

/**
 * \brief Install /vsitar/ file system handler.
 *
 * A special file handler is installed that allows reading on-the-fly in TAR
 * (regular .tar, or compressed .tar.gz/.tgz) archives.
 *
 * All portions of the file system underneath the base path "/vsitar/" will be
 * handled by this driver.
 *
 * The syntax to open a file inside a tar file is /vsitar/path/to/the/file.tar/path/inside/the/tar/file
 * were path/to/the/file.tar is relative or absolute and path/inside/the/tar/file
 * is the relative path to the file inside the archive.
 *
 * Starting with GDAL 2.2, an alternate syntax is available so as to enable
 * chaining and not being dependent on .tar extension :
 * /vsitar/{/path/to/the/archive}/path/inside/the/tar/file. Note that /path/to/the/archive
 * may also itself this alternate syntax.
 *
 * If the path is absolute, it should begin with a / on a Unix-like OS (or C:\ on Windows),
 * so the line looks like /vsitar//home/gdal/...
 * For example gdalinfo /vsitar/myarchive.tar/subdir1/file1.tif
 *
 * Syntactic sugar : if the tar archive contains only one file located at its
 * root, just mentionning "/vsitar/path/to/the/file.tar" will work
 *
 * VSIStatL() will return the uncompressed size in st_size member and file
 * nature- file or directory - in st_mode member.
 *
 * Directory listing is available through VSIReadDir().
 *
 * @since GDAL 1.8.0
 */

void VSIInstallTarFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsitar/", new VSITarFilesystemHandler() );
}
