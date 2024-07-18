/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for /vsi7z/ and /vsirar/
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef HAVE_LIBARCHIVE

/************************************************************************/
/*                    VSIInstall7zFileHandler()                         */
/************************************************************************/

/*!
 \brief Install /vsi7z/ 7zip file system handler (requires libarchive)

 \verbatim embed:rst
 See :ref:`/vsi7z/ documentation <vsi7z>`
 \endverbatim

 @since GDAL 3.7
 */
void VSIInstall7zFileHandler(void)
{
    // dummy
}

/************************************************************************/
/*                    VSIInstallRarFileHandler()                         */
/************************************************************************/

/*!
 \brief Install /vsirar/ RAR file system handler (requires libarchive)

 \verbatim embed:rst
 See :ref:`/vsirar/ documentation <vsirar>`
 \endverbatim

 @since GDAL 3.7
 */
void VSIInstallRarFileHandler(void)
{
    // dummy
}

#else

//! @cond Doxygen_Suppress

#include <algorithm>
#include <limits>
#include <memory>

// libarchive
#ifdef USE_INTERNAL_LIBARCHIVE
#include "archive_gdal_config.h"
#endif

#include "archive.h"
#include "archive_entry.h"

/************************************************************************/
/* ==================================================================== */
/*                      VSILibArchiveClientData                         */
/* ==================================================================== */
/************************************************************************/

struct VSILibArchiveClientData
{
    CPL_DISALLOW_COPY_ASSIGN(VSILibArchiveClientData)

    const std::string m_osFilename;
    VSIVirtualHandle *m_poBaseHandle = nullptr;
    std::vector<GByte> m_abyBuffer{};

    VSILibArchiveClientData(const char *pszFilename) : m_osFilename(pszFilename)
    {
        m_abyBuffer.resize(4096);
    }

    static int openCbk(struct archive *pArchive, void *pClientData)
    {
        auto poClientData = static_cast<VSILibArchiveClientData *>(pClientData);
        CPLDebug("VSIARCH", "Opening %s", poClientData->m_osFilename.c_str());
        poClientData->m_poBaseHandle = reinterpret_cast<VSIVirtualHandle *>(
            VSIFOpenL(poClientData->m_osFilename.c_str(), "rb"));
        if (poClientData->m_poBaseHandle == nullptr)
        {
            archive_set_error(pArchive, -1, "Cannot open file");
            return ARCHIVE_FATAL;
        }
        return ARCHIVE_OK;
    }

    static int closeCbk(struct archive *pArchive, void *pClientData)
    {
        auto poClientData = static_cast<VSILibArchiveClientData *>(pClientData);
        int ret = 0;
        if (poClientData->m_poBaseHandle)
        {
            ret = poClientData->m_poBaseHandle->Close();
            delete poClientData->m_poBaseHandle;
        }
        delete poClientData;
        if (ret == 0)
            return ARCHIVE_OK;
        archive_set_error(pArchive, -1, "Cannot close file");
        return ARCHIVE_FATAL;
    }

    static la_ssize_t readCbk(struct archive *, void *pClientData,
                              const void **ppBuffer)
    {
        auto poClientData = static_cast<VSILibArchiveClientData *>(pClientData);
        *ppBuffer = poClientData->m_abyBuffer.data();
        return static_cast<la_ssize_t>(poClientData->m_poBaseHandle->Read(
            poClientData->m_abyBuffer.data(), 1,
            poClientData->m_abyBuffer.size()));
    }

    static la_int64_t seekCkb(struct archive *, void *pClientData,
                              la_int64_t offset, int whence)
    {
        auto poClientData = static_cast<VSILibArchiveClientData *>(pClientData);
        if (whence == SEEK_CUR && offset < 0)
        {
            whence = SEEK_SET;
            offset = poClientData->m_poBaseHandle->Tell() + offset;
        }
        if (poClientData->m_poBaseHandle->Seek(
                static_cast<vsi_l_offset>(offset), whence) != 0)
            return ARCHIVE_FATAL;
        return static_cast<la_int64_t>(poClientData->m_poBaseHandle->Tell());
    }
};

/************************************************************************/
/*                    VSILibArchiveReadOpen()                           */
/************************************************************************/

/**Open an archive, with the base handle being a VSIVirtualHandle* */
static int VSILibArchiveReadOpen(struct archive *pArchive,
                                 const char *pszFilename)
{
    archive_read_set_seek_callback(pArchive, VSILibArchiveClientData::seekCkb);
    return archive_read_open(pArchive, new VSILibArchiveClientData(pszFilename),
                             VSILibArchiveClientData::openCbk,
                             VSILibArchiveClientData::readCbk,
                             VSILibArchiveClientData::closeCbk);
}

/************************************************************************/
/*                    VSICreateArchiveHandle()                          */
/************************************************************************/

static struct archive *VSICreateArchiveHandle(const std::string &osFSPrefix)
{
    auto pArchive = archive_read_new();

    if (osFSPrefix == "/vsi7z")
    {
        archive_read_support_format_7zip(pArchive);
    }
    else
    {
        archive_read_support_format_rar(pArchive);
#ifdef ARCHIVE_FORMAT_RAR_V5
        archive_read_support_format_rar5(pArchive);
#endif
    }

    return pArchive;
}

/************************************************************************/
/* ==================================================================== */
/*                      VSILibArchiveReader                             */
/* ==================================================================== */
/************************************************************************/

class VSILibArchiveReader final : public VSIArchiveReader
{
    CPL_DISALLOW_COPY_ASSIGN(VSILibArchiveReader)

    std::string m_osArchiveFileName;
    struct archive *m_pArchive;
    std::string m_osPrefix;
    bool m_bFirst = true;
    std::string m_osFilename{};
    GUIntBig m_nFilesize = 0;
    GIntBig m_nMTime = 0;

  public:
    VSILibArchiveReader(const char *pszArchiveFileName,
                        struct archive *pArchive, const std::string &osPrefix)
        : m_osArchiveFileName(pszArchiveFileName), m_pArchive(pArchive),
          m_osPrefix(osPrefix)
    {
    }

    ~VSILibArchiveReader() override;

    struct archive *GetArchiveHandler()
    {
        return m_pArchive;
    }

    int GotoFirstFileForced();

    virtual int GotoFirstFile() override;
    virtual int GotoNextFile() override;
    virtual VSIArchiveEntryFileOffset *GetFileOffset() override;

    virtual GUIntBig GetFileSize() override
    {
        return m_nFilesize;
    }

    virtual CPLString GetFileName() override
    {
        return m_osFilename;
    }

    virtual GIntBig GetModifiedTime() override
    {
        return m_nMTime;
    }

    virtual int GotoFileOffset(VSIArchiveEntryFileOffset *pOffset) override;

    int GotoFileOffsetForced(VSIArchiveEntryFileOffset *pOffset);
};

/************************************************************************/
/*                       ~VSILibArchiveReader()                         */
/************************************************************************/

VSILibArchiveReader::~VSILibArchiveReader()
{
    archive_free(m_pArchive);
}

/************************************************************************/
/*                           GotoFirstFile()                            */
/************************************************************************/

int VSILibArchiveReader::GotoFirstFile()
{
    if (!m_bFirst)
    {
        archive_free(m_pArchive);

        m_pArchive = VSICreateArchiveHandle(m_osPrefix);

        if (VSILibArchiveReadOpen(m_pArchive, m_osArchiveFileName.c_str()))
        {
            CPLDebug("VSIARCH", "%s: %s", m_osArchiveFileName.c_str(),
                     archive_error_string(m_pArchive));
            return false;
        }
        m_bFirst = true;
    }
    return GotoNextFile();
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSILibArchiveReader::GotoNextFile()
{
    struct archive_entry *entry;
    int r = archive_read_next_header(m_pArchive, &entry);
    if (r == ARCHIVE_EOF)
        return FALSE;
    if (r != ARCHIVE_OK)
    {
        CPLDebug("VSIARCH", "%s", archive_error_string(m_pArchive));
        return FALSE;
    }
    m_osFilename = archive_entry_pathname_utf8(entry);
    m_nFilesize = archive_entry_size(entry);
    m_nMTime = archive_entry_mtime(entry);
    return TRUE;
}

/************************************************************************/
/*                      VSILibArchiveEntryFileOffset                    */
/************************************************************************/

struct VSILibArchiveEntryFileOffset : public VSIArchiveEntryFileOffset
{
    const std::string m_osFilename;

    VSILibArchiveEntryFileOffset(const std::string &osFilename)
        : m_osFilename(osFilename)
    {
    }
};

/************************************************************************/
/*                          GetFileOffset()                             */
/************************************************************************/

VSIArchiveEntryFileOffset *VSILibArchiveReader::GetFileOffset()
{
    return new VSILibArchiveEntryFileOffset(m_osFilename);
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSILibArchiveReader::GotoFileOffset(VSIArchiveEntryFileOffset *pOffset)
{
    VSILibArchiveEntryFileOffset *pMyOffset =
        static_cast<VSILibArchiveEntryFileOffset *>(pOffset);
    if (!GotoFirstFile())
        return false;
    while (m_osFilename != pMyOffset->m_osFilename)
    {
        if (!GotoNextFile())
            return false;
    }
    return true;
}

/************************************************************************/
/*                       GotoFileOffsetForced()                         */
/************************************************************************/

int VSILibArchiveReader::GotoFileOffsetForced(
    VSIArchiveEntryFileOffset *pOffset)
{
    m_bFirst = false;
    return GotoFileOffset(pOffset);
}

/************************************************************************/
/* ==================================================================== */
/*                      VSILibArchiveHandler                            */
/* ==================================================================== */
/************************************************************************/

class VSILibArchiveHandler final : public VSIVirtualHandle
{
    const std::string m_osFilename;
    std::unique_ptr<VSILibArchiveReader> m_poReader;
    std::unique_ptr<VSIArchiveEntryFileOffset> m_pOffset;
    vsi_l_offset m_nOffset = 0;
    bool m_bEOF = false;
    bool m_bError = false;

  public:
    VSILibArchiveHandler(const std::string &osFilename,
                         VSILibArchiveReader *poReader)
        : m_osFilename(osFilename), m_poReader(poReader),
          m_pOffset(poReader->GetFileOffset())
    {
    }

    virtual size_t Read(void *pBuffer, size_t nSize, size_t nCount) override;
    virtual int Seek(vsi_l_offset nOffset, int nWhence) override;

    virtual vsi_l_offset Tell() override
    {
        return m_nOffset;
    }

    virtual size_t Write(const void *, size_t, size_t) override
    {
        return 0;
    }

    virtual int Eof() override
    {
        return m_bEOF ? 1 : 0;
    }

    virtual int Close() override
    {
        return 0;
    }
};

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSILibArchiveHandler::Read(void *pBuffer, size_t nSize, size_t nCount)
{
    if (m_bError || nSize == 0 || nCount == 0)
        return 0;
    if (m_nOffset == m_poReader->GetFileSize())
    {
        m_bEOF = true;
        return 0;
    }
    size_t nToRead = nSize * nCount;
    auto nRead = static_cast<size_t>(
        archive_read_data(m_poReader->GetArchiveHandler(), pBuffer, nToRead));
    if (nRead < nToRead)
        m_bEOF = true;
    m_nOffset += nRead;
    return nRead / nSize;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSILibArchiveHandler::Seek(vsi_l_offset nOffset, int nWhence)
{
    if (m_bError)
        return -1;
    m_bEOF = false;
    if (nWhence == SEEK_END && nOffset == 0)
    {
        m_nOffset = m_poReader->GetFileSize();
        return 0;
    }
    auto nNewOffset = m_nOffset;
    if (nWhence == SEEK_CUR)
        nNewOffset += nOffset;
    else
        nNewOffset = nOffset;
    if (nNewOffset == m_nOffset)
        return 0;

    if (nNewOffset < m_nOffset)
    {
        CPLDebug("VSIARCH", "Seeking backwards in %s", m_osFilename.c_str());
        // If we need to go backwards, we must completely reset the
        // reader!
        if (!m_poReader->GotoFileOffsetForced(m_pOffset.get()))
        {
            m_bError = true;
            return -1;
        }
        m_nOffset = 0;
    }

    std::vector<GByte> abyBuffer(4096);
    while (m_nOffset < nNewOffset)
    {
        size_t nToRead = static_cast<size_t>(
            std::min<vsi_l_offset>(abyBuffer.size(), nNewOffset - m_nOffset));
        if (Read(abyBuffer.data(), 1, nToRead) != nToRead)
            break;
    }

    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*                      VSILibArchiveFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

class VSILibArchiveFilesystemHandler final : public VSIArchiveFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSILibArchiveFilesystemHandler)

    const std::string m_osPrefix;

    virtual const char *GetPrefix() override
    {
        return m_osPrefix.c_str();
    }

    virtual std::vector<CPLString> GetExtensions() override
    {
        if (m_osPrefix == "/vsi7z")
        {
            return {".7z", ".lpk", ".lpkx", ".mpk", ".mpkx", ".ppkx"};
        }
        else
        {
            return {".rar"};
        }
    }

    virtual VSIArchiveReader *
    CreateReader(const char *pszArchiveFileName) override;

  public:
    VSILibArchiveFilesystemHandler(const std::string &osPrefix)
        : m_osPrefix(osPrefix)
    {
    }

    virtual VSIVirtualHandle *Open(const char *pszFilename,
                                   const char *pszAccess, bool bSetError,
                                   CSLConstList papszOptions) override;
};

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle *VSILibArchiveFilesystemHandler::Open(const char *pszFilename,
                                                       const char *pszAccess,
                                                       bool, CSLConstList)
{
    if (strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, '+') != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for %s", m_osPrefix.c_str());
        return nullptr;
    }

    CPLString osFileInArchive;
    char *pszArchiveFileName =
        SplitFilename(pszFilename, osFileInArchive, TRUE);
    if (pszArchiveFileName == nullptr)
        return nullptr;

    VSILibArchiveReader *poReader = cpl::down_cast<VSILibArchiveReader *>(
        OpenArchiveFile(pszArchiveFileName, osFileInArchive));
    CPLFree(pszArchiveFileName);
    if (poReader == nullptr)
    {
        return nullptr;
    }

    return new VSILibArchiveHandler(pszFilename, poReader);
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

VSIArchiveReader *
VSILibArchiveFilesystemHandler::CreateReader(const char *pszArchiveFileName)
{
    auto pArchive = VSICreateArchiveHandle(m_osPrefix);

    if (VSILibArchiveReadOpen(pArchive, pszArchiveFileName))
    {
        CPLDebug("VSIARCH", "%s: %s", pszArchiveFileName,
                 archive_error_string(pArchive));
        archive_read_free(pArchive);
        return nullptr;
    }
    return new VSILibArchiveReader(pszArchiveFileName, pArchive, m_osPrefix);
}

//! @endcond

/************************************************************************/
/*                    VSIInstall7zFileHandler()                         */
/************************************************************************/

/*!
 \brief Install /vsi7z/ 7zip file system handler (requires libarchive)

 \verbatim embed:rst
 See :ref:`/vsi7z/ documentation <vsi7z>`
 \endverbatim

 @since GDAL 3.7
 */
void VSIInstall7zFileHandler(void)
{
    VSIFileManager::InstallHandler(
        "/vsi7z/", new VSILibArchiveFilesystemHandler("/vsi7z"));
}

/************************************************************************/
/*                    VSIInstallRarFileHandler()                         */
/************************************************************************/

/*!
 \brief Install /vsirar/ rar file system handler (requires libarchive)

 \verbatim embed:rst
 See :ref:`/vsirar/ documentation <vsirar>`
 \endverbatim

 @since GDAL 3.7
 */
void VSIInstallRarFileHandler(void)
{
    VSIFileManager::InstallHandler(
        "/vsirar/", new VSILibArchiveFilesystemHandler("/vsirar"));
}

#endif
