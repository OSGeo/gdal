/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for gz/zip files (.gz and .zip).
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

/* gzio.c -- IO on .gz files
  Copyright (C) 1995-2005 Jean-loup Gailly.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu

  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files http://www.ietf.org/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

/* This file contains a refactoring of gzio.c from zlib project.

   It replaces classical calls operating on FILE* by calls to the VSI large file
   API. It also adds the capability to seek at the end of the file, which is not
   implemented in original gzSeek. It also implements a concept of in-memory
   "snapshots", that are a way of improving efficiency while seeking GZip
   files. Snapshots are created regularly when decompressing the data a snapshot
   of the gzip state.  Later we can seek directly in the compressed data to the
   closest snapshot in order to reduce the amount of data to uncompress again.

   For .gz files, an effort is done to cache the size of the uncompressed data
   in a .gz.properties file, so that we don't need to seek at the end of the
   file each time a Stat() is done.

   For .zip and .gz, both reading and writing are supported, but just one mode
   at a time (read-only or write-only).
*/

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "cpl_zlib_header.h"  // to avoid warnings when including zlib.h

#ifdef HAVE_LIBDEFLATE
#include "libdeflate.h"
#endif

#include <algorithm>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cpl_error.h"
#include "cpl_minizip_ioapi.h"
#include "cpl_minizip_unzip.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"

constexpr int Z_BUFSIZE = 65536;           // Original size is 16384
constexpr int gz_magic[2] = {0x1f, 0x8b};  // gzip magic header

// gzip flag byte.
#define ASCII_FLAG 0x01   // bit 0 set: file probably ascii text
#define HEAD_CRC 0x02     // bit 1 set: header CRC present
#define EXTRA_FIELD 0x04  // bit 2 set: extra field present
#define ORIG_NAME 0x08    // bit 3 set: original file name present
#define COMMENT 0x10      // bit 4 set: file comment present
#define RESERVED 0xE0     // bits 5..7: reserved

#define ALLOC(size) malloc(size)
#define TRYFREE(p)                                                             \
    {                                                                          \
        if (p)                                                                 \
            free(p);                                                           \
    }

#define CPL_VSIL_GZ_RETURN(ret)                                                \
    CPLError(CE_Failure, CPLE_AppDefined, "In file %s, at line %d, return %d", \
             __FILE__, __LINE__, ret)

// #define ENABLE_DEBUG 1

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipHandle                                  */
/* ==================================================================== */
/************************************************************************/

typedef struct
{
    vsi_l_offset posInBaseHandle;
    z_stream stream;
    uLong crc;
    int transparent;
    vsi_l_offset in;
    vsi_l_offset out;
} GZipSnapshot;

class VSIGZipHandle final : public VSIVirtualHandle
{
    VSIVirtualHandle *m_poBaseHandle = nullptr;
#ifdef DEBUG
    vsi_l_offset m_offset = 0;
#endif
    vsi_l_offset m_compressed_size = 0;
    vsi_l_offset m_uncompressed_size = 0;
    vsi_l_offset offsetEndCompressedData = 0;
    uLong m_expected_crc = 0;
    char *m_pszBaseFileName = nullptr; /* optional */
    bool m_bWriteProperties = false;
    bool m_bCanSaveInfo = false;

    /* Fields from gz_stream structure */
    z_stream stream;
    int z_err = Z_OK; /* error code for last stream operation */
    int z_eof = 0;    /* set if end of input file (but not necessarily of the
                         uncompressed stream ! "in" must be null too ) */
    Byte *inbuf = nullptr;  /* input buffer */
    Byte *outbuf = nullptr; /* output buffer */
    uLong crc = 0;          /* crc32 of uncompressed data */
    int m_transparent = 0;  /* 1 if input file is not a .gz file */
    vsi_l_offset startOff =
        0; /* startOff of compressed data in file (header skipped) */
    vsi_l_offset in = 0;  /* bytes into deflate or inflate */
    vsi_l_offset out = 0; /* bytes out of deflate or inflate */
    vsi_l_offset m_nLastReadOffset = 0;

    GZipSnapshot *snapshots = nullptr;
    vsi_l_offset snapshot_byte_interval =
        0; /* number of compressed bytes at which we create a "snapshot" */

    void check_header();
    int get_byte();
    bool gzseek(vsi_l_offset nOffset, int nWhence);
    int gzrewind();
    uLong getLong();

    CPL_DISALLOW_COPY_ASSIGN(VSIGZipHandle)

  public:
    VSIGZipHandle(VSIVirtualHandle *poBaseHandle, const char *pszBaseFileName,
                  vsi_l_offset offset = 0, vsi_l_offset compressed_size = 0,
                  vsi_l_offset uncompressed_size = 0, uLong expected_crc = 0,
                  int transparent = 0);
    ~VSIGZipHandle() override;

    bool IsInitOK() const
    {
        return inbuf != nullptr;
    }

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    VSIGZipHandle *Duplicate();
    bool CloseBaseHandle();

    vsi_l_offset GetLastReadOffset()
    {
        return m_nLastReadOffset;
    }

    const char *GetBaseFileName()
    {
        return m_pszBaseFileName;
    }

    void SetUncompressedSize(vsi_l_offset nUncompressedSize)
    {
        m_uncompressed_size = nUncompressedSize;
    }

    vsi_l_offset GetUncompressedSize()
    {
        return m_uncompressed_size;
    }

    void SaveInfo_unlocked();

    void UnsetCanSaveInfo()
    {
        m_bCanSaveInfo = false;
    }
};

#ifdef ENABLE_DEFLATE64

/************************************************************************/
/* ==================================================================== */
/*                           VSIDeflate64Handle                         */
/* ==================================================================== */
/************************************************************************/

struct VSIDeflate64Snapshot
{
    vsi_l_offset posInBaseHandle = 0;
    z_stream stream{};
    uLong crc = 0;
    vsi_l_offset in = 0;
    vsi_l_offset out = 0;
    std::vector<GByte> extraOutput{};
    bool m_bStreamEndReached = false;
};

class VSIDeflate64Handle final : public VSIVirtualHandle
{
    VSIVirtualHandle *m_poBaseHandle = nullptr;
#ifdef DEBUG
    vsi_l_offset m_offset = 0;
#endif
    vsi_l_offset m_compressed_size = 0;
    vsi_l_offset m_uncompressed_size = 0;
    vsi_l_offset offsetEndCompressedData = 0;
    uLong m_expected_crc = 0;
    char *m_pszBaseFileName = nullptr; /* optional */

    /* Fields from gz_stream structure */
    z_stream stream;
    int z_err = Z_OK; /* error code for last stream operation */
    int z_eof = 0;    /* set if end of input file (but not necessarily of the
                         uncompressed stream ! "in" must be null too ) */
    Byte *inbuf = nullptr;  /* input buffer */
    Byte *outbuf = nullptr; /* output buffer */
    std::vector<GByte> extraOutput{};
    bool m_bStreamEndReached = false;
    uLong crc = 0; /* crc32 of uncompressed data */
    vsi_l_offset startOff =
        0; /* startOff of compressed data in file (header skipped) */
    vsi_l_offset in = 0;  /* bytes into deflate or inflate */
    vsi_l_offset out = 0; /* bytes out of deflate or inflate */

    std::vector<VSIDeflate64Snapshot> snapshots{};
    vsi_l_offset snapshot_byte_interval =
        0; /* number of compressed bytes at which we create a "snapshot" */

    bool gzseek(vsi_l_offset nOffset, int nWhence);
    int gzrewind();

    CPL_DISALLOW_COPY_ASSIGN(VSIDeflate64Handle)

  public:
    VSIDeflate64Handle(VSIVirtualHandle *poBaseHandle,
                       const char *pszBaseFileName, vsi_l_offset offset = 0,
                       vsi_l_offset compressed_size = 0,
                       vsi_l_offset uncompressed_size = 0,
                       uLong expected_crc = 0);
    ~VSIDeflate64Handle() override;

    bool IsInitOK() const
    {
        return inbuf != nullptr;
    }

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    VSIDeflate64Handle *Duplicate();
    bool CloseBaseHandle();

    const char *GetBaseFileName()
    {
        return m_pszBaseFileName;
    }

    void SetUncompressedSize(vsi_l_offset nUncompressedSize)
    {
        m_uncompressed_size = nUncompressedSize;
    }

    vsi_l_offset GetUncompressedSize()
    {
        return m_uncompressed_size;
    }
};
#endif

class VSIGZipFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGZipFilesystemHandler)

    CPLMutex *hMutex = nullptr;
    VSIGZipHandle *poHandleLastGZipFile = nullptr;
    bool m_bInSaveInfo = false;

  public:
    VSIGZipFilesystemHandler() = default;
    ~VSIGZipFilesystemHandler() override;

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;
    VSIGZipHandle *OpenGZipReadOnly(const char *pszFilename,
                                    const char *pszAccess);
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;
    int Unlink(const char *pszFilename) override;
    int Rename(const char *oldpath, const char *newpath) override;
    int Mkdir(const char *pszDirname, long nMode) override;
    int Rmdir(const char *pszDirname) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;

    const char *GetOptions() override;

    virtual bool SupportsSequentialWrite(const char *pszPath,
                                         bool bAllowLocalTempFile) override;

    virtual bool SupportsRandomWrite(const char * /* pszPath */,
                                     bool /* bAllowLocalTempFile */) override
    {
        return false;
    }

    void SaveInfo(VSIGZipHandle *poHandle);
    void SaveInfo_unlocked(VSIGZipHandle *poHandle);
};

/************************************************************************/
/*                            Duplicate()                               */
/************************************************************************/

VSIGZipHandle *VSIGZipHandle::Duplicate()
{
    CPLAssert(m_offset == 0);
    CPLAssert(m_compressed_size != 0);
    CPLAssert(m_pszBaseFileName != nullptr);

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(m_pszBaseFileName);

    VSIVirtualHandle *poNewBaseHandle =
        poFSHandler->Open(m_pszBaseFileName, "rb");

    if (poNewBaseHandle == nullptr)
        return nullptr;

    VSIGZipHandle *poHandle =
        new VSIGZipHandle(poNewBaseHandle, m_pszBaseFileName, 0,
                          m_compressed_size, m_uncompressed_size);
    if (!(poHandle->IsInitOK()))
    {
        delete poHandle;
        return nullptr;
    }

    poHandle->m_nLastReadOffset = m_nLastReadOffset;

    // Most important: duplicate the snapshots!

    for (unsigned int i = 0; i < m_compressed_size / snapshot_byte_interval + 1;
         i++)
    {
        if (snapshots[i].posInBaseHandle == 0)
            break;

        poHandle->snapshots[i].posInBaseHandle = snapshots[i].posInBaseHandle;
        inflateCopy(&poHandle->snapshots[i].stream, &snapshots[i].stream);
        poHandle->snapshots[i].crc = snapshots[i].crc;
        poHandle->snapshots[i].transparent = snapshots[i].transparent;
        poHandle->snapshots[i].in = snapshots[i].in;
        poHandle->snapshots[i].out = snapshots[i].out;
    }

    return poHandle;
}

/************************************************************************/
/*                     CloseBaseHandle()                                */
/************************************************************************/

bool VSIGZipHandle::CloseBaseHandle()
{
    bool bRet = true;
    if (m_poBaseHandle)
    {
        bRet = m_poBaseHandle->Close() == 0;
        delete m_poBaseHandle;
    }
    m_poBaseHandle = nullptr;
    return bRet;
}

/************************************************************************/
/*                       VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::VSIGZipHandle(VSIVirtualHandle *poBaseHandle,
                             const char *pszBaseFileName, vsi_l_offset offset,
                             vsi_l_offset compressed_size,
                             vsi_l_offset uncompressed_size, uLong expected_crc,
                             int transparent)
    : m_poBaseHandle(poBaseHandle),
#ifdef DEBUG
      m_offset(offset),
#endif
      m_uncompressed_size(uncompressed_size), m_expected_crc(expected_crc),
      m_pszBaseFileName(pszBaseFileName ? CPLStrdup(pszBaseFileName) : nullptr),
      m_bWriteProperties(CPLTestBool(
          CPLGetConfigOption("CPL_VSIL_GZIP_WRITE_PROPERTIES", "YES"))),
      m_bCanSaveInfo(
          CPLTestBool(CPLGetConfigOption("CPL_VSIL_GZIP_SAVE_INFO", "YES"))),
      stream(), crc(0), m_transparent(transparent)
{
    if (compressed_size || transparent)
    {
        m_compressed_size = compressed_size;
    }
    else
    {
        if (poBaseHandle->Seek(0, SEEK_END) != 0)
            CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
        m_compressed_size = poBaseHandle->Tell() - offset;
        compressed_size = m_compressed_size;
    }
    offsetEndCompressedData = offset + compressed_size;

    if (poBaseHandle->Seek(offset, SEEK_SET) != 0)
        CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");

    stream.zalloc = nullptr;
    stream.zfree = nullptr;
    stream.opaque = nullptr;
    stream.next_in = inbuf = nullptr;
    stream.next_out = outbuf = nullptr;
    stream.avail_in = stream.avail_out = 0;

    inbuf = static_cast<Byte *>(ALLOC(Z_BUFSIZE));
    stream.next_in = inbuf;

    int err = inflateInit2(&(stream), -MAX_WBITS);
    // windowBits is passed < 0 to tell that there is no zlib header.
    // Note that in this case inflate *requires* an extra "dummy" byte
    // after the compressed stream in order to complete decompression and
    // return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
    // present after the compressed stream.
    if (err != Z_OK || inbuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "inflateInit2 init failed");
        TRYFREE(inbuf);
        inbuf = nullptr;
        return;
    }
    stream.avail_out = static_cast<uInt>(Z_BUFSIZE);

    if (offset == 0)
        check_header();  // Skip the .gz header.
    startOff = poBaseHandle->Tell() - stream.avail_in;

    if (transparent == 0)
    {
        snapshot_byte_interval = std::max(static_cast<vsi_l_offset>(Z_BUFSIZE),
                                          compressed_size / 100);
        snapshots = static_cast<GZipSnapshot *>(CPLCalloc(
            sizeof(GZipSnapshot),
            static_cast<size_t>(compressed_size / snapshot_byte_interval + 1)));
    }
}

/************************************************************************/
/*                      SaveInfo_unlocked()                             */
/************************************************************************/

void VSIGZipHandle::SaveInfo_unlocked()
{
    if (m_pszBaseFileName && m_bCanSaveInfo)
    {
        VSIFilesystemHandler *poFSHandler =
            VSIFileManager::GetHandler("/vsigzip/");
        cpl::down_cast<VSIGZipFilesystemHandler *>(poFSHandler)
            ->SaveInfo_unlocked(this);
        m_bCanSaveInfo = false;
    }
}

/************************************************************************/
/*                      ~VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::~VSIGZipHandle()
{
    if (m_pszBaseFileName && m_bCanSaveInfo)
    {
        VSIFilesystemHandler *poFSHandler =
            VSIFileManager::GetHandler("/vsigzip/");
        cpl::down_cast<VSIGZipFilesystemHandler *>(poFSHandler)->SaveInfo(this);
    }

    if (stream.state != nullptr)
    {
        inflateEnd(&(stream));
    }

    TRYFREE(inbuf);
    TRYFREE(outbuf);

    if (snapshots != nullptr)
    {
        for (size_t i = 0; i < m_compressed_size / snapshot_byte_interval + 1;
             i++)
        {
            if (snapshots[i].posInBaseHandle)
            {
                inflateEnd(&(snapshots[i].stream));
            }
        }
        CPLFree(snapshots);
    }
    CPLFree(m_pszBaseFileName);

    CloseBaseHandle();
}

/************************************************************************/
/*                      check_header()                                  */
/************************************************************************/

void VSIGZipHandle::check_header()
{
    // Assure two bytes in the buffer so we can peek ahead -- handle case
    // where first byte of header is at the end of the buffer after the last
    // gzip segment.
    uInt len = stream.avail_in;
    if (len < 2)
    {
        if (len)
            inbuf[0] = stream.next_in[0];
        errno = 0;
        size_t nToRead = static_cast<size_t>(Z_BUFSIZE - len);
        CPLAssert(m_poBaseHandle->Tell() <= offsetEndCompressedData);
        if (m_poBaseHandle->Tell() + nToRead > offsetEndCompressedData)
            nToRead = static_cast<size_t>(offsetEndCompressedData -
                                          m_poBaseHandle->Tell());

        len = static_cast<uInt>(m_poBaseHandle->Read(inbuf + len, 1, nToRead));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 m_poBaseHandle->Tell(), offsetEndCompressedData);
#endif
        if (len == 0)  // && ferror(file)
        {
            if (m_poBaseHandle->Tell() != offsetEndCompressedData)
                z_err = Z_ERRNO;
        }
        stream.avail_in += len;
        stream.next_in = inbuf;
        if (stream.avail_in < 2)
        {
            m_transparent = stream.avail_in;
            return;
        }
    }

    // Peek ahead to check the gzip magic header.
    if (stream.next_in[0] != gz_magic[0] || stream.next_in[1] != gz_magic[1])
    {
        m_transparent = 1;
        return;
    }
    stream.avail_in -= 2;
    stream.next_in += 2;

    // Check the rest of the gzip header.
    const int method = get_byte();
    const int flags = get_byte();
    if (method != Z_DEFLATED || (flags & RESERVED) != 0)
    {
        z_err = Z_DATA_ERROR;
        return;
    }

    // Discard time, xflags and OS code:
    for (len = 0; len < 6; len++)
        CPL_IGNORE_RET_VAL(get_byte());

    if ((flags & EXTRA_FIELD) != 0)
    {
        // Skip the extra field.
        len = static_cast<uInt>(get_byte()) & 0xFF;
        len += (static_cast<uInt>(get_byte()) & 0xFF) << 8;
        // len is garbage if EOF but the loop below will quit anyway.
        while (len != 0 && get_byte() != EOF)
        {
            --len;
        }
    }

    if ((flags & ORIG_NAME) != 0)
    {
        // Skip the original file name.
        int c;
        while ((c = get_byte()) != 0 && c != EOF)
        {
        }
    }
    if ((flags & COMMENT) != 0)
    {
        // skip the .gz file comment.
        int c;
        while ((c = get_byte()) != 0 && c != EOF)
        {
        }
    }
    if ((flags & HEAD_CRC) != 0)
    {
        // Skip the header crc.
        for (len = 0; len < 2; len++)
            CPL_IGNORE_RET_VAL(get_byte());
    }
    z_err = z_eof ? Z_DATA_ERROR : Z_OK;
}

/************************************************************************/
/*                            get_byte()                                */
/************************************************************************/

int VSIGZipHandle::get_byte()
{
    if (z_eof)
        return EOF;
    if (stream.avail_in == 0)
    {
        errno = 0;
        size_t nToRead = static_cast<size_t>(Z_BUFSIZE);
        CPLAssert(m_poBaseHandle->Tell() <= offsetEndCompressedData);
        if (m_poBaseHandle->Tell() + nToRead > offsetEndCompressedData)
            nToRead = static_cast<size_t>(offsetEndCompressedData -
                                          m_poBaseHandle->Tell());
        stream.avail_in =
            static_cast<uInt>(m_poBaseHandle->Read(inbuf, 1, nToRead));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 m_poBaseHandle->Tell(), offsetEndCompressedData);
#endif
        if (stream.avail_in == 0)
        {
            z_eof = 1;
            if (m_poBaseHandle->Tell() != offsetEndCompressedData)
                z_err = Z_ERRNO;
            // if( ferror(file) ) z_err = Z_ERRNO;
            return EOF;
        }
        stream.next_in = inbuf;
    }
    stream.avail_in--;
    return *(stream.next_in)++;
}

/************************************************************************/
/*                            gzrewind()                                */
/************************************************************************/

int VSIGZipHandle::gzrewind()
{
    z_err = Z_OK;
    z_eof = 0;
    stream.avail_in = 0;
    stream.next_in = inbuf;
    crc = 0;
    if (!m_transparent)
        CPL_IGNORE_RET_VAL(inflateReset(&stream));
    in = 0;
    out = 0;
    return m_poBaseHandle->Seek(startOff, SEEK_SET);
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSIGZipHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    return gzseek(nOffset, nWhence) ? 0 : -1;
}

/************************************************************************/
/*                            gzseek()                                  */
/************************************************************************/

bool VSIGZipHandle::gzseek(vsi_l_offset offset, int whence)
{
    const vsi_l_offset original_offset = offset;
    const int original_nWhence = whence;

    z_eof = 0;
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Seek(" CPL_FRMT_GUIB ",%d)", offset, whence);
#endif

    if (m_transparent)
    {
        stream.avail_in = 0;
        stream.next_in = inbuf;
        if (whence == SEEK_CUR)
        {
            if (out + offset > m_compressed_size)
            {
                CPL_VSIL_GZ_RETURN(FALSE);
                return false;
            }

            offset = startOff + out + offset;
        }
        else if (whence == SEEK_SET)
        {
            if (offset > m_compressed_size)
            {
                CPL_VSIL_GZ_RETURN(FALSE);
                return false;
            }

            offset = startOff + offset;
        }
        else if (whence == SEEK_END)
        {
            // Commented test: because vsi_l_offset is unsigned (for the moment)
            // so no way to seek backward. See #1590 */
            if (offset > 0)  // || -offset > compressed_size
            {
                CPL_VSIL_GZ_RETURN(FALSE);
                return false;
            }

            offset = startOff + m_compressed_size - offset;
        }
        else
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }

        if (m_poBaseHandle->Seek(offset, SEEK_SET) < 0)
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }

        out = offset - startOff;
        in = out;
        return true;
    }

    // whence == SEEK_END is unsuppored in original gzseek.
    if (whence == SEEK_END)
    {
        // If we known the uncompressed size, we can fake a jump to
        // the end of the stream.
        if (offset == 0 && m_uncompressed_size != 0)
        {
            out = m_uncompressed_size;
            return true;
        }

        // We don't know the uncompressed size. This is unfortunate.
        // Do the slow version.
        static int firstWarning = 1;
        if (m_compressed_size > 10 * 1024 * 1024 && firstWarning)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "VSIFSeekL(xxx, SEEK_END) may be really slow "
                     "on GZip streams.");
            firstWarning = 0;
        }

        whence = SEEK_CUR;
        offset = 1024 * 1024 * 1024;
        offset *= 1024 * 1024;
    }

    // Rest of function is for reading only.

    // Compute absolute position.
    if (whence == SEEK_CUR)
    {
        offset += out;
    }

    // For a negative seek, rewind and use positive seek.
    if (offset >= out)
    {
        offset -= out;
    }
    else if (gzrewind() < 0)
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    if (z_err != Z_OK && z_err != Z_STREAM_END)
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    for (unsigned int i = 0; i < m_compressed_size / snapshot_byte_interval + 1;
         i++)
    {
        if (snapshots[i].posInBaseHandle == 0)
            break;
        if (snapshots[i].out <= out + offset &&
            (i == m_compressed_size / snapshot_byte_interval ||
             snapshots[i + 1].out == 0 || snapshots[i + 1].out > out + offset))
        {
            if (out >= snapshots[i].out)
                break;

#ifdef ENABLE_DEBUG
            CPLDebug("SNAPSHOT",
                     "using snapshot %d : "
                     "posInBaseHandle(snapshot)=" CPL_FRMT_GUIB
                     " in(snapshot)=" CPL_FRMT_GUIB
                     " out(snapshot)=" CPL_FRMT_GUIB " out=" CPL_FRMT_GUIB
                     " offset=" CPL_FRMT_GUIB,
                     i, snapshots[i].posInBaseHandle, snapshots[i].in,
                     snapshots[i].out, out, offset);
#endif
            offset = out + offset - snapshots[i].out;
            if (m_poBaseHandle->Seek(snapshots[i].posInBaseHandle, SEEK_SET) !=
                0)
                CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");

            inflateEnd(&stream);
            inflateCopy(&stream, &snapshots[i].stream);
            crc = snapshots[i].crc;
            m_transparent = snapshots[i].transparent;
            in = snapshots[i].in;
            out = snapshots[i].out;
            break;
        }
    }

    // Offset is now the number of bytes to skip.

    if (offset != 0 && outbuf == nullptr)
    {
        outbuf = static_cast<Byte *>(ALLOC(Z_BUFSIZE));
        if (outbuf == nullptr)
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
    }

    if (original_nWhence == SEEK_END && z_err == Z_STREAM_END)
    {
        return true;
    }

    while (offset > 0)
    {
        int size = Z_BUFSIZE;
        if (offset < static_cast<vsi_l_offset>(Z_BUFSIZE))
            size = static_cast<int>(offset);

        int read_size =
            static_cast<int>(Read(outbuf, 1, static_cast<uInt>(size)));
        if (original_nWhence == SEEK_END)
        {
            if (size != read_size)
            {
                z_err = Z_STREAM_END;
                break;
            }
        }
        else if (read_size == 0)
        {
            // CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
        offset -= read_size;
    }
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "gzseek at offset " CPL_FRMT_GUIB, out);
#endif

    if (original_offset == 0 && original_nWhence == SEEK_END)
    {
        m_uncompressed_size = out;

        if (m_pszBaseFileName &&
            !STARTS_WITH_CI(m_pszBaseFileName, "/vsicurl/") &&
            m_bWriteProperties)
        {
            CPLString osCacheFilename(m_pszBaseFileName);
            osCacheFilename += ".properties";

            // Write a .properties file to avoid seeking next time.
            VSILFILE *fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "wb");
            if (fpCacheLength)
            {
                char szBuffer[32] = {};

                CPLPrintUIntBig(szBuffer, m_compressed_size, 31);
                char *pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ')
                    pszFirstNonSpace++;
                CPL_IGNORE_RET_VAL(VSIFPrintfL(
                    fpCacheLength, "compressed_size=%s\n", pszFirstNonSpace));

                CPLPrintUIntBig(szBuffer, m_uncompressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ')
                    pszFirstNonSpace++;
                CPL_IGNORE_RET_VAL(VSIFPrintfL(
                    fpCacheLength, "uncompressed_size=%s\n", pszFirstNonSpace));

                CPL_IGNORE_RET_VAL(VSIFCloseL(fpCacheLength));
            }
        }
    }

    return true;
}

/************************************************************************/
/*                              Tell()                                  */
/************************************************************************/

vsi_l_offset VSIGZipHandle::Tell()
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Tell() = " CPL_FRMT_GUIB, out);
#endif
    return out;
}

/************************************************************************/
/*                              Read()                                  */
/************************************************************************/

size_t VSIGZipHandle::Read(void *const buf, size_t const nSize,
                           size_t const nMemb)
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read(%p, %d, %d)", buf, static_cast<int>(nSize),
             static_cast<int>(nMemb));
#endif

    if ((z_eof && in == 0) || z_err == Z_STREAM_END)
    {
        z_eof = 1;
        in = 0;
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", "Read: Eof");
#endif
        return 0; /* EOF */
    }

    const unsigned len =
        static_cast<unsigned int>(nSize) * static_cast<unsigned int>(nMemb);
    Bytef *pStart =
        static_cast<Bytef *>(buf);  // Start off point for crc computation.
    // == stream.next_out but not forced far (for MSDOS).
    Byte *next_out = static_cast<Byte *>(buf);
    stream.next_out = static_cast<Bytef *>(buf);
    stream.avail_out = len;

    while (stream.avail_out != 0)
    {
        if (m_transparent)
        {
            // Copy first the lookahead bytes:
            uInt nRead = 0;
            uInt n = stream.avail_in;
            if (n > stream.avail_out)
                n = stream.avail_out;
            if (n > 0)
            {
                memcpy(stream.next_out, stream.next_in, n);
                next_out += n;
                stream.next_out = next_out;
                stream.next_in += n;
                stream.avail_out -= n;
                stream.avail_in -= n;
                nRead += n;
            }
            if (stream.avail_out > 0)
            {
                const uInt nToRead = static_cast<uInt>(
                    std::min(m_compressed_size - (in + nRead),
                             static_cast<vsi_l_offset>(stream.avail_out)));
                uInt nReadFromFile = static_cast<uInt>(
                    m_poBaseHandle->Read(next_out, 1, nToRead));
                stream.avail_out -= nReadFromFile;
                nRead += nReadFromFile;
            }
            in += nRead;
            out += nRead;
            if (nRead < len)
            {
                z_eof = 1;
                in = 0;
            }
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", "Read return %d", static_cast<int>(nRead / nSize));
#endif
            return static_cast<int>(nRead) / nSize;
        }
        if (stream.avail_in == 0 && !z_eof)
        {
            vsi_l_offset posInBaseHandle = m_poBaseHandle->Tell();
            if (posInBaseHandle - startOff > m_compressed_size)
            {
                // If we reach here, file size has changed (because at
                // construction time startOff + m_compressed_size marked the
                // end of file).
                // We should probably have a better fix than that, by detecting
                // at open time that the saved snapshot is not valid and
                // discarding it.
                CPLError(CE_Failure, CPLE_AppDefined,
                         "File size of underlying /vsigzip/ file has changed");
                z_eof = 1;
                in = 0;
                CPL_VSIL_GZ_RETURN(0);
                return 0;
            }
            GZipSnapshot *snapshot = &snapshots[(posInBaseHandle - startOff) /
                                                snapshot_byte_interval];
            if (snapshot->posInBaseHandle == 0)
            {
                snapshot->crc = crc32(
                    crc, pStart, static_cast<uInt>(stream.next_out - pStart));
#ifdef ENABLE_DEBUG
                CPLDebug("SNAPSHOT",
                         "creating snapshot %d : "
                         "posInBaseHandle=" CPL_FRMT_GUIB " in=" CPL_FRMT_GUIB
                         " out=" CPL_FRMT_GUIB " crc=%X",
                         static_cast<int>((posInBaseHandle - startOff) /
                                          snapshot_byte_interval),
                         posInBaseHandle, in, out,
                         static_cast<unsigned int>(snapshot->crc));
#endif
                snapshot->posInBaseHandle = posInBaseHandle;
                inflateCopy(&snapshot->stream, &stream);
                snapshot->transparent = m_transparent;
                snapshot->in = in;
                snapshot->out = out;

                if (out > m_nLastReadOffset)
                    m_nLastReadOffset = out;
            }

            errno = 0;
            stream.avail_in =
                static_cast<uInt>(m_poBaseHandle->Read(inbuf, 1, Z_BUFSIZE));
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                     m_poBaseHandle->Tell(), offsetEndCompressedData);
#endif
            if (m_poBaseHandle->Tell() > offsetEndCompressedData)
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
#endif
                stream.avail_in = stream.avail_in -
                                  static_cast<uInt>(m_poBaseHandle->Tell() -
                                                    offsetEndCompressedData);
                if (m_poBaseHandle->Seek(offsetEndCompressedData, SEEK_SET) !=
                    0)
                    CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
#endif
            }
            if (stream.avail_in == 0)
            {
                z_eof = 1;
                if (m_poBaseHandle->Tell() != offsetEndCompressedData)
                {
                    z_err = Z_ERRNO;
                    break;
                }
            }
            stream.next_in = inbuf;
        }
        in += stream.avail_in;
        out += stream.avail_out;
        z_err = inflate(&(stream), Z_NO_FLUSH);
        in -= stream.avail_in;
        out -= stream.avail_out;

        if (z_err == Z_STREAM_END && m_compressed_size != 2)
        {
            // Check CRC and original size.
            crc =
                crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));
            pStart = stream.next_out;
            if (m_expected_crc)
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "Computed CRC = %X. Expected CRC = %X",
                         static_cast<unsigned int>(crc),
                         static_cast<unsigned int>(m_expected_crc));
#endif
            }
            if (m_expected_crc != 0 && m_expected_crc != crc)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "CRC error. Got %X instead of %X",
                         static_cast<unsigned int>(crc),
                         static_cast<unsigned int>(m_expected_crc));
                z_err = Z_DATA_ERROR;
            }
            else if (m_expected_crc == 0)
            {
                const uLong read_crc = static_cast<unsigned long>(getLong());
                if (read_crc != crc)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "CRC error. Got %X instead of %X",
                             static_cast<unsigned int>(crc),
                             static_cast<unsigned int>(read_crc));
                    z_err = Z_DATA_ERROR;
                }
                else
                {
                    CPL_IGNORE_RET_VAL(getLong());
                    // The uncompressed length returned by above getlong() may
                    // be different from out in case of concatenated .gz files.
                    // Check for such files:
                    check_header();
                    if (z_err == Z_OK)
                    {
                        inflateReset(&(stream));
                        crc = 0;
                    }
                }
            }
        }
        if (z_err != Z_OK || z_eof)
            break;
    }
    crc = crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));

    size_t ret = (len - stream.avail_out) / nSize;
    if (z_err != Z_OK && z_err != Z_STREAM_END)
    {
        z_eof = 1;
        in = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In file %s, at line %d, decompression failed with "
                 "z_err = %d, return = %d",
                 __FILE__, __LINE__, z_err, static_cast<int>(ret));
    }

#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read return %d (z_err=%d, z_eof=%d)",
             static_cast<int>(ret), z_err, z_eof);
#endif
    return ret;
}

/************************************************************************/
/*                              getLong()                               */
/************************************************************************/

uLong VSIGZipHandle::getLong()
{
    uLong x = static_cast<uLong>(get_byte()) & 0xFF;

    x += (static_cast<uLong>(get_byte()) & 0xFF) << 8;
    x += (static_cast<uLong>(get_byte()) & 0xFF) << 16;
    const int c = get_byte();
    if (c == EOF)
    {
        z_err = Z_DATA_ERROR;
        return 0;
    }
    x += static_cast<uLong>(c) << 24;
    // coverity[overflow_sink]
    return x;
}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

size_t VSIGZipHandle::Write(const void * /* pBuffer */, size_t /* nSize */,
                            size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFWriteL is not supported on GZip streams");
    return 0;
}

/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/

int VSIGZipHandle::Eof()
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Eof()");
#endif
    return z_eof && in == 0;
}

/************************************************************************/
/*                              Flush()                                 */
/************************************************************************/

int VSIGZipHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIGZipHandle::Close()
{
    return 0;
}

#ifdef ENABLE_DEFLATE64

/************************************************************************/
/*                            Duplicate()                               */
/************************************************************************/

VSIDeflate64Handle *VSIDeflate64Handle::Duplicate()
{
    CPLAssert(m_offset == 0);
    CPLAssert(m_compressed_size != 0);
    CPLAssert(m_pszBaseFileName != nullptr);

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(m_pszBaseFileName);

    VSIVirtualHandle *poNewBaseHandle =
        poFSHandler->Open(m_pszBaseFileName, "rb");

    if (poNewBaseHandle == nullptr)
        return nullptr;

    VSIDeflate64Handle *poHandle =
        new VSIDeflate64Handle(poNewBaseHandle, m_pszBaseFileName, 0,
                               m_compressed_size, m_uncompressed_size);
    if (!(poHandle->IsInitOK()))
    {
        delete poHandle;
        return nullptr;
    }

    // Most important: duplicate the snapshots!

    for (unsigned int i = 0; i < m_compressed_size / snapshot_byte_interval + 1;
         i++)
    {
        if (snapshots[i].posInBaseHandle == 0)
            break;

        poHandle->snapshots[i].posInBaseHandle = snapshots[i].posInBaseHandle;
        if (inflateBack9Copy(&poHandle->snapshots[i].stream,
                             &snapshots[i].stream) != Z_OK)
            CPLError(CE_Failure, CPLE_AppDefined, "inflateBack9Copy() failed");
        poHandle->snapshots[i].crc = snapshots[i].crc;
        poHandle->snapshots[i].in = snapshots[i].in;
        poHandle->snapshots[i].out = snapshots[i].out;
        poHandle->snapshots[i].extraOutput = snapshots[i].extraOutput;
        poHandle->snapshots[i].m_bStreamEndReached =
            snapshots[i].m_bStreamEndReached;
    }

    return poHandle;
}

/************************************************************************/
/*                     CloseBaseHandle()                                */
/************************************************************************/

bool VSIDeflate64Handle::CloseBaseHandle()
{
    bool bRet = true;
    if (m_poBaseHandle)
    {
        bRet = m_poBaseHandle->Close() == 0;
        delete m_poBaseHandle;
    }
    m_poBaseHandle = nullptr;
    return bRet;
}

/************************************************************************/
/*                       VSIDeflate64Handle()                                */
/************************************************************************/

VSIDeflate64Handle::VSIDeflate64Handle(VSIVirtualHandle *poBaseHandle,
                                       const char *pszBaseFileName,
                                       vsi_l_offset offset,
                                       vsi_l_offset compressed_size,
                                       vsi_l_offset uncompressed_size,
                                       uLong expected_crc)
    : m_poBaseHandle(poBaseHandle),
#ifdef DEBUG
      m_offset(offset),
#endif
      m_uncompressed_size(uncompressed_size), m_expected_crc(expected_crc),
      m_pszBaseFileName(pszBaseFileName ? CPLStrdup(pszBaseFileName) : nullptr),
      stream(), crc(0)
{
    if (compressed_size)
    {
        m_compressed_size = compressed_size;
    }
    else
    {
        if (poBaseHandle->Seek(0, SEEK_END) != 0)
            CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
        m_compressed_size = poBaseHandle->Tell() - offset;
        compressed_size = m_compressed_size;
    }
    offsetEndCompressedData = offset + compressed_size;

    if (poBaseHandle->Seek(offset, SEEK_SET) != 0)
        CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");

    stream.zalloc = nullptr;
    stream.zfree = nullptr;
    stream.opaque = nullptr;
    stream.next_in = inbuf = nullptr;
    stream.next_out = outbuf = nullptr;
    stream.avail_in = stream.avail_out = 0;

    inbuf = static_cast<Byte *>(ALLOC(Z_BUFSIZE));
    stream.next_in = inbuf;

    int err = inflateBack9Init(&(stream), nullptr);
    // Note that in this case inflate *requires* an extra "dummy" byte
    // after the compressed stream in order to complete decompression and
    // return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
    // present after the compressed stream.
    if (err != Z_OK || inbuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "inflateBack9Init init failed");
        TRYFREE(inbuf);
        inbuf = nullptr;
        return;
    }
    startOff = poBaseHandle->Tell() - stream.avail_in;

    snapshot_byte_interval =
        std::max(static_cast<vsi_l_offset>(Z_BUFSIZE), compressed_size / 100);
    snapshots.resize(
        static_cast<size_t>(compressed_size / snapshot_byte_interval + 1));
}

/************************************************************************/
/*                      ~VSIDeflate64Handle()                       */
/************************************************************************/

VSIDeflate64Handle::~VSIDeflate64Handle()
{
    if (stream.state != nullptr)
    {
        inflateBack9End(&(stream));
    }

    TRYFREE(inbuf);
    TRYFREE(outbuf);

    for (auto &snapshot : snapshots)
    {
        if (snapshot.posInBaseHandle)
        {
            inflateBack9End(&(snapshot.stream));
        }
    }
    CPLFree(m_pszBaseFileName);

    CloseBaseHandle();
}

/************************************************************************/
/*                            gzrewind()                                */
/************************************************************************/

int VSIDeflate64Handle::gzrewind()
{
    m_bStreamEndReached = false;
    extraOutput.clear();
    z_err = Z_OK;
    z_eof = 0;
    stream.avail_in = 0;
    stream.next_in = inbuf;
    crc = 0;
    CPL_IGNORE_RET_VAL(inflateBack9End(&stream));
    CPL_IGNORE_RET_VAL(inflateBack9Init(&stream, nullptr));
    in = 0;
    out = 0;
    return m_poBaseHandle->Seek(startOff, SEEK_SET);
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSIDeflate64Handle::Seek(vsi_l_offset nOffset, int nWhence)
{
    return gzseek(nOffset, nWhence) ? 0 : -1;
}

/************************************************************************/
/*                            gzseek()                                  */
/************************************************************************/

bool VSIDeflate64Handle::gzseek(vsi_l_offset offset, int whence)
{
    const vsi_l_offset original_offset = offset;
    const int original_nWhence = whence;

    z_eof = 0;
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Seek(" CPL_FRMT_GUIB ",%d)", offset, whence);
#endif

    // whence == SEEK_END is unsuppored in original gzseek.
    if (whence == SEEK_END)
    {
        // If we known the uncompressed size, we can fake a jump to
        // the end of the stream.
        if (offset == 0 && m_uncompressed_size != 0)
        {
            out = m_uncompressed_size;
            return true;
        }

        // We don't know the uncompressed size. This is unfortunate.
        // Do the slow version.
        static int firstWarning = 1;
        if (m_compressed_size > 10 * 1024 * 1024 && firstWarning)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "VSIFSeekL(xxx, SEEK_END) may be really slow "
                     "on GZip streams.");
            firstWarning = 0;
        }

        whence = SEEK_CUR;
        offset = 1024 * 1024 * 1024;
        offset *= 1024 * 1024;
    }

    // Rest of function is for reading only.

    // Compute absolute position.
    if (whence == SEEK_CUR)
    {
        offset += out;
    }

    // For a negative seek, rewind and use positive seek.
    if (offset >= out)
    {
        offset -= out;
    }
    else if (gzrewind() < 0)
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    if (z_err != Z_OK && z_err != Z_STREAM_END)
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    for (unsigned int i = 0; i < m_compressed_size / snapshot_byte_interval + 1;
         i++)
    {
        if (snapshots[i].posInBaseHandle == 0)
            break;
        if (snapshots[i].out <= out + offset &&
            (i == m_compressed_size / snapshot_byte_interval ||
             snapshots[i + 1].out == 0 || snapshots[i + 1].out > out + offset))
        {
            if (out >= snapshots[i].out)
                break;

#ifdef ENABLE_DEBUG
            CPLDebug("SNAPSHOT",
                     "using snapshot %d : "
                     "posInBaseHandle(snapshot)=" CPL_FRMT_GUIB
                     " in(snapshot)=" CPL_FRMT_GUIB
                     " out(snapshot)=" CPL_FRMT_GUIB " out=" CPL_FRMT_GUIB
                     " offset=" CPL_FRMT_GUIB,
                     i, snapshots[i].posInBaseHandle, snapshots[i].in,
                     snapshots[i].out, out, offset);
#endif
            offset = out + offset - snapshots[i].out;
            if (m_poBaseHandle->Seek(snapshots[i].posInBaseHandle, SEEK_SET) !=
                0)
                CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");

            inflateBack9End(&stream);
            if (inflateBack9Copy(&stream, &snapshots[i].stream) != Z_OK)
                CPLError(CE_Failure, CPLE_AppDefined,
                         "inflateBack9Copy() failed");
            crc = snapshots[i].crc;
            in = snapshots[i].in;
            out = snapshots[i].out;
            extraOutput = snapshots[i].extraOutput;
            m_bStreamEndReached = snapshots[i].m_bStreamEndReached;
            break;
        }
    }

    // Offset is now the number of bytes to skip.

    if (offset != 0 && outbuf == nullptr)
    {
        outbuf = static_cast<Byte *>(ALLOC(Z_BUFSIZE));
        if (outbuf == nullptr)
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
    }

    if (original_nWhence == SEEK_END && z_err == Z_STREAM_END)
    {
        return true;
    }

    while (offset > 0)
    {
        int size = Z_BUFSIZE;
        if (offset < static_cast<vsi_l_offset>(Z_BUFSIZE))
            size = static_cast<int>(offset);

        int read_size =
            static_cast<int>(Read(outbuf, 1, static_cast<uInt>(size)));
        if (original_nWhence == SEEK_END)
        {
            if (size != read_size)
            {
                z_err = Z_STREAM_END;
                break;
            }
        }
        else if (read_size == 0)
        {
            // CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
        offset -= read_size;
    }
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "gzseek at offset " CPL_FRMT_GUIB, out);
#endif

    if (original_offset == 0 && original_nWhence == SEEK_END)
    {
        m_uncompressed_size = out;
    }

    return true;
}

/************************************************************************/
/*                              Tell()                                  */
/************************************************************************/

vsi_l_offset VSIDeflate64Handle::Tell()
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Tell() = " CPL_FRMT_GUIB, out);
#endif
    return out;
}

/************************************************************************/
/*                              Read()                                  */
/************************************************************************/

size_t VSIDeflate64Handle::Read(void *const buf, size_t const nSize,
                                size_t const nMemb)
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read(%p, %d, %d)", buf, static_cast<int>(nSize),
             static_cast<int>(nMemb));
#endif

    if ((z_eof && in == 0) || z_err == Z_STREAM_END)
    {
        z_eof = 1;
        in = 0;
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", "Read: Eof");
#endif
        return 0; /* EOF */
    }

    const unsigned len =
        static_cast<unsigned int>(nSize) * static_cast<unsigned int>(nMemb);
    Bytef *pStart =
        static_cast<Bytef *>(buf);  // Start off point for crc computation.
    // == stream.next_out but not forced far (for MSDOS).
    stream.next_out = static_cast<Bytef *>(buf);
    stream.avail_out = len;

    while (stream.avail_out != 0)
    {
        if (!extraOutput.empty())
        {
            if (extraOutput.size() >= stream.avail_out)
            {
                memcpy(stream.next_out, extraOutput.data(), stream.avail_out);
                extraOutput.erase(extraOutput.begin(),
                                  extraOutput.begin() + stream.avail_out);
                out += stream.avail_out;
                stream.next_out += stream.avail_out;
                stream.avail_out = 0;
            }
            else
            {
                memcpy(stream.next_out, extraOutput.data(), extraOutput.size());
                stream.next_out += extraOutput.size();
                out += static_cast<uInt>(extraOutput.size());
                stream.avail_out -= static_cast<uInt>(extraOutput.size());
                CPLAssert(stream.avail_out > 0);
                extraOutput.clear();
            }
            z_err = Z_OK;
        }

        if (stream.avail_in == 0 && !z_eof)
        {
            vsi_l_offset posInBaseHandle = m_poBaseHandle->Tell();
            if (posInBaseHandle - startOff > m_compressed_size)
            {
                // If we reach here, file size has changed (because at
                // construction time startOff + m_compressed_size marked the
                // end of file).
                // We should probably have a better fix than that, by detecting
                // at open time that the saved snapshot is not valid and
                // discarding it.
                CPLError(CE_Failure, CPLE_AppDefined,
                         "File size of underlying /vsigzip/ file has changed");
                z_eof = 1;
                in = 0;
                CPL_VSIL_GZ_RETURN(0);
                return 0;
            }
            auto snapshot = &snapshots[static_cast<size_t>(
                (posInBaseHandle - startOff) / snapshot_byte_interval)];
            if (snapshot->posInBaseHandle == 0)
            {
                snapshot->crc = crc32(
                    crc, pStart, static_cast<uInt>(stream.next_out - pStart));
#ifdef ENABLE_DEBUG
                CPLDebug("SNAPSHOT",
                         "creating snapshot %d : "
                         "posInBaseHandle=" CPL_FRMT_GUIB " in=" CPL_FRMT_GUIB
                         " out=" CPL_FRMT_GUIB " crc=%X",
                         static_cast<int>((posInBaseHandle - startOff) /
                                          snapshot_byte_interval),
                         posInBaseHandle, in, out,
                         static_cast<unsigned int>(snapshot->crc));
#endif
                snapshot->posInBaseHandle = posInBaseHandle;
                if (inflateBack9Copy(&snapshot->stream, &stream) != Z_OK)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "inflateBack9Copy() failed");
                snapshot->in = in;
                snapshot->out = out;
                snapshot->extraOutput = extraOutput;
                snapshot->m_bStreamEndReached = m_bStreamEndReached;
            }

            errno = 0;
            stream.avail_in =
                static_cast<uInt>(m_poBaseHandle->Read(inbuf, 1, Z_BUFSIZE));
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                     m_poBaseHandle->Tell(), offsetEndCompressedData);
#endif
            if (m_poBaseHandle->Tell() > offsetEndCompressedData)
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
#endif
                stream.avail_in = stream.avail_in -
                                  static_cast<uInt>(m_poBaseHandle->Tell() -
                                                    offsetEndCompressedData);
                if (m_poBaseHandle->Seek(offsetEndCompressedData, SEEK_SET) !=
                    0)
                    CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
#endif
            }
            if (stream.avail_in == 0)
            {
                z_eof = 1;
                if (m_poBaseHandle->Tell() != offsetEndCompressedData)
                {
                    z_err = Z_ERRNO;
                    break;
                }
            }
            stream.next_in = inbuf;
        }

        struct InOutCallback
        {
            vsi_l_offset *pOut = nullptr;
            std::vector<GByte> *pExtraOutput = nullptr;
            z_stream *pStream = nullptr;

            static unsigned inCbk(void FAR *, z_const unsigned char FAR *FAR *)
            {
                return 0;
            }

            static int outCbk(void FAR *user_data, unsigned char FAR *data,
                              unsigned len)
            {
                auto self = static_cast<InOutCallback *>(user_data);
                if (self->pStream->avail_out >= len)
                {
                    memcpy(self->pStream->next_out, data, len);
                    *(self->pOut) += len;
                    self->pStream->next_out += len;
                    self->pStream->avail_out -= len;
                }
                else
                {
                    if (self->pStream->avail_out != 0)
                    {
                        memcpy(self->pStream->next_out, data,
                               self->pStream->avail_out);
                        *(self->pOut) += self->pStream->avail_out;
                        data += self->pStream->avail_out;
                        len -= self->pStream->avail_out;
                        self->pStream->next_out += self->pStream->avail_out;
                        self->pStream->avail_out = 0;
                    }
                    if (len > 0)
                    {
                        self->pExtraOutput->insert(self->pExtraOutput->end(),
                                                   data, data + len);
                    }
                }
                return 0;
            }
        };

        InOutCallback cbkData;
        cbkData.pOut = &out;
        cbkData.pExtraOutput = &extraOutput;
        cbkData.pStream = &stream;

        if (stream.avail_out)
        {
            if (m_bStreamEndReached)
                z_err = Z_STREAM_END;
            else
            {
                in += stream.avail_in;
                z_err = inflateBack9(&(stream), InOutCallback::inCbk, &cbkData,
                                     InOutCallback::outCbk, &cbkData);
                in -= stream.avail_in;
            }
        }
        if (z_err == Z_BUF_ERROR && stream.next_in == Z_NULL)
            z_err = Z_OK;
        else if (!extraOutput.empty() && z_err == Z_STREAM_END)
        {
            m_bStreamEndReached = true;
            z_err = Z_OK;
        }

        if (z_err == Z_STREAM_END /*&& m_compressed_size != 2*/)
        {
            // Check CRC and original size.
            crc =
                crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));
            pStart = stream.next_out;
            if (m_expected_crc)
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "Computed CRC = %X. Expected CRC = %X",
                         static_cast<unsigned int>(crc),
                         static_cast<unsigned int>(m_expected_crc));
#endif
            }
            if (m_expected_crc != 0 && m_expected_crc != crc)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "CRC error. Got %X instead of %X",
                         static_cast<unsigned int>(crc),
                         static_cast<unsigned int>(m_expected_crc));
                z_err = Z_DATA_ERROR;
            }
        }
        if (z_err != Z_OK || z_eof)
            break;
    }
    crc = crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));

    size_t ret = (len - stream.avail_out) / nSize;
    if (z_err != Z_OK && z_err != Z_STREAM_END)
    {
        z_eof = 1;
        in = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In file %s, at line %d, decompression failed with "
                 "z_err = %d, return = %d",
                 __FILE__, __LINE__, z_err, static_cast<int>(ret));
    }

#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read return %d (z_err=%d, z_eof=%d)",
             static_cast<int>(ret), z_err, z_eof);
#endif
    return ret;
}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

size_t VSIDeflate64Handle::Write(const void * /* pBuffer */, size_t /* nSize */,
                                 size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFWriteL is not supported on GZip streams");
    return 0;
}

/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/

int VSIDeflate64Handle::Eof()
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Eof()");
#endif
    return z_eof && in == 0;
}

/************************************************************************/
/*                              Flush()                                 */
/************************************************************************/

int VSIDeflate64Handle::Flush()
{
    return 0;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIDeflate64Handle::Close()
{
    return 0;
}
#endif

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipWriteHandleMT                           */
/* ==================================================================== */
/************************************************************************/

class VSIGZipWriteHandleMT final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGZipWriteHandleMT)

    VSIVirtualHandle *poBaseHandle_ = nullptr;
    vsi_l_offset nCurOffset_ = 0;
    uLong nCRC_ = 0;
    int nDeflateType_ = CPL_DEFLATE_TYPE_GZIP;
    bool bAutoCloseBaseHandle_ = false;
    int nThreads_ = 0;
    std::unique_ptr<CPLWorkerThreadPool> poPool_{};
    std::list<std::string *> aposBuffers_{};
    std::string *pCurBuffer_ = nullptr;
    std::mutex sMutex_{};
    int nSeqNumberGenerated_ = 0;
    int nSeqNumberExpected_ = 0;
    int nSeqNumberExpectedCRC_ = 0;
    size_t nChunkSize_ = 0;
    bool bHasErrored_ = false;

    struct Job
    {
        VSIGZipWriteHandleMT *pParent_ = nullptr;
        std::string *pBuffer_ = nullptr;
        int nSeqNumber_ = 0;
        bool bFinish_ = false;
        bool bInCRCComputation_ = false;

        std::string sCompressedData_{};
        uLong nCRC_ = 0;
    };

    std::list<Job *> apoFinishedJobs_{};
    std::list<Job *> apoCRCFinishedJobs_{};
    std::list<Job *> apoFreeJobs_{};
    vsi_l_offset nStartOffset_ = 0;
    size_t nSOZIPIndexEltSize_ = 0;
    std::vector<uint8_t> *panSOZIPIndex_ = nullptr;

    static void DeflateCompress(void *inData);
    static void CRCCompute(void *inData);
    bool ProcessCompletedJobs();
    Job *GetJobObject();
#ifdef DEBUG_VERBOSE
    void DumpState();
#endif

  public:
    VSIGZipWriteHandleMT(VSIVirtualHandle *poBaseHandle, int nDeflateType,
                         bool bAutoCloseBaseHandleIn, int nThreads,
                         size_t nChunkSize, size_t nSOZIPIndexEltSize,
                         std::vector<uint8_t> *panSOZIPIndex);

    ~VSIGZipWriteHandleMT() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

/************************************************************************/
/*                        VSIGZipWriteHandleMT()                        */
/************************************************************************/

VSIGZipWriteHandleMT::VSIGZipWriteHandleMT(VSIVirtualHandle *poBaseHandle,
                                           int nDeflateType,
                                           bool bAutoCloseBaseHandleIn,
                                           int nThreads, size_t nChunkSize,
                                           size_t nSOZIPIndexEltSize,
                                           std::vector<uint8_t> *panSOZIPIndex)
    : poBaseHandle_(poBaseHandle), nDeflateType_(nDeflateType),
      bAutoCloseBaseHandle_(bAutoCloseBaseHandleIn), nThreads_(nThreads),
      nChunkSize_(nChunkSize), nSOZIPIndexEltSize_(nSOZIPIndexEltSize),
      panSOZIPIndex_(panSOZIPIndex)
{
    if (nChunkSize_ == 0)
    {
        const char *pszChunkSize =
            CPLGetConfigOption("CPL_VSIL_DEFLATE_CHUNK_SIZE", "1024K");
        nChunkSize_ = static_cast<size_t>(atoi(pszChunkSize));
        if (strchr(pszChunkSize, 'K'))
            nChunkSize_ *= 1024;
        else if (strchr(pszChunkSize, 'M'))
            nChunkSize_ *= 1024 * 1024;
        nChunkSize_ =
            std::max(static_cast<size_t>(4 * 1024),
                     std::min(static_cast<size_t>(UINT_MAX), nChunkSize_));
    }

    for (int i = 0; i < 1 + nThreads_; i++)
        aposBuffers_.emplace_back(new std::string());

    nStartOffset_ = poBaseHandle_->Tell();
    if (nDeflateType == CPL_DEFLATE_TYPE_GZIP)
    {
        char header[11] = {};

        // Write a very simple .gz header:
        snprintf(header, sizeof(header), "%c%c%c%c%c%c%c%c%c%c", gz_magic[0],
                 gz_magic[1], Z_DEFLATED, 0 /*flags*/, 0, 0, 0, 0 /*time*/,
                 0 /*xflags*/, 0x03);
        poBaseHandle_->Write(header, 1, 10);
    }
}

/************************************************************************/
/*                       ~VSIGZipWriteHandleMT()                        */
/************************************************************************/

VSIGZipWriteHandleMT::~VSIGZipWriteHandleMT()

{
    VSIGZipWriteHandleMT::Close();
    for (auto &psJob : apoFinishedJobs_)
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for (auto &psJob : apoCRCFinishedJobs_)
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for (auto &psJob : apoFreeJobs_)
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for (auto &pstr : aposBuffers_)
    {
        delete pstr;
    }
    delete pCurBuffer_;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIGZipWriteHandleMT::Close()

{
    if (!poBaseHandle_)
        return 0;

    int nRet = 0;

    if (!pCurBuffer_)
        pCurBuffer_ = new std::string();

    {
        auto psJob = GetJobObject();
        psJob->bFinish_ = true;
        psJob->pParent_ = this;
        psJob->pBuffer_ = pCurBuffer_;
        pCurBuffer_ = nullptr;
        psJob->nSeqNumber_ = nSeqNumberGenerated_;
        VSIGZipWriteHandleMT::DeflateCompress(psJob);
    }

    if (poPool_)
    {
        poPool_->WaitCompletion(0);
    }
    if (!ProcessCompletedJobs())
    {
        nRet = -1;
    }
    else
    {
        CPLAssert(apoFinishedJobs_.empty());
        if (nDeflateType_ == CPL_DEFLATE_TYPE_GZIP)
        {
            if (poPool_)
            {
                poPool_->WaitCompletion(0);
            }
            ProcessCompletedJobs();
        }
        CPLAssert(apoCRCFinishedJobs_.empty());
    }

    if (nDeflateType_ == CPL_DEFLATE_TYPE_GZIP)
    {
        const GUInt32 anTrailer[2] = {
            CPL_LSBWORD32(static_cast<GUInt32>(nCRC_)),
            CPL_LSBWORD32(static_cast<GUInt32>(nCurOffset_))};

        if (poBaseHandle_->Write(anTrailer, 1, 8) < 8)
        {
            nRet = -1;
        }
    }

    if (bAutoCloseBaseHandle_)
    {
        int nRetClose = poBaseHandle_->Close();
        if (nRet == 0)
            nRet = nRetClose;

        delete poBaseHandle_;
    }
    poBaseHandle_ = nullptr;

    return nRet;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIGZipWriteHandleMT::Read(void * /* pBuffer */, size_t /* nSize */,
                                  size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL is not supported on GZip write streams");
    return 0;
}

/************************************************************************/
/*                        DeflateCompress()                             */
/************************************************************************/

void VSIGZipWriteHandleMT::DeflateCompress(void *inData)
{
    Job *psJob = static_cast<Job *>(inData);

    CPLAssert(psJob->pBuffer_);

    z_stream sStream;
    memset(&sStream, 0, sizeof(sStream));
    sStream.zalloc = nullptr;
    sStream.zfree = nullptr;
    sStream.opaque = nullptr;

    sStream.avail_in = static_cast<uInt>(psJob->pBuffer_->size());
    sStream.next_in = reinterpret_cast<Bytef *>(&(*psJob->pBuffer_)[0]);

    int ret = deflateInit2(
        &sStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
        (psJob->pParent_->nDeflateType_ == CPL_DEFLATE_TYPE_ZLIB) ? MAX_WBITS
                                                                  : -MAX_WBITS,
        8, Z_DEFAULT_STRATEGY);
    CPLAssertAlwaysEval(ret == Z_OK);

    size_t nRealSize = 0;

    while (sStream.avail_in > 0)
    {
        psJob->sCompressedData_.resize(nRealSize + Z_BUFSIZE);
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);
        sStream.next_out =
            reinterpret_cast<Bytef *>(&psJob->sCompressedData_[0]) + nRealSize;

        const int zlibRet = deflate(&sStream, Z_NO_FLUSH);
        CPLAssertAlwaysEval(zlibRet == Z_OK);

        nRealSize += static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;
    }

    psJob->sCompressedData_.resize(nRealSize + Z_BUFSIZE);
    sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);
    sStream.next_out =
        reinterpret_cast<Bytef *>(&psJob->sCompressedData_[0]) + nRealSize;

    if (psJob->bFinish_)
    {
        const int zlibRet = deflate(&sStream, Z_FINISH);
        CPLAssertAlwaysEval(zlibRet == Z_STREAM_END);
    }
    else
    {
        // Do a Z_SYNC_FLUSH and Z_FULL_FLUSH, so as to have two markers when
        // independent as pigz 2.3.4 or later. The following 9 byte sequence
        // will be found: 0x00 0x00 0xff 0xff 0x00 0x00 0x00 0xff 0xff
        // Z_FULL_FLUSH only is sufficient, but it is not obvious if a
        // 0x00 0x00 0xff 0xff marker in the codestream is just a SYNC_FLUSH (
        // without dictionary reset) or a FULL_FLUSH (with dictionary reset)
        {
            const int zlibRet = deflate(&sStream, Z_SYNC_FLUSH);
            CPLAssertAlwaysEval(zlibRet == Z_OK);
        }

        {
            const int zlibRet = deflate(&sStream, Z_FULL_FLUSH);
            CPLAssertAlwaysEval(zlibRet == Z_OK);
        }
    }

    nRealSize += static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;
    psJob->sCompressedData_.resize(nRealSize);

    deflateEnd(&sStream);

    {
        std::lock_guard<std::mutex> oLock(psJob->pParent_->sMutex_);
        psJob->pParent_->apoFinishedJobs_.push_back(psJob);
    }
}

/************************************************************************/
/*                          CRCCompute()                                */
/************************************************************************/

void VSIGZipWriteHandleMT::CRCCompute(void *inData)
{
    Job *psJob = static_cast<Job *>(inData);
    psJob->bInCRCComputation_ = true;
    psJob->nCRC_ =
        crc32(0U, reinterpret_cast<const Bytef *>(psJob->pBuffer_->data()),
              static_cast<uInt>(psJob->pBuffer_->size()));

    {
        std::lock_guard<std::mutex> oLock(psJob->pParent_->sMutex_);
        psJob->pParent_->apoCRCFinishedJobs_.push_back(psJob);
    }
}

/************************************************************************/
/*                                DumpState()                           */
/************************************************************************/

#ifdef DEBUG_VERBOSE
void VSIGZipWriteHandleMT::DumpState()
{
    fprintf(stderr, "Finished jobs (expected = %d):\n",  // ok
            nSeqNumberExpected_);
    for (const auto *psJob : apoFinishedJobs_)
    {
        fprintf(stderr, "seq number=%d, bInCRCComputation = %d\n",  // ok
                psJob->nSeqNumber_, psJob->bInCRCComputation_ ? 1 : 0);
    }
    fprintf(stderr, "Finished CRC jobs (expected = %d):\n",  // ok
            nSeqNumberExpectedCRC_);
    for (const auto *psJob : apoFinishedJobs_)
    {
        fprintf(stderr, "seq number=%d\n",  // ok
                psJob->nSeqNumber_);
    }
    fprintf(stderr, "apoFreeJobs_.size() = %d\n",  // ok
            static_cast<int>(apoFreeJobs_.size()));
    fprintf(stderr, "aposBuffers_.size() = %d\n",  // ok
            static_cast<int>(aposBuffers_.size()));
}
#endif

/************************************************************************/
/*                         ProcessCompletedJobs()                       */
/************************************************************************/

bool VSIGZipWriteHandleMT::ProcessCompletedJobs()
{
    std::lock_guard<std::mutex> oLock(sMutex_);
    bool do_it_again = true;
    while (do_it_again)
    {
        do_it_again = false;
        if (nDeflateType_ == CPL_DEFLATE_TYPE_GZIP)
        {
            for (auto iter = apoFinishedJobs_.begin();
                 iter != apoFinishedJobs_.end(); ++iter)
            {
                auto psJob = *iter;

                if (!psJob->bInCRCComputation_)
                {
                    psJob->bInCRCComputation_ = true;
                    sMutex_.unlock();
                    if (poPool_)
                    {
                        poPool_->SubmitJob(VSIGZipWriteHandleMT::CRCCompute,
                                           psJob);
                    }
                    else
                    {
                        CRCCompute(psJob);
                    }
                    sMutex_.lock();
                }
            }
        }

        for (auto iter = apoFinishedJobs_.begin();
             iter != apoFinishedJobs_.end(); ++iter)
        {
            auto psJob = *iter;
            if (psJob->nSeqNumber_ == nSeqNumberExpected_)
            {
                apoFinishedJobs_.erase(iter);

                sMutex_.unlock();

                const size_t nToWrite = psJob->sCompressedData_.size();
                if (panSOZIPIndex_ && nSeqNumberExpected_ != 0 &&
                    !psJob->pBuffer_->empty())
                {
                    uint64_t nOffset = poBaseHandle_->Tell() - nStartOffset_;
                    if (nSOZIPIndexEltSize_ == 8)
                    {
                        CPL_LSBPTR64(&nOffset);
                        std::copy(reinterpret_cast<const uint8_t *>(&nOffset),
                                  reinterpret_cast<const uint8_t *>(&nOffset) +
                                      sizeof(nOffset),
                                  std::back_inserter(*panSOZIPIndex_));
                    }
                    else
                    {
                        if (nOffset > std::numeric_limits<uint32_t>::max())
                        {
                            // shouldn't happen normally...
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Too big offset for SOZIP_OFFSET_SIZE = 4");
                            panSOZIPIndex_->clear();
                            panSOZIPIndex_ = nullptr;
                        }
                        else
                        {
                            uint32_t nOffset32 = static_cast<uint32_t>(nOffset);
                            CPL_LSBPTR32(&nOffset32);
                            std::copy(
                                reinterpret_cast<const uint8_t *>(&nOffset32),
                                reinterpret_cast<const uint8_t *>(&nOffset32) +
                                    sizeof(nOffset32),
                                std::back_inserter(*panSOZIPIndex_));
                        }
                    }
                }
                bool bError =
                    poBaseHandle_->Write(psJob->sCompressedData_.data(), 1,
                                         nToWrite) < nToWrite;
                sMutex_.lock();
                nSeqNumberExpected_++;

                if (nDeflateType_ != CPL_DEFLATE_TYPE_GZIP)
                {
                    aposBuffers_.push_back(psJob->pBuffer_);
                    psJob->pBuffer_ = nullptr;

                    apoFreeJobs_.push_back(psJob);
                }

                if (bError)
                {
                    return false;
                }

                do_it_again = true;
                break;
            }
        }

        if (nDeflateType_ == CPL_DEFLATE_TYPE_GZIP)
        {
            for (auto iter = apoCRCFinishedJobs_.begin();
                 iter != apoCRCFinishedJobs_.end(); ++iter)
            {
                auto psJob = *iter;
                if (psJob->nSeqNumber_ == nSeqNumberExpectedCRC_)
                {
                    apoCRCFinishedJobs_.erase(iter);

                    nCRC_ = crc32_combine(
                        nCRC_, psJob->nCRC_,
                        static_cast<uLong>(psJob->pBuffer_->size()));

                    nSeqNumberExpectedCRC_++;

                    aposBuffers_.push_back(psJob->pBuffer_);
                    psJob->pBuffer_ = nullptr;

                    apoFreeJobs_.push_back(psJob);
                    do_it_again = true;
                    break;
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                           GetJobObject()                             */
/************************************************************************/

VSIGZipWriteHandleMT::Job *VSIGZipWriteHandleMT::GetJobObject()
{
    {
        std::lock_guard<std::mutex> oLock(sMutex_);
        if (!apoFreeJobs_.empty())
        {
            auto job = apoFreeJobs_.back();
            apoFreeJobs_.pop_back();
            job->sCompressedData_.clear();
            job->bInCRCComputation_ = false;
            return job;
        }
    }
    return new Job();
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIGZipWriteHandleMT::Write(const void *const pBuffer,
                                   size_t const nSize, size_t const nMemb)

{
    if (bHasErrored_)
        return 0;

    const char *pszBuffer = static_cast<const char *>(pBuffer);
    size_t nBytesToWrite = nSize * nMemb;
    while (nBytesToWrite > 0)
    {
        if (pCurBuffer_ == nullptr)
        {
            while (true)
            {
                // We store in a local variable instead of pCurBuffer_ directly
                // to avoid Coverity Scan to be confused by the fact that we
                // have used above pCurBuffer_ outside of the mutex. But what
                // is protected by the mutex is aposBuffers_, not pCurBuffer_.
                std::string *l_pCurBuffer = nullptr;
                {
                    std::lock_guard<std::mutex> oLock(sMutex_);
                    if (!aposBuffers_.empty())
                    {
                        l_pCurBuffer = aposBuffers_.back();
                        aposBuffers_.pop_back();
                    }
                }
                pCurBuffer_ = l_pCurBuffer;
                if (pCurBuffer_)
                    break;

                if (poPool_)
                {
                    poPool_->WaitEvent();
                }
                if (!ProcessCompletedJobs())
                {
                    bHasErrored_ = true;
                    return 0;
                }
            }
            pCurBuffer_->clear();
        }
        size_t nConsumed =
            std::min(nBytesToWrite, nChunkSize_ - pCurBuffer_->size());
        pCurBuffer_->append(pszBuffer, nConsumed);
        nCurOffset_ += nConsumed;
        pszBuffer += nConsumed;
        nBytesToWrite -= nConsumed;
        if (pCurBuffer_->size() == nChunkSize_)
        {
            if (poPool_ == nullptr)
            {
                poPool_.reset(new CPLWorkerThreadPool());
                if (!poPool_->Setup(nThreads_, nullptr, nullptr, false))
                {
                    bHasErrored_ = true;
                    poPool_.reset();
                    return 0;
                }
            }

            auto psJob = GetJobObject();
            psJob->pParent_ = this;
            psJob->pBuffer_ = pCurBuffer_;
            psJob->nSeqNumber_ = nSeqNumberGenerated_;
            nSeqNumberGenerated_++;
            pCurBuffer_ = nullptr;
            poPool_->SubmitJob(VSIGZipWriteHandleMT::DeflateCompress, psJob);
        }
    }

    return nMemb;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIGZipWriteHandleMT::Flush()

{
    // we *could* do something for this but for now we choose not to.

    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIGZipWriteHandleMT::Eof()

{
    return 1;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIGZipWriteHandleMT::Seek(vsi_l_offset nOffset, int nWhence)

{
    if (nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR))
        return 0;
    else if (nWhence == SEEK_SET && nOffset == nCurOffset_)
        return 0;
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seeking on writable compressed data streams not supported.");

        return -1;
    }
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIGZipWriteHandleMT::Tell()

{
    return nCurOffset_;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipWriteHandle                             */
/* ==================================================================== */
/************************************************************************/

class VSIGZipWriteHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGZipWriteHandle)

    VSIVirtualHandle *m_poBaseHandle = nullptr;
    z_stream sStream;
    Byte *pabyInBuf = nullptr;
    Byte *pabyOutBuf = nullptr;
    bool bCompressActive = false;
    vsi_l_offset nCurOffset = 0;
    uLong nCRC = 0;
    int nDeflateType = CPL_DEFLATE_TYPE_GZIP;
    bool bAutoCloseBaseHandle = false;

  public:
    VSIGZipWriteHandle(VSIVirtualHandle *poBaseHandle, int nDeflateType,
                       bool bAutoCloseBaseHandleIn);

    ~VSIGZipWriteHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

/************************************************************************/
/*                         VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::VSIGZipWriteHandle(VSIVirtualHandle *poBaseHandle,
                                       int nDeflateTypeIn,
                                       bool bAutoCloseBaseHandleIn)
    : m_poBaseHandle(poBaseHandle), sStream(),
      pabyInBuf(static_cast<Byte *>(CPLMalloc(Z_BUFSIZE))),
      pabyOutBuf(static_cast<Byte *>(CPLMalloc(Z_BUFSIZE))),
      nCRC(crc32(0L, nullptr, 0)), nDeflateType(nDeflateTypeIn),
      bAutoCloseBaseHandle(bAutoCloseBaseHandleIn)
{
    sStream.zalloc = nullptr;
    sStream.zfree = nullptr;
    sStream.opaque = nullptr;
    sStream.next_in = nullptr;
    sStream.next_out = nullptr;
    sStream.avail_in = sStream.avail_out = 0;

    sStream.next_in = pabyInBuf;

    if (deflateInit2(&sStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     (nDeflateType == CPL_DEFLATE_TYPE_ZLIB) ? MAX_WBITS
                                                             : -MAX_WBITS,
                     8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        bCompressActive = false;
    }
    else
    {
        if (nDeflateType == CPL_DEFLATE_TYPE_GZIP)
        {
            char header[11] = {};

            // Write a very simple .gz header:
            snprintf(header, sizeof(header), "%c%c%c%c%c%c%c%c%c%c",
                     gz_magic[0], gz_magic[1], Z_DEFLATED, 0 /*flags*/, 0, 0, 0,
                     0 /*time*/, 0 /*xflags*/, 0x03);
            m_poBaseHandle->Write(header, 1, 10);
        }

        bCompressActive = true;
    }
}

/************************************************************************/
/*                       VSICreateGZipWritable()                        */
/************************************************************************/

VSIVirtualHandle *VSICreateGZipWritable(VSIVirtualHandle *poBaseHandle,
                                        int nDeflateTypeIn,
                                        int bAutoCloseBaseHandle)
{
    return VSICreateGZipWritable(poBaseHandle, nDeflateTypeIn,
                                 CPL_TO_BOOL(bAutoCloseBaseHandle), 0, 0, 0,
                                 nullptr);
}

VSIVirtualHandle *VSICreateGZipWritable(VSIVirtualHandle *poBaseHandle,
                                        int nDeflateTypeIn,
                                        bool bAutoCloseBaseHandle, int nThreads,
                                        size_t nChunkSize,
                                        size_t nSOZIPIndexEltSize,
                                        std::vector<uint8_t> *panSOZIPIndex)
{
    const char *pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if (pszThreads || nThreads > 0 || nChunkSize > 0)
    {
        if (nThreads == 0)
        {
            if (!pszThreads || EQUAL(pszThreads, "ALL_CPUS"))
                nThreads = CPLGetNumCPUs();
            else
                nThreads = atoi(pszThreads);
            nThreads = std::max(1, std::min(128, nThreads));
        }
        if (nThreads > 1 || nChunkSize > 0)
        {
            // coverity[tainted_data]
            return new VSIGZipWriteHandleMT(
                poBaseHandle, nDeflateTypeIn, bAutoCloseBaseHandle, nThreads,
                nChunkSize, nSOZIPIndexEltSize, panSOZIPIndex);
        }
    }
    return new VSIGZipWriteHandle(poBaseHandle, nDeflateTypeIn,
                                  bAutoCloseBaseHandle);
}

/************************************************************************/
/*                        ~VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::~VSIGZipWriteHandle()

{
    if (bCompressActive)
        VSIGZipWriteHandle::Close();

    CPLFree(pabyInBuf);
    CPLFree(pabyOutBuf);
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIGZipWriteHandle::Close()

{
    int nRet = 0;
    if (bCompressActive)
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);

        const int zlibRet = deflate(&sStream, Z_FINISH);
        CPLAssertAlwaysEval(zlibRet == Z_STREAM_END);

        const size_t nOutBytes =
            static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;

        deflateEnd(&sStream);

        if (m_poBaseHandle->Write(pabyOutBuf, 1, nOutBytes) < nOutBytes)
        {
            nRet = -1;
        }

        if (nRet == 0 && nDeflateType == CPL_DEFLATE_TYPE_GZIP)
        {
            const GUInt32 anTrailer[2] = {
                CPL_LSBWORD32(static_cast<GUInt32>(nCRC)),
                CPL_LSBWORD32(static_cast<GUInt32>(nCurOffset))};

            if (m_poBaseHandle->Write(anTrailer, 1, 8) < 8)
            {
                nRet = -1;
            }
        }

        if (bAutoCloseBaseHandle)
        {
            if (nRet == 0)
                nRet = m_poBaseHandle->Close();

            delete m_poBaseHandle;
        }

        bCompressActive = false;
    }

    return nRet;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Read(void * /* pBuffer */, size_t /* nSize */,
                                size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL is not supported on GZip write streams");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Write(const void *const pBuffer, size_t const nSize,
                                 size_t const nMemb)

{
    size_t nBytesToWrite = nSize * nMemb;

    {
        size_t nOffset = 0;
        while (nOffset < nBytesToWrite)
        {
            uInt nChunk = static_cast<uInt>(std::min(
                static_cast<size_t>(UINT_MAX), nBytesToWrite - nOffset));
            nCRC =
                crc32(nCRC, reinterpret_cast<const Bytef *>(pBuffer) + nOffset,
                      nChunk);
            nOffset += nChunk;
        }
    }

    if (!bCompressActive)
        return 0;

    size_t nNextByte = 0;
    while (nNextByte < nBytesToWrite)
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);

        if (sStream.avail_in > 0)
            memmove(pabyInBuf, sStream.next_in, sStream.avail_in);

        const uInt nNewBytesToWrite = static_cast<uInt>(
            std::min(static_cast<size_t>(Z_BUFSIZE - sStream.avail_in),
                     nBytesToWrite - nNextByte));
        memcpy(pabyInBuf + sStream.avail_in,
               reinterpret_cast<const Byte *>(pBuffer) + nNextByte,
               nNewBytesToWrite);

        sStream.next_in = pabyInBuf;
        sStream.avail_in += nNewBytesToWrite;

        const int zlibRet = deflate(&sStream, Z_NO_FLUSH);
        CPLAssertAlwaysEval(zlibRet == Z_OK);

        const size_t nOutBytes =
            static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;

        if (nOutBytes > 0)
        {
            if (m_poBaseHandle->Write(pabyOutBuf, 1, nOutBytes) < nOutBytes)
                return 0;
        }

        nNextByte += nNewBytesToWrite;
        nCurOffset += nNewBytesToWrite;
    }

    return nMemb;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIGZipWriteHandle::Flush()

{
    // we *could* do something for this but for now we choose not to.

    return 0;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIGZipWriteHandle::Eof()

{
    return 1;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int VSIGZipWriteHandle::Seek(vsi_l_offset nOffset, int nWhence)

{
    if (nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR))
        return 0;
    else if (nWhence == SEEK_SET && nOffset == nCurOffset)
        return 0;
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seeking on writable compressed data streams not supported.");

        return -1;
    }
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

vsi_l_offset VSIGZipWriteHandle::Tell()

{
    return nCurOffset;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipFilesystemHandler                       */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                  ~VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::~VSIGZipFilesystemHandler()
{
    if (poHandleLastGZipFile)
    {
        poHandleLastGZipFile->UnsetCanSaveInfo();
        delete poHandleLastGZipFile;
    }

    if (hMutex != nullptr)
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
}

/************************************************************************/
/*                            SaveInfo()                                */
/************************************************************************/

void VSIGZipFilesystemHandler::SaveInfo(VSIGZipHandle *poHandle)
{
    CPLMutexHolder oHolder(&hMutex);
    SaveInfo_unlocked(poHandle);
}

void VSIGZipFilesystemHandler::SaveInfo_unlocked(VSIGZipHandle *poHandle)
{
    if (m_bInSaveInfo)
        return;
    m_bInSaveInfo = true;

    CPLAssert(poHandle != poHandleLastGZipFile);
    CPLAssert(poHandle->GetBaseFileName() != nullptr);

    if (poHandleLastGZipFile == nullptr ||
        strcmp(poHandleLastGZipFile->GetBaseFileName(),
               poHandle->GetBaseFileName()) != 0 ||
        poHandle->GetLastReadOffset() >
            poHandleLastGZipFile->GetLastReadOffset())
    {
        VSIGZipHandle *poTmp = poHandleLastGZipFile;
        poHandleLastGZipFile = nullptr;
        if (poTmp)
        {
            poTmp->UnsetCanSaveInfo();
            delete poTmp;
        }
        CPLAssert(poHandleLastGZipFile == nullptr);
        poHandleLastGZipFile = poHandle->Duplicate();
        if (poHandleLastGZipFile)
            poHandleLastGZipFile->CloseBaseHandle();
    }
    m_bInSaveInfo = false;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle *
VSIGZipFilesystemHandler::Open(const char *pszFilename, const char *pszAccess,
                               bool /* bSetError */,
                               CSLConstList /* papszOptions */)
{
    if (!STARTS_WITH_CI(pszFilename, "/vsigzip/"))
        return nullptr;

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(pszFilename + strlen("/vsigzip/"));

    /* -------------------------------------------------------------------- */
    /*      Is this an attempt to write a new file without update (w+)      */
    /*      access?  If so, create a writable handle for the underlying     */
    /*      filename.                                                       */
    /* -------------------------------------------------------------------- */
    if (strchr(pszAccess, 'w') != nullptr)
    {
        if (strchr(pszAccess, '+') != nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Write+update (w+) not supported for /vsigzip, "
                     "only read-only or write-only.");
            return nullptr;
        }

        VSIVirtualHandle *poVirtualHandle =
            poFSHandler->Open(pszFilename + strlen("/vsigzip/"), "wb");

        if (poVirtualHandle == nullptr)
            return nullptr;

        return VSICreateGZipWritable(poVirtualHandle,
                                     strchr(pszAccess, 'z') != nullptr, TRUE);
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise we are in the read access case.                       */
    /* -------------------------------------------------------------------- */

    VSIGZipHandle *poGZIPHandle = OpenGZipReadOnly(pszFilename, pszAccess);
    if (poGZIPHandle)
        // Wrap the VSIGZipHandle inside a buffered reader that will
        // improve dramatically performance when doing small backward
        // seeks.
        return VSICreateBufferedReaderHandle(poGZIPHandle);

    return nullptr;
}

/************************************************************************/
/*                      SupportsSequentialWrite()                       */
/************************************************************************/

bool VSIGZipFilesystemHandler::SupportsSequentialWrite(const char *pszPath,
                                                       bool bAllowLocalTempFile)
{
    if (!STARTS_WITH_CI(pszPath, "/vsigzip/"))
        return false;
    const char *pszBaseFileName = pszPath + strlen("/vsigzip/");
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(pszBaseFileName);
    return poFSHandler->SupportsSequentialWrite(pszPath, bAllowLocalTempFile);
}

/************************************************************************/
/*                          OpenGZipReadOnly()                          */
/************************************************************************/

VSIGZipHandle *
VSIGZipFilesystemHandler::OpenGZipReadOnly(const char *pszFilename,
                                           const char *pszAccess)
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler(pszFilename + strlen("/vsigzip/"));

    CPLMutexHolder oHolder(&hMutex);

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Disable caching in fuzzing mode as the /vsigzip/ file is likely to
    // change very often
    // TODO: filename-based logic isn't enough. We should probably check
    // timestamp and/or file size.
    if (poHandleLastGZipFile != nullptr &&
        strcmp(pszFilename + strlen("/vsigzip/"),
               poHandleLastGZipFile->GetBaseFileName()) == 0 &&
        EQUAL(pszAccess, "rb"))
    {
        VSIGZipHandle *poHandle = poHandleLastGZipFile->Duplicate();
        if (poHandle)
            return poHandle;
    }
#else
    CPL_IGNORE_RET_VAL(pszAccess);
#endif

    VSIVirtualHandle *poVirtualHandle =
        poFSHandler->Open(pszFilename + strlen("/vsigzip/"), "rb");

    if (poVirtualHandle == nullptr)
        return nullptr;

    unsigned char signature[2] = {'\0', '\0'};
    if (poVirtualHandle->Read(signature, 1, 2) != 2 ||
        signature[0] != gz_magic[0] || signature[1] != gz_magic[1])
    {
        poVirtualHandle->Close();
        delete poVirtualHandle;
        return nullptr;
    }

    if (poHandleLastGZipFile)
    {
        poHandleLastGZipFile->UnsetCanSaveInfo();
        delete poHandleLastGZipFile;
        poHandleLastGZipFile = nullptr;
    }

    VSIGZipHandle *poHandle =
        new VSIGZipHandle(poVirtualHandle, pszFilename + strlen("/vsigzip/"));
    if (!(poHandle->IsInitOK()))
    {
        delete poHandle;
        return nullptr;
    }
    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Stat(const char *pszFilename,
                                   VSIStatBufL *pStatBuf, int nFlags)
{
    if (!STARTS_WITH_CI(pszFilename, "/vsigzip/"))
        return -1;

    CPLMutexHolder oHolder(&hMutex);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if (poHandleLastGZipFile != nullptr &&
        strcmp(pszFilename + strlen("/vsigzip/"),
               poHandleLastGZipFile->GetBaseFileName()) == 0)
    {
        if (poHandleLastGZipFile->GetUncompressedSize() != 0)
        {
            pStatBuf->st_mode = S_IFREG;
            pStatBuf->st_size = poHandleLastGZipFile->GetUncompressedSize();
            return 0;
        }
    }

    // Begin by doing a stat on the real file.
    int ret = VSIStatExL(pszFilename + strlen("/vsigzip/"), pStatBuf, nFlags);

    if (ret == 0 && (nFlags & VSI_STAT_SIZE_FLAG))
    {
        CPLString osCacheFilename(pszFilename + strlen("/vsigzip/"));
        osCacheFilename += ".properties";

        // Can we save a bit of seeking by using a .properties file?
        VSILFILE *fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "rb");
        if (fpCacheLength)
        {
            const char *pszLine;
            GUIntBig nCompressedSize = 0;
            GUIntBig nUncompressedSize = 0;
            while ((pszLine = CPLReadLineL(fpCacheLength)) != nullptr)
            {
                if (STARTS_WITH_CI(pszLine, "compressed_size="))
                {
                    const char *pszBuffer =
                        pszLine + strlen("compressed_size=");
                    nCompressedSize = CPLScanUIntBig(
                        pszBuffer, static_cast<int>(strlen(pszBuffer)));
                }
                else if (STARTS_WITH_CI(pszLine, "uncompressed_size="))
                {
                    const char *pszBuffer =
                        pszLine + strlen("uncompressed_size=");
                    nUncompressedSize = CPLScanUIntBig(
                        pszBuffer, static_cast<int>(strlen(pszBuffer)));
                }
            }

            CPL_IGNORE_RET_VAL(VSIFCloseL(fpCacheLength));

            if (nCompressedSize == static_cast<GUIntBig>(pStatBuf->st_size))
            {
                // Patch with the uncompressed size.
                pStatBuf->st_size = nUncompressedSize;

                VSIGZipHandle *poHandle =
                    VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename,
                                                               "rb");
                if (poHandle)
                {
                    poHandle->SetUncompressedSize(nUncompressedSize);
                    SaveInfo_unlocked(poHandle);
                    delete poHandle;
                }

                return ret;
            }
        }

        // No, then seek at the end of the data (slow).
        VSIGZipHandle *poHandle =
            VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename, "rb");
        if (poHandle)
        {
            poHandle->Seek(0, SEEK_END);
            const GUIntBig uncompressed_size =
                static_cast<GUIntBig>(poHandle->Tell());
            poHandle->Seek(0, SEEK_SET);

            // Patch with the uncompressed size.
            pStatBuf->st_size = uncompressed_size;

            delete poHandle;
        }
        else
        {
            ret = -1;
        }
    }

    return ret;
}

/************************************************************************/
/*                               Unlink()                               */
/************************************************************************/

int VSIGZipFilesystemHandler::Unlink(const char * /* pszFilename */)
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIGZipFilesystemHandler::Rename(const char * /* oldpath */,
                                     const char * /* newpath */)
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Mkdir(const char * /* pszDirname */,
                                    long /* nMode */)
{
    return -1;
}

/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Rmdir(const char * /* pszDirname */)
{
    return -1;
}

/************************************************************************/
/*                             ReadDirEx()                                */
/************************************************************************/

char **VSIGZipFilesystemHandler::ReadDirEx(const char * /*pszDirname*/,
                                           int /* nMaxFiles */)
{
    return nullptr;
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIGZipFilesystemHandler::GetOptions()
{
    return "<Options>"
           "  <Option name='GDAL_NUM_THREADS' type='string' "
           "description='Number of threads for compression. Either a integer "
           "or ALL_CPUS'/>"
           "  <Option name='CPL_VSIL_DEFLATE_CHUNK_SIZE' type='string' "
           "description='Chunk of uncompressed data for parallelization. "
           "Use K(ilobytes) or M(egabytes) suffix' default='1M'/>"
           "</Options>";
}

//! @endcond
/************************************************************************/
/*                   VSIInstallGZipFileHandler()                        */
/************************************************************************/

/*!
 \brief Install GZip file system handler.

 A special file handler is installed that allows reading on-the-fly and
 writing in GZip (.gz) files.

 All portions of the file system underneath the base
 path "/vsigzip/" will be handled by this driver.

 \verbatim embed:rst
 See :ref:`/vsigzip/ documentation <vsigzip>`
 \endverbatim

 @since GDAL 1.6.0
 */

void VSIInstallGZipFileHandler()
{
    VSIFileManager::InstallHandler("/vsigzip/", new VSIGZipFilesystemHandler);
}

//! @cond Doxygen_Suppress

/************************************************************************/
/* ==================================================================== */
/*                         VSIZipEntryFileOffset                        */
/* ==================================================================== */
/************************************************************************/

class VSIZipEntryFileOffset final : public VSIArchiveEntryFileOffset
{
  public:
    unz_file_pos m_file_pos;

    explicit VSIZipEntryFileOffset(unz_file_pos file_pos) : m_file_pos()
    {
        m_file_pos.pos_in_zip_directory = file_pos.pos_in_zip_directory;
        m_file_pos.num_of_file = file_pos.num_of_file;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                             VSIZipReader                             */
/* ==================================================================== */
/************************************************************************/

class VSIZipReader final : public VSIArchiveReader
{
    CPL_DISALLOW_COPY_ASSIGN(VSIZipReader)

  private:
    unzFile unzF = nullptr;
    unz_file_pos file_pos;
    GUIntBig nNextFileSize = 0;
    CPLString osNextFileName{};
    GIntBig nModifiedTime = 0;

    bool SetInfo();

  public:
    explicit VSIZipReader(const char *pszZipFileName);
    ~VSIZipReader() override;

    int IsValid()
    {
        return unzF != nullptr;
    }

    unzFile GetUnzFileHandle()
    {
        return unzF;
    }

    int GotoFirstFile() override;
    int GotoNextFile() override;

    VSIArchiveEntryFileOffset *GetFileOffset() override
    {
        return new VSIZipEntryFileOffset(file_pos);
    }

    GUIntBig GetFileSize() override
    {
        return nNextFileSize;
    }

    CPLString GetFileName() override
    {
        return osNextFileName;
    }

    GIntBig GetModifiedTime() override
    {
        return nModifiedTime;
    }

    int GotoFileOffset(VSIArchiveEntryFileOffset *pOffset) override;
};

/************************************************************************/
/*                           VSIZipReader()                             */
/************************************************************************/

VSIZipReader::VSIZipReader(const char *pszZipFileName)
    : unzF(cpl_unzOpen(pszZipFileName)), file_pos()
{
    file_pos.pos_in_zip_directory = 0;
    file_pos.num_of_file = 0;
}

/************************************************************************/
/*                          ~VSIZipReader()                             */
/************************************************************************/

VSIZipReader::~VSIZipReader()
{
    if (unzF)
        cpl_unzClose(unzF);
}

/************************************************************************/
/*                              SetInfo()                               */
/************************************************************************/

bool VSIZipReader::SetInfo()
{
    char fileName[8193] = {};
    unz_file_info file_info;
    if (UNZ_OK != cpl_unzGetCurrentFileInfo(unzF, &file_info, fileName,
                                            sizeof(fileName) - 1, nullptr, 0,
                                            nullptr, 0))
    {
        CPLError(CE_Failure, CPLE_FileIO, "cpl_unzGetCurrentFileInfo failed");
        cpl_unzGetFilePos(unzF, &file_pos);
        return false;
    }
    fileName[sizeof(fileName) - 1] = '\0';
    osNextFileName = fileName;
    nNextFileSize = file_info.uncompressed_size;
    struct tm brokendowntime;
    brokendowntime.tm_sec = file_info.tmu_date.tm_sec;
    brokendowntime.tm_min = file_info.tmu_date.tm_min;
    brokendowntime.tm_hour = file_info.tmu_date.tm_hour;
    brokendowntime.tm_mday = file_info.tmu_date.tm_mday;
    brokendowntime.tm_mon = file_info.tmu_date.tm_mon;
    // The minizip conventions differs from the Unix one.
    brokendowntime.tm_year = file_info.tmu_date.tm_year - 1900;
    nModifiedTime = CPLYMDHMSToUnixTime(&brokendowntime);

    cpl_unzGetFilePos(unzF, &file_pos);
    return true;
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSIZipReader::GotoNextFile()
{
    if (cpl_unzGoToNextFile(unzF) != UNZ_OK)
        return FALSE;

    if (!SetInfo())
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSIZipReader::GotoFirstFile()
{
    if (cpl_unzGoToFirstFile(unzF) != UNZ_OK)
        return FALSE;

    if (!SetInfo())
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSIZipReader::GotoFileOffset(VSIArchiveEntryFileOffset *pOffset)
{
    VSIZipEntryFileOffset *pZipEntryOffset =
        reinterpret_cast<VSIZipEntryFileOffset *>(pOffset);
    if (cpl_unzGoToFilePos(unzF, &(pZipEntryOffset->m_file_pos)) != UNZ_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GotoFileOffset failed");
        return FALSE;
    }

    if (!SetInfo())
        return FALSE;

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIZipFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

class VSIZipWriteHandle;

class VSIZipFilesystemHandler final : public VSIArchiveFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIZipFilesystemHandler)

    std::map<CPLString, VSIZipWriteHandle *> oMapZipWriteHandles{};
    VSIVirtualHandle *OpenForWrite_unlocked(const char *pszFilename,
                                            const char *pszAccess);

    struct VSIFileInZipInfo
    {
        VSIVirtualHandleUniquePtr poVirtualHandle{};
        std::map<std::string, std::string> oMapProperties{};
        int nCompressionMethod = 0;
        uint64_t nUncompressedSize = 0;
        uint64_t nCompressedSize = 0;
        uint64_t nStartDataStream = 0;
        uLong nCRC = 0;
        bool bSOZipIndexFound = false;
        bool bSOZipIndexValid = false;
        uint32_t nSOZIPVersion = 0;
        uint32_t nSOZIPToSkip = 0;
        uint32_t nSOZIPChunkSize = 0;
        uint32_t nSOZIPOffsetSize = 0;
        uint64_t nSOZIPStartData = 0;
    };

    bool GetFileInfo(const char *pszFilename, VSIFileInZipInfo &info);

  public:
    VSIZipFilesystemHandler() = default;
    ~VSIZipFilesystemHandler() override;

    const char *GetPrefix() override
    {
        return "/vsizip";
    }

    std::vector<CPLString> GetExtensions() override;
    VSIArchiveReader *CreateReader(const char *pszZipFileName) override;

    VSIVirtualHandle *Open(const char *pszFilename, const char *pszAccess,
                           bool bSetError,
                           CSLConstList /* papszOptions */) override;

    char **GetFileMetadata(const char *pszFilename, const char *pszDomain,
                           CSLConstList papszOptions) override;

    VSIVirtualHandle *OpenForWrite(const char *pszFilename,
                                   const char *pszAccess);

    int CopyFile(const char *pszSource, const char *pszTarget,
                 VSILFILE *fpSource, vsi_l_offset nSourceSize,
                 const char *const *papszOptions,
                 GDALProgressFunc pProgressFunc, void *pProgressData) override;

    int Mkdir(const char *pszDirname, long nMode) override;
    char **ReadDirEx(const char *pszDirname, int nMaxFiles) override;
    int Stat(const char *pszFilename, VSIStatBufL *pStatBuf,
             int nFlags) override;

    const char *GetOptions() override;

    void RemoveFromMap(VSIZipWriteHandle *poHandle);
};

/************************************************************************/
/* ==================================================================== */
/*                       VSIZipWriteHandle                              */
/* ==================================================================== */
/************************************************************************/

class VSIZipWriteHandle final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIZipWriteHandle)

    VSIZipFilesystemHandler *m_poFS = nullptr;
    void *m_hZIP = nullptr;
    VSIZipWriteHandle *poChildInWriting = nullptr;
    VSIZipWriteHandle *m_poParent = nullptr;
    bool bAutoDeleteParent = false;
    vsi_l_offset nCurOffset = 0;

  public:
    VSIZipWriteHandle(VSIZipFilesystemHandler *poFS, void *hZIP,
                      VSIZipWriteHandle *poParent);

    ~VSIZipWriteHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override;
    vsi_l_offset Tell() override;
    size_t Read(void *pBuffer, size_t nSize, size_t nMemb) override;
    size_t Write(const void *pBuffer, size_t nSize, size_t nMemb) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    void StartNewFile(VSIZipWriteHandle *poSubFile);
    void StopCurrentFile();

    void *GetHandle()
    {
        return m_hZIP;
    }

    VSIZipWriteHandle *GetChildInWriting()
    {
        return poChildInWriting;
    }

    void SetAutoDeleteParent()
    {
        bAutoDeleteParent = true;
    }
};

/************************************************************************/
/*                      ~VSIZipFilesystemHandler()                      */
/************************************************************************/

VSIZipFilesystemHandler::~VSIZipFilesystemHandler()
{
    for (std::map<CPLString, VSIZipWriteHandle *>::const_iterator iter =
             oMapZipWriteHandles.begin();
         iter != oMapZipWriteHandles.end(); ++iter)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s has not been closed",
                 iter->first.c_str());
    }
}

/************************************************************************/
/*                          GetExtensions()                             */
/************************************************************************/

std::vector<CPLString> VSIZipFilesystemHandler::GetExtensions()
{
    std::vector<CPLString> oList;
    oList.push_back(".zip");
    oList.push_back(".kmz");
    oList.push_back(".dwf");
    oList.push_back(".ods");
    oList.push_back(".xlsx");
    oList.push_back(".xlsm");

    // Add to zip FS handler extensions array additional extensions
    // listed in CPL_VSIL_ZIP_ALLOWED_EXTENSIONS config option.
    // The extensions are divided by commas.
    const char *pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", nullptr);
    if (pszAllowedExtensions)
    {
        char **papszExtensions =
            CSLTokenizeString2(pszAllowedExtensions, ", ", 0);
        for (int i = 0; papszExtensions[i] != nullptr; i++)
        {
            oList.push_back(papszExtensions[i]);
        }
        CSLDestroy(papszExtensions);
    }

    return oList;
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

VSIArchiveReader *
VSIZipFilesystemHandler::CreateReader(const char *pszZipFileName)
{
    VSIZipReader *poReader = new VSIZipReader(pszZipFileName);

    if (!poReader->IsValid())
    {
        delete poReader;
        return nullptr;
    }

    if (!poReader->GotoFirstFile())
    {
        delete poReader;
        return nullptr;
    }

    return poReader;
}

/************************************************************************/
/*                         VSISOZipHandle                               */
/************************************************************************/

class VSISOZipHandle final : public VSIVirtualHandle
{
    VSIVirtualHandle *poBaseHandle_;
    vsi_l_offset nPosCompressedStream_;
    uint64_t compressed_size_;
    uint64_t uncompressed_size_;
    vsi_l_offset indexPos_;
    uint32_t nToSkip_;
    uint32_t nChunkSize_;
    bool bEOF_ = false;
    vsi_l_offset nCurPos_ = 0;
    bool bOK_ = true;
#ifdef HAVE_LIBDEFLATE
    struct libdeflate_decompressor *pDecompressor_ = nullptr;
#else
    z_stream sStream_{};
#endif

    VSISOZipHandle(const VSISOZipHandle &) = delete;
    VSISOZipHandle &operator=(const VSISOZipHandle &) = delete;

  public:
    VSISOZipHandle(VSIVirtualHandle *poVirtualHandle,
                   vsi_l_offset nPosCompressedStream, uint64_t compressed_size,
                   uint64_t uncompressed_size, vsi_l_offset indexPos,
                   uint32_t nToSkip, uint32_t nChunkSize);
    ~VSISOZipHandle() override;

    virtual int Seek(vsi_l_offset nOffset, int nWhence) override;

    virtual vsi_l_offset Tell() override
    {
        return nCurPos_;
    }

    virtual size_t Read(void *pBuffer, size_t nSize, size_t nCount) override;

    virtual size_t Write(const void *, size_t, size_t) override
    {
        return 0;
    }

    virtual int Eof() override
    {
        return bEOF_;
    }

    virtual int Close() override;

    bool IsOK() const
    {
        return bOK_;
    }
};

/************************************************************************/
/*                         VSISOZipHandle()                             */
/************************************************************************/

VSISOZipHandle::VSISOZipHandle(VSIVirtualHandle *poVirtualHandle,
                               vsi_l_offset nPosCompressedStream,
                               uint64_t compressed_size,
                               uint64_t uncompressed_size,
                               vsi_l_offset indexPos, uint32_t nToSkip,
                               uint32_t nChunkSize)
    : poBaseHandle_(poVirtualHandle),
      nPosCompressedStream_(nPosCompressedStream),
      compressed_size_(compressed_size), uncompressed_size_(uncompressed_size),
      indexPos_(indexPos), nToSkip_(nToSkip), nChunkSize_(nChunkSize)
{
#ifdef HAVE_LIBDEFLATE
    pDecompressor_ = libdeflate_alloc_decompressor();
    if (!pDecompressor_)
        bOK_ = false;
#else
    memset(&sStream_, 0, sizeof(sStream_));
    int err = inflateInit2(&sStream_, -MAX_WBITS);
    if (err != Z_OK)
        bOK_ = false;
#endif
}

/************************************************************************/
/*                        ~VSISOZipHandle()                             */
/************************************************************************/

VSISOZipHandle::~VSISOZipHandle()
{
    VSISOZipHandle::Close();
    if (bOK_)
    {
#ifdef HAVE_LIBDEFLATE
        libdeflate_free_decompressor(pDecompressor_);
#else
        inflateEnd(&sStream_);
#endif
    }
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSISOZipHandle::Close()
{
    delete poBaseHandle_;
    poBaseHandle_ = nullptr;
    return 0;
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSISOZipHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    bEOF_ = false;
    if (nWhence == SEEK_SET)
        nCurPos_ = nOffset;
    else if (nWhence == SEEK_END)
        nCurPos_ = uncompressed_size_;
    else
        nCurPos_ += nOffset;
    return 0;
}

/************************************************************************/
/*                              Read()                                  */
/************************************************************************/

size_t VSISOZipHandle::Read(void *pBuffer, size_t nSize, size_t nCount)
{
    size_t nToRead = nSize * nCount;
    if (nCurPos_ >= uncompressed_size_ && nToRead > 0)
    {
        bEOF_ = true;
        return 0;
    }

    if (nSize != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported nSize");
        return 0;
    }
    if ((nCurPos_ % nChunkSize_) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "nCurPos is not a multiple of nChunkSize");
        return 0;
    }
    if (nCurPos_ + nToRead > uncompressed_size_)
    {
        nToRead = static_cast<size_t>(uncompressed_size_ - nCurPos_);
        nCount = nToRead;
    }
    else if ((nToRead % nChunkSize_) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "nToRead is not a multiple of nChunkSize");
        return 0;
    }

    const auto ReadOffsetInCompressedStream =
        [this](uint64_t nChunkIdx) -> uint64_t
    {
        if (nChunkIdx == 0)
            return 0;
        if (nChunkIdx == 1 + (uncompressed_size_ - 1) / nChunkSize_)
            return compressed_size_;
        constexpr size_t nOffsetSize = 8;
        if (poBaseHandle_->Seek(indexPos_ + 32 + nToSkip_ +
                                    (nChunkIdx - 1) * nOffsetSize,
                                SEEK_SET) != 0)
            return static_cast<uint64_t>(-1);

        uint64_t nOffset;
        if (poBaseHandle_->Read(&nOffset, sizeof(nOffset), 1) != 1)
            return static_cast<uint64_t>(-1);
        CPL_LSBPTR64(&nOffset);
        return nOffset;
    };

    size_t nOffsetInOutputBuffer = 0;
    while (true)
    {
        uint64_t nOffsetInCompressedStream =
            ReadOffsetInCompressedStream(nCurPos_ / nChunkSize_);
        if (nOffsetInCompressedStream == static_cast<uint64_t>(-1))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read nOffsetInCompressedStream");
            return 0;
        }
        uint64_t nNextOffsetInCompressedStream =
            ReadOffsetInCompressedStream(1 + nCurPos_ / nChunkSize_);
        if (nNextOffsetInCompressedStream == static_cast<uint64_t>(-1))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read nNextOffsetInCompressedStream");
            return 0;
        }

        if (nNextOffsetInCompressedStream <= nOffsetInCompressedStream ||
            nNextOffsetInCompressedStream - nOffsetInCompressedStream >
                13 + 2 * nChunkSize_ ||
            nNextOffsetInCompressedStream > compressed_size_)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Invalid values for nOffsetInCompressedStream (" CPL_FRMT_GUIB
                ") / "
                "nNextOffsetInCompressedStream(" CPL_FRMT_GUIB ")",
                static_cast<GUIntBig>(nOffsetInCompressedStream),
                static_cast<GUIntBig>(nNextOffsetInCompressedStream));
            return 0;
        }

        // CPLDebug("VSIZIP", "Seek to compressed data at offset "
        // CPL_FRMT_GUIB, static_cast<GUIntBig>(nPosCompressedStream_ +
        // nOffsetInCompressedStream));
        if (poBaseHandle_->Seek(
                nPosCompressedStream_ + nOffsetInCompressedStream, SEEK_SET) !=
            0)
            return 0;

        const int nCompressedToRead = static_cast<int>(
            nNextOffsetInCompressedStream - nOffsetInCompressedStream);
        // CPLDebug("VSIZIP", "nCompressedToRead = %d", nCompressedToRead);
        std::vector<GByte> abyCompressedData(nCompressedToRead);
        if (poBaseHandle_->Read(&abyCompressedData[0], nCompressedToRead, 1) !=
            1)
            return 0;

        size_t nToReadThisIter =
            std::min(nToRead, static_cast<size_t>(nChunkSize_));

        if (nCompressedToRead >= 5 &&
            abyCompressedData[nCompressedToRead - 5] == 0x00 &&
            memcmp(&abyCompressedData[nCompressedToRead - 4],
                   "\x00\x00\xFF\xFF", 4) == 0)
        {
            // Tag this flush block as the last one.
            abyCompressedData[nCompressedToRead - 5] = 0x01;
        }

#ifdef HAVE_LIBDEFLATE
        size_t nOut = 0;
        if (libdeflate_deflate_decompress(
                pDecompressor_, &abyCompressedData[0], nCompressedToRead,
                static_cast<Bytef *>(pBuffer) + nOffsetInOutputBuffer,
                nToReadThisIter, &nOut) != LIBDEFLATE_SUCCESS)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "libdeflate_deflate_decompress() failed at pos " CPL_FRMT_GUIB,
                static_cast<GUIntBig>(nCurPos_));
            return 0;
        }
        if (nOut != nToReadThisIter)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Only %u bytes decompressed at pos " CPL_FRMT_GUIB
                     " whereas %u where expected",
                     static_cast<unsigned>(nOut),
                     static_cast<GUIntBig>(nCurPos_),
                     static_cast<unsigned>(nToReadThisIter));
            return 0;
        }
#else
        sStream_.avail_in = nCompressedToRead;
        sStream_.next_in = &abyCompressedData[0];
        sStream_.avail_out = static_cast<int>(nToReadThisIter);
        sStream_.next_out =
            static_cast<Bytef *>(pBuffer) + nOffsetInOutputBuffer;

        int err = inflate(&sStream_, Z_FINISH);
        if ((err != Z_OK && err != Z_STREAM_END))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "inflate() failed at pos " CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(nCurPos_));
            inflateReset(&sStream_);
            return 0;
        }
        if (sStream_.avail_in != 0)
            CPLDebug("VSIZIP", "avail_in = %d", sStream_.avail_in);
        if (sStream_.avail_out != 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Only %u bytes decompressed at pos " CPL_FRMT_GUIB
                " whereas %u where expected",
                static_cast<unsigned>(nToReadThisIter - sStream_.avail_out),
                static_cast<GUIntBig>(nCurPos_),
                static_cast<unsigned>(nToReadThisIter));
            inflateReset(&sStream_);
            return 0;
        }
        inflateReset(&sStream_);
#endif
        nOffsetInOutputBuffer += nToReadThisIter;
        nCurPos_ += nToReadThisIter;
        nToRead -= nToReadThisIter;
        if (nToRead == 0)
            break;
    }

    return nCount;
}

/************************************************************************/
/*                          GetFileInfo()                               */
/************************************************************************/

bool VSIZipFilesystemHandler::GetFileInfo(const char *pszFilename,
                                          VSIFileInZipInfo &info)
{

    CPLString osZipInFileName;
    char *zipFilename = SplitFilename(pszFilename, osZipInFileName, TRUE);
    if (zipFilename == nullptr)
        return false;

    {
        CPLMutexHolder oHolder(&hMutex);
        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return false;
        }
    }

    VSIArchiveReader *poReader = OpenArchiveFile(zipFilename, osZipInFileName);
    if (poReader == nullptr)
    {
        CPLFree(zipFilename);
        return false;
    }

    VSIFilesystemHandler *poFSHandler = VSIFileManager::GetHandler(zipFilename);

    VSIVirtualHandle *poVirtualHandle = poFSHandler->Open(zipFilename, "rb");

    CPLFree(zipFilename);
    zipFilename = nullptr;

    if (poVirtualHandle == nullptr)
    {
        delete poReader;
        return false;
    }

    unzFile unzF =
        reinterpret_cast<VSIZipReader *>(poReader)->GetUnzFileHandle();

    if (cpl_unzOpenCurrentFile(unzF) != UNZ_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzOpenCurrentFile() failed");
        delete poReader;
        delete poVirtualHandle;
        return false;
    }

    info.nStartDataStream = cpl_unzGetCurrentFileZStreamPos(unzF);

    unz_file_info file_info;
    if (cpl_unzGetCurrentFileInfo(unzF, &file_info, nullptr, 0, nullptr, 0,
                                  nullptr, 0) != UNZ_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzGetCurrentFileInfo() failed");
        cpl_unzCloseCurrentFile(unzF);
        delete poReader;
        delete poVirtualHandle;
        return false;
    }

    if (file_info.size_file_extra)
    {
        std::vector<GByte> abyExtra(file_info.size_file_extra);
        poVirtualHandle->Seek(file_info.file_extra_abs_offset, SEEK_SET);
        if (poVirtualHandle->Read(&abyExtra[0], abyExtra.size(), 1) == 1)
        {
            size_t nPos = 0;
            while (nPos + 2 * sizeof(uint16_t) <= abyExtra.size())
            {
                uint16_t nId;
                memcpy(&nId, &abyExtra[nPos], sizeof(uint16_t));
                nPos += sizeof(uint16_t);
                CPL_LSBPTR16(&nId);
                uint16_t nSize;
                memcpy(&nSize, &abyExtra[nPos], sizeof(uint16_t));
                nPos += sizeof(uint16_t);
                CPL_LSBPTR16(&nSize);
                if (nId == 0x564b && nPos + nSize <= abyExtra.size())  // "KV"
                {
                    if (nSize >= strlen("KeyValuePairs") + 1 &&
                        memcmp(&abyExtra[nPos], "KeyValuePairs",
                               strlen("KeyValuePairs")) == 0)
                    {
                        int nPos2 = static_cast<int>(strlen("KeyValuePairs"));
                        int nKVPairs = abyExtra[nPos + nPos2];
                        nPos2++;
                        for (int iKV = 0; iKV < nKVPairs; ++iKV)
                        {
                            if (nPos2 + sizeof(uint16_t) > nSize)
                                break;
                            uint16_t nKeyLen;
                            memcpy(&nKeyLen, &abyExtra[nPos + nPos2],
                                   sizeof(uint16_t));
                            nPos2 += sizeof(uint16_t);
                            CPL_LSBPTR16(&nKeyLen);
                            if (nPos2 + nKeyLen > nSize)
                                break;
                            std::string osKey;
                            osKey.resize(nKeyLen);
                            memcpy(&osKey[0], &abyExtra[nPos + nPos2], nKeyLen);
                            nPos2 += nKeyLen;

                            if (nPos2 + sizeof(uint16_t) > nSize)
                                break;
                            uint16_t nValLen;
                            memcpy(&nValLen, &abyExtra[nPos + nPos2],
                                   sizeof(uint16_t));
                            nPos2 += sizeof(uint16_t);
                            CPL_LSBPTR16(&nValLen);
                            if (nPos2 + nValLen > nSize)
                                break;
                            std::string osVal;
                            osVal.resize(nValLen);
                            memcpy(&osVal[0], &abyExtra[nPos + nPos2], nValLen);
                            nPos2 += nValLen;

                            info.oMapProperties[osKey] = std::move(osVal);
                        }
                    }
                }
                nPos += nSize;
            }
        }
    }

    info.nCRC = file_info.crc;
    info.nCompressionMethod = static_cast<int>(file_info.compression_method);
    info.nUncompressedSize = static_cast<uint64_t>(file_info.uncompressed_size);
    info.nCompressedSize = static_cast<uint64_t>(file_info.compressed_size);

    // Try to locate .sozip.idx file
    uLong64 local_header_pos;
    cpl_unzGetLocalHeaderPos(unzF, &local_header_pos);
    local_header_pos = info.nStartDataStream + file_info.compressed_size;
    unz_file_info file_info2;
    std::string osAuxName;
    osAuxName.resize(1024);
    uLong64 indexPos;
    if (file_info.compression_method == 8 &&
        cpl_unzCurrentFileInfoFromLocalHeader(
            unzF, local_header_pos, &file_info2, &osAuxName[0],
            osAuxName.size(), &indexPos) == UNZ_OK)
    {
        osAuxName.resize(strlen(osAuxName.c_str()));
        if (osAuxName.find(".sozip.idx") != std::string::npos)
        {
            info.bSOZipIndexFound = true;
            info.nSOZIPStartData = indexPos;
            poVirtualHandle->Seek(indexPos, SEEK_SET);
            uint32_t nVersion = 0;
            poVirtualHandle->Read(&nVersion, sizeof(nVersion), 1);
            CPL_LSBPTR32(&nVersion);
            uint32_t nToSkip = 0;
            poVirtualHandle->Read(&nToSkip, sizeof(nToSkip), 1);
            CPL_LSBPTR32(&nToSkip);
            uint32_t nChunkSize = 0;
            poVirtualHandle->Read(&nChunkSize, sizeof(nChunkSize), 1);
            CPL_LSBPTR32(&nChunkSize);
            uint32_t nOffsetSize = 0;
            poVirtualHandle->Read(&nOffsetSize, sizeof(nOffsetSize), 1);
            CPL_LSBPTR32(&nOffsetSize);
            uint64_t nUncompressedSize = 0;
            poVirtualHandle->Read(&nUncompressedSize, sizeof(nUncompressedSize),
                                  1);
            CPL_LSBPTR64(&nUncompressedSize);
            uint64_t nCompressedSize = 0;
            poVirtualHandle->Read(&nCompressedSize, sizeof(nCompressedSize), 1);
            CPL_LSBPTR64(&nCompressedSize);

            info.nSOZIPVersion = nVersion;
            info.nSOZIPToSkip = nToSkip;
            info.nSOZIPChunkSize = nChunkSize;
            info.nSOZIPOffsetSize = nOffsetSize;

            bool bValid = true;
            if (nVersion != 1)
            {
                CPLDebug("SOZIP", "version = %u, expected 1", nVersion);
                bValid = false;
            }
            if (nCompressedSize != file_info.compressed_size)
            {
                CPLDebug("SOZIP",
                         "compressedSize field inconsistent with file");
                bValid = false;
            }
            if (nUncompressedSize != file_info.uncompressed_size)
            {
                CPLDebug("SOZIP",
                         "uncompressedSize field inconsistent with file");
                bValid = false;
            }
            if (!(nChunkSize > 0 && nChunkSize < 100 * 1024 * 1024))
            {
                CPLDebug("SOZIP", "invalid chunkSize = %u", nChunkSize);
                bValid = false;
            }
            if (nOffsetSize != 8)
            {
                CPLDebug("SOZIP", "invalid offsetSize = %u", nOffsetSize);
                bValid = false;
            }
            if (file_info2.compression_method != 0)
            {
                CPLDebug("SOZIP", "unexpected compression_method = %u",
                         static_cast<unsigned>(file_info2.compression_method));
                bValid = false;
            }
            if (bValid)
            {
                const auto nExpectedIndexSize =
                    32 + static_cast<uint64_t>(nToSkip) +
                    ((nUncompressedSize - 1) / nChunkSize) * nOffsetSize;
                if (nExpectedIndexSize != file_info2.uncompressed_size)
                {
                    CPLDebug("SOZIP", "invalid file size for index");
                    bValid = false;
                }
            }
            if (bValid)
            {
                info.bSOZipIndexValid = true;
                CPLDebug("SOZIP", "Found valid SOZIP index: %s",
                         osAuxName.c_str());
            }
            else
            {
                CPLDebug("SOZIP", "Found *invalid* SOZIP index: %s",
                         osAuxName.c_str());
            }
        }
    }

    cpl_unzCloseCurrentFile(unzF);

    delete poReader;

    info.poVirtualHandle.reset(poVirtualHandle);

    return true;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle *VSIZipFilesystemHandler::Open(const char *pszFilename,
                                                const char *pszAccess,
                                                bool /* bSetError */,
                                                CSLConstList /* papszOptions */)
{

    if (strchr(pszAccess, 'w') != nullptr)
    {
        return OpenForWrite(pszFilename, pszAccess);
    }

    if (strchr(pszAccess, '+') != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read-write random access not supported for /vsizip");
        return nullptr;
    }

    VSIFileInZipInfo info;
    if (!GetFileInfo(pszFilename, info))
        return nullptr;

#ifdef ENABLE_DEFLATE64
    if (info.nCompressionMethod == 9)
    {
        auto poGZIPHandle = new VSIDeflate64Handle(
            info.poVirtualHandle.release(), nullptr, info.nStartDataStream,
            info.nCompressedSize, info.nUncompressedSize, info.nCRC);
        if (!(poGZIPHandle->IsInitOK()))
        {
            delete poGZIPHandle;
            return nullptr;
        }

        // Wrap the VSIGZipHandle inside a buffered reader that will
        // improve dramatically performance when doing small backward
        // seeks.
        return VSICreateBufferedReaderHandle(poGZIPHandle);
    }
    else
#endif
    {
        if (info.bSOZipIndexValid)
        {
            auto poSOZIPHandle = new VSISOZipHandle(
                info.poVirtualHandle.release(), info.nStartDataStream,
                info.nCompressedSize, info.nUncompressedSize,
                info.nSOZIPStartData, info.nSOZIPToSkip, info.nSOZIPChunkSize);
            if (!poSOZIPHandle->IsOK())
            {
                delete poSOZIPHandle;
                return nullptr;
            }
            return VSICreateCachedFile(poSOZIPHandle, info.nSOZIPChunkSize, 0);
        }

        VSIGZipHandle *poGZIPHandle = new VSIGZipHandle(
            info.poVirtualHandle.release(), nullptr, info.nStartDataStream,
            info.nCompressedSize, info.nUncompressedSize, info.nCRC,
            info.nCompressionMethod == 0);
        if (!(poGZIPHandle->IsInitOK()))
        {
            delete poGZIPHandle;
            return nullptr;
        }

        // Wrap the VSIGZipHandle inside a buffered reader that will
        // improve dramatically performance when doing small backward
        // seeks.
        return VSICreateBufferedReaderHandle(poGZIPHandle);
    }
}

/************************************************************************/
/*                          GetFileMetadata()                           */
/************************************************************************/

char **VSIZipFilesystemHandler::GetFileMetadata(const char *pszFilename,
                                                const char *pszDomain,
                                                CSLConstList /*papszOptions*/)
{
    VSIFileInZipInfo info;
    if (!GetFileInfo(pszFilename, info))
        return nullptr;

    if (!pszDomain)
    {
        CPLStringList aosMetadata;
        for (const auto &kv : info.oMapProperties)
        {
            aosMetadata.AddNameValue(kv.first.c_str(), kv.second.c_str());
        }
        return aosMetadata.StealList();
    }
    else if (EQUAL(pszDomain, "ZIP"))
    {
        CPLStringList aosMetadata;
        aosMetadata.SetNameValue(
            "START_DATA_OFFSET",
            CPLSPrintf(CPL_FRMT_GUIB,
                       static_cast<GUIntBig>(info.nStartDataStream)));

        if (info.nCompressionMethod == 0)
            aosMetadata.SetNameValue("COMPRESSION_METHOD", "0 (STORED)");
        else if (info.nCompressionMethod == 8)
            aosMetadata.SetNameValue("COMPRESSION_METHOD", "8 (DEFLATE)");
        else
        {
            aosMetadata.SetNameValue("COMPRESSION_METHOD",
                                     CPLSPrintf("%d", info.nCompressionMethod));
        }
        aosMetadata.SetNameValue(
            "COMPRESSED_SIZE",
            CPLSPrintf(CPL_FRMT_GUIB,
                       static_cast<GUIntBig>(info.nCompressedSize)));
        aosMetadata.SetNameValue(
            "UNCOMPRESSED_SIZE",
            CPLSPrintf(CPL_FRMT_GUIB,
                       static_cast<GUIntBig>(info.nUncompressedSize)));

        if (info.bSOZipIndexFound)
        {
            aosMetadata.SetNameValue("SOZIP_FOUND", "YES");

            aosMetadata.SetNameValue("SOZIP_VERSION",
                                     CPLSPrintf("%u", info.nSOZIPVersion));

            aosMetadata.SetNameValue("SOZIP_OFFSET_SIZE",
                                     CPLSPrintf("%u", info.nSOZIPOffsetSize));

            aosMetadata.SetNameValue("SOZIP_CHUNK_SIZE",
                                     CPLSPrintf("%u", info.nSOZIPChunkSize));

            aosMetadata.SetNameValue(
                "SOZIP_START_DATA_OFFSET",
                CPLSPrintf(CPL_FRMT_GUIB,
                           static_cast<GUIntBig>(info.nSOZIPStartData)));

            if (info.bSOZipIndexValid)
            {
                aosMetadata.SetNameValue("SOZIP_VALID", "YES");
            }
        }

        return aosMetadata.StealList();
    }
    return nullptr;
}

/************************************************************************/
/*                                Mkdir()                               */
/************************************************************************/

int VSIZipFilesystemHandler::Mkdir(const char *pszDirname, long /* nMode */)
{
    CPLString osDirname = pszDirname;
    if (!osDirname.empty() && osDirname.back() != '/')
        osDirname += "/";
    VSIVirtualHandle *poZIPHandle = OpenForWrite(osDirname, "wb");
    if (poZIPHandle == nullptr)
        return -1;
    delete poZIPHandle;
    return 0;
}

/************************************************************************/
/*                               ReadDirEx()                            */
/************************************************************************/

char **VSIZipFilesystemHandler::ReadDirEx(const char *pszDirname, int nMaxFiles)
{
    CPLString osInArchiveSubDir;
    char *zipFilename = SplitFilename(pszDirname, osInArchiveSubDir, TRUE);
    if (zipFilename == nullptr)
        return nullptr;

    {
        CPLMutexHolder oHolder(&hMutex);

        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return nullptr;
        }
    }
    CPLFree(zipFilename);

    return VSIArchiveFilesystemHandler::ReadDirEx(pszDirname, nMaxFiles);
}

/************************************************************************/
/*                                 Stat()                               */
/************************************************************************/

int VSIZipFilesystemHandler::Stat(const char *pszFilename,
                                  VSIStatBufL *pStatBuf, int nFlags)
{
    CPLString osInArchiveSubDir;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    char *zipFilename = SplitFilename(pszFilename, osInArchiveSubDir, TRUE);
    if (zipFilename == nullptr)
        return -1;

    {
        CPLMutexHolder oHolder(&hMutex);

        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return -1;
        }
    }
    CPLFree(zipFilename);

    return VSIArchiveFilesystemHandler::Stat(pszFilename, pStatBuf, nFlags);
}

/************************************************************************/
/*                             RemoveFromMap()                           */
/************************************************************************/

void VSIZipFilesystemHandler::RemoveFromMap(VSIZipWriteHandle *poHandle)
{
    CPLMutexHolder oHolder(&hMutex);

    for (std::map<CPLString, VSIZipWriteHandle *>::iterator iter =
             oMapZipWriteHandles.begin();
         iter != oMapZipWriteHandles.end(); ++iter)
    {
        if (iter->second == poHandle)
        {
            oMapZipWriteHandles.erase(iter);
            break;
        }
    }
}

/************************************************************************/
/*                             OpenForWrite()                           */
/************************************************************************/

VSIVirtualHandle *VSIZipFilesystemHandler::OpenForWrite(const char *pszFilename,
                                                        const char *pszAccess)
{
    CPLMutexHolder oHolder(&hMutex);
    return OpenForWrite_unlocked(pszFilename, pszAccess);
}

VSIVirtualHandle *
VSIZipFilesystemHandler::OpenForWrite_unlocked(const char *pszFilename,
                                               const char *pszAccess)
{
    CPLString osZipInFileName;

    char *zipFilename = SplitFilename(pszFilename, osZipInFileName, FALSE);
    if (zipFilename == nullptr)
        return nullptr;
    CPLString osZipFilename = zipFilename;
    CPLFree(zipFilename);
    zipFilename = nullptr;

    // Invalidate cached file list.
    std::map<CPLString, VSIArchiveContent *>::iterator iter =
        oFileList.find(osZipFilename);
    if (iter != oFileList.end())
    {
        delete iter->second;

        oFileList.erase(iter);
    }

    if (oMapZipWriteHandles.find(osZipFilename) != oMapZipWriteHandles.end())
    {
        if (strchr(pszAccess, '+') != nullptr)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Random access not supported for writable file in /vsizip");
            return nullptr;
        }

        VSIZipWriteHandle *poZIPHandle = oMapZipWriteHandles[osZipFilename];

        if (poZIPHandle->GetChildInWriting() != nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create %s while another file is being "
                     "written in the .zip",
                     osZipInFileName.c_str());
            return nullptr;
        }

        poZIPHandle->StopCurrentFile();

        // Re-add path separator when creating directories.
        char chLastChar = pszFilename[strlen(pszFilename) - 1];
        if (chLastChar == '/' || chLastChar == '\\')
            osZipInFileName += chLastChar;

        if (CPLCreateFileInZip(poZIPHandle->GetHandle(), osZipInFileName,
                               nullptr) != CE_None)
            return nullptr;

        VSIZipWriteHandle *poChildHandle =
            new VSIZipWriteHandle(this, nullptr, poZIPHandle);

        poZIPHandle->StartNewFile(poChildHandle);

        return poChildHandle;
    }
    else
    {
        char **papszOptions = nullptr;
        if ((strchr(pszAccess, '+') && osZipInFileName.empty()) ||
            !osZipInFileName.empty())
        {
            VSIStatBufL sBuf;
            if (VSIStatExL(osZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0)
                papszOptions = CSLAddNameValue(papszOptions, "APPEND", "TRUE");
        }

        void *hZIP = CPLCreateZip(osZipFilename, papszOptions);
        CSLDestroy(papszOptions);

        if (hZIP == nullptr)
            return nullptr;

        auto poHandle = new VSIZipWriteHandle(this, hZIP, nullptr);
        oMapZipWriteHandles[osZipFilename] = poHandle;

        if (!osZipInFileName.empty())
        {
            VSIZipWriteHandle *poRes = reinterpret_cast<VSIZipWriteHandle *>(
                OpenForWrite_unlocked(pszFilename, pszAccess));
            if (poRes == nullptr)
            {
                delete poHandle;
                oMapZipWriteHandles.erase(osZipFilename);
                return nullptr;
            }

            poRes->SetAutoDeleteParent();

            return poRes;
        }

        return poHandle;
    }
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIZipFilesystemHandler::GetOptions()
{
    return "<Options>"
           "  <Option name='GDAL_NUM_THREADS' type='string' "
           "description='Number of threads for compression. Either a integer "
           "or ALL_CPUS'/>"
           "  <Option name='CPL_VSIL_DEFLATE_CHUNK_SIZE' type='string' "
           "description='Chunk of uncompressed data for parallelization. "
           "Use K(ilobytes) or M(egabytes) suffix' default='1M'/>"
           "</Options>";
}

/************************************************************************/
/*                           CopyFile()                                 */
/************************************************************************/

int VSIZipFilesystemHandler::CopyFile(const char *pszSource,
                                      const char *pszTarget, VSILFILE *fpSource,
                                      vsi_l_offset /* nSourceSize */,
                                      CSLConstList papszOptions,
                                      GDALProgressFunc pProgressFunc,
                                      void *pProgressData)
{
    CPLString osZipInFileName;

    char *zipFilename = SplitFilename(pszTarget, osZipInFileName, FALSE);
    if (zipFilename == nullptr)
        return -1;
    CPLString osZipFilename = zipFilename;
    CPLFree(zipFilename);
    zipFilename = nullptr;
    if (osZipInFileName.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Target filename should be of the form "
                 "/vsizip/path_to.zip/filename_within_zip");
        return -1;
    }

    // Invalidate cached file list.
    auto oIterFileList = oFileList.find(osZipFilename);
    if (oIterFileList != oFileList.end())
    {
        delete oIterFileList->second;

        oFileList.erase(oIterFileList);
    }

    const auto oIter = oMapZipWriteHandles.find(osZipFilename);
    if (oIter != oMapZipWriteHandles.end())
    {
        VSIZipWriteHandle *poZIPHandle = oIter->second;

        if (poZIPHandle->GetChildInWriting() != nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create %s while another file is being "
                     "written in the .zip",
                     osZipInFileName.c_str());
            return -1;
        }

        if (CPLAddFileInZip(poZIPHandle->GetHandle(), osZipInFileName.c_str(),
                            pszSource, fpSource, papszOptions, pProgressFunc,
                            pProgressData) != CE_None)
        {
            return -1;
        }
        return 0;
    }
    else
    {
        CPLStringList aosOptionsCreateZip;
        VSIStatBufL sBuf;
        if (VSIStatExL(osZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0)
            aosOptionsCreateZip.SetNameValue("APPEND", "TRUE");

        void *hZIP = CPLCreateZip(osZipFilename, aosOptionsCreateZip.List());

        if (hZIP == nullptr)
            return -1;

        if (CPLAddFileInZip(hZIP, osZipInFileName.c_str(), pszSource, fpSource,
                            papszOptions, pProgressFunc,
                            pProgressData) != CE_None)
        {
            CPLCloseZip(hZIP);
            return -1;
        }
        CPLCloseZip(hZIP);
        return 0;
    }
}

/************************************************************************/
/*                          VSIZipWriteHandle()                         */
/************************************************************************/

VSIZipWriteHandle::VSIZipWriteHandle(VSIZipFilesystemHandler *poFS, void *hZIP,
                                     VSIZipWriteHandle *poParent)
    : m_poFS(poFS), m_hZIP(hZIP), m_poParent(poParent)
{
}

/************************************************************************/
/*                         ~VSIZipWriteHandle()                         */
/************************************************************************/

VSIZipWriteHandle::~VSIZipWriteHandle()
{
    VSIZipWriteHandle::Close();
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIZipWriteHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    if (nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR))
        return 0;
    if (nOffset == nCurOffset && nWhence == SEEK_SET)
        return 0;

    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFSeekL() is not supported on writable Zip files");
    return -1;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIZipWriteHandle::Tell()
{
    return nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIZipWriteHandle::Read(void * /* pBuffer */, size_t /* nSize */,
                               size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL() is not supported on writable Zip files");
    return 0;
}

/************************************************************************/
/*                               Write()                                 */
/************************************************************************/

size_t VSIZipWriteHandle::Write(const void *pBuffer, size_t nSize, size_t nMemb)
{
    if (m_poParent == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VSIFWriteL() is not supported on "
                 "main Zip file or closed subfiles");
        return 0;
    }

    const GByte *pabyBuffer = static_cast<const GByte *>(pBuffer);
    size_t nBytesToWrite = nSize * nMemb;
    size_t nWritten = 0;
    while (nWritten < nBytesToWrite)
    {
        int nToWrite = static_cast<int>(
            std::min(static_cast<size_t>(INT_MAX), nBytesToWrite));
        if (CPLWriteFileInZip(m_poParent->m_hZIP, pabyBuffer, nToWrite) !=
            CE_None)
            return 0;
        nWritten += nToWrite;
        pabyBuffer += nToWrite;
    }

    nCurOffset += nSize * nMemb;

    return nMemb;
}

/************************************************************************/
/*                                Eof()                                 */
/************************************************************************/

int VSIZipWriteHandle::Eof()
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFEofL() is not supported on writable Zip files");
    return FALSE;
}

/************************************************************************/
/*                               Flush()                                */
/************************************************************************/

int VSIZipWriteHandle::Flush()
{
    /*CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFFlushL() is not supported on writable Zip files");*/
    return 0;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIZipWriteHandle::Close()
{
    int nRet = 0;
    if (m_poParent)
    {
        CPLCloseFileInZip(m_poParent->m_hZIP);
        m_poParent->poChildInWriting = nullptr;
        if (bAutoDeleteParent)
        {
            if (m_poParent->Close() != 0)
                nRet = -1;
            delete m_poParent;
        }
        m_poParent = nullptr;
    }
    if (poChildInWriting)
    {
        if (poChildInWriting->Close() != 0)
            nRet = -1;
        poChildInWriting = nullptr;
    }
    if (m_hZIP)
    {
        if (CPLCloseZip(m_hZIP) != CE_None)
            nRet = -1;
        m_hZIP = nullptr;

        m_poFS->RemoveFromMap(this);
    }

    return nRet;
}

/************************************************************************/
/*                           StopCurrentFile()                          */
/************************************************************************/

void VSIZipWriteHandle::StopCurrentFile()
{
    if (poChildInWriting)
        poChildInWriting->Close();
    poChildInWriting = nullptr;
}

/************************************************************************/
/*                           StartNewFile()                             */
/************************************************************************/

void VSIZipWriteHandle::StartNewFile(VSIZipWriteHandle *poSubFile)
{
    poChildInWriting = poSubFile;
}

//! @endcond

/************************************************************************/
/*                    VSIInstallZipFileHandler()                        */
/************************************************************************/

/*!
 \brief Install ZIP file system handler.

 A special file handler is installed that allows reading on-the-fly in ZIP
 (.zip) archives.

 All portions of the file system underneath the base path "/vsizip/" will be
 handled by this driver.

 \verbatim embed:rst
 See :ref:`/vsizip/ documentation <vsizip>`
 \endverbatim

 @since GDAL 1.6.0
 */

void VSIInstallZipFileHandler()
{
    VSIFileManager::InstallHandler("/vsizip/", new VSIZipFilesystemHandler());
}

/************************************************************************/
/*                         CPLZLibDeflate()                             */
/************************************************************************/

/**
 * \brief Compress a buffer with ZLib compression.
 *
 * @param ptr input buffer.
 * @param nBytes size of input buffer in bytes.
 * @param nLevel ZLib compression level (-1 for default).
 * @param outptr output buffer, or NULL to let the function allocate it.
 * @param nOutAvailableBytes size of output buffer if provided, or ignored.
 * @param pnOutBytes pointer to a size_t, where to store the size of the
 *                   output buffer.
 *
 * @return the output buffer (to be freed with VSIFree() if not provided)
 *         or NULL in case of error.
 *
 * @since GDAL 1.10.0
 */

void *CPLZLibDeflate(const void *ptr, size_t nBytes, int nLevel, void *outptr,
                     size_t nOutAvailableBytes, size_t *pnOutBytes)
{
    if (pnOutBytes != nullptr)
        *pnOutBytes = 0;

    size_t nTmpSize = 0;
    void *pTmp;
#ifdef HAVE_LIBDEFLATE
    struct libdeflate_compressor *enc =
        libdeflate_alloc_compressor(nLevel < 0 ? 7 : nLevel);
    if (enc == nullptr)
    {
        return nullptr;
    }
#endif
    if (outptr == nullptr)
    {
#ifdef HAVE_LIBDEFLATE
        nTmpSize = libdeflate_zlib_compress_bound(enc, nBytes);
#else
        nTmpSize = 32 + nBytes * 2;
#endif
        pTmp = VSIMalloc(nTmpSize);
        if (pTmp == nullptr)
        {
#ifdef HAVE_LIBDEFLATE
            libdeflate_free_compressor(enc);
#endif
            return nullptr;
        }
    }
    else
    {
        pTmp = outptr;
        nTmpSize = nOutAvailableBytes;
    }

#ifdef HAVE_LIBDEFLATE
    size_t nCompressedBytes =
        libdeflate_zlib_compress(enc, ptr, nBytes, pTmp, nTmpSize);
    libdeflate_free_compressor(enc);
    if (nCompressedBytes == 0)
    {
        if (pTmp != outptr)
            VSIFree(pTmp);
        return nullptr;
    }
    if (pnOutBytes != nullptr)
        *pnOutBytes = nCompressedBytes;
#else
    z_stream strm;
    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    int ret = deflateInit(&strm, nLevel < 0 ? Z_DEFAULT_COMPRESSION : nLevel);
    if (ret != Z_OK)
    {
        if (pTmp != outptr)
            VSIFree(pTmp);
        return nullptr;
    }

    strm.avail_in = static_cast<uInt>(nBytes);
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<void *>(ptr));
    strm.avail_out = static_cast<uInt>(nTmpSize);
    strm.next_out = reinterpret_cast<Bytef *>(pTmp);
    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        if (pTmp != outptr)
            VSIFree(pTmp);
        return nullptr;
    }
    if (pnOutBytes != nullptr)
        *pnOutBytes = nTmpSize - strm.avail_out;
    deflateEnd(&strm);
#endif

    return pTmp;
}

/************************************************************************/
/*                         CPLZLibInflate()                             */
/************************************************************************/

/**
 * \brief Uncompress a buffer compressed with ZLib compression.
 *
 * @param ptr input buffer.
 * @param nBytes size of input buffer in bytes.
 * @param outptr output buffer, or NULL to let the function allocate it.
 * @param nOutAvailableBytes size of output buffer if provided, or ignored.
 * @param pnOutBytes pointer to a size_t, where to store the size of the
 *                   output buffer.
 *
 * @return the output buffer (to be freed with VSIFree() if not provided)
 *         or NULL in case of error.
 *
 * @since GDAL 1.10.0
 */

void *CPLZLibInflate(const void *ptr, size_t nBytes, void *outptr,
                     size_t nOutAvailableBytes, size_t *pnOutBytes)
{
    return CPLZLibInflateEx(ptr, nBytes, outptr, nOutAvailableBytes, false,
                            pnOutBytes);
}

/************************************************************************/
/*                         CPLZLibInflateEx()                           */
/************************************************************************/

/**
 * \brief Uncompress a buffer compressed with ZLib compression.
 *
 * @param ptr input buffer.
 * @param nBytes size of input buffer in bytes.
 * @param outptr output buffer, or NULL to let the function allocate it.
 * @param nOutAvailableBytes size of output buffer if provided, or ignored.
 * @param bAllowResizeOutptr whether the function is allowed to grow outptr
 *                           (using VSIRealloc) if its initial capacity
 *                           provided by nOutAvailableBytes is not
 *                           large enough. Ignored if outptr is NULL.
 * @param pnOutBytes pointer to a size_t, where to store the size of the
 *                   output buffer.
 *
 * @return the output buffer (to be freed with VSIFree() if not provided)
 *         or NULL in case of error. If bAllowResizeOutptr is set to true,
 *         only the returned pointer should be freed by the caller, as outptr
 *         might have been reallocated or freed.
 *
 * @since GDAL 3.9.0
 */

void *CPLZLibInflateEx(const void *ptr, size_t nBytes, void *outptr,
                       size_t nOutAvailableBytes, bool bAllowResizeOutptr,
                       size_t *pnOutBytes)
{
    if (pnOutBytes != nullptr)
        *pnOutBytes = 0;
    char *pszReallocatableBuf = nullptr;

#ifdef HAVE_LIBDEFLATE
    if (outptr)
    {
        struct libdeflate_decompressor *dec = libdeflate_alloc_decompressor();
        if (dec == nullptr)
        {
            if (bAllowResizeOutptr)
                VSIFree(outptr);
            return nullptr;
        }
        enum libdeflate_result res;
        size_t nOutBytes = 0;
        if (nBytes > 2 && static_cast<const GByte *>(ptr)[0] == 0x1F &&
            static_cast<const GByte *>(ptr)[1] == 0x8B)
        {
            res = libdeflate_gzip_decompress(dec, ptr, nBytes, outptr,
                                             nOutAvailableBytes, &nOutBytes);
        }
        else
        {
            res = libdeflate_zlib_decompress(dec, ptr, nBytes, outptr,
                                             nOutAvailableBytes, &nOutBytes);
        }
        if (pnOutBytes)
            *pnOutBytes = nOutBytes;
        libdeflate_free_decompressor(dec);
        if (res == LIBDEFLATE_INSUFFICIENT_SPACE && bAllowResizeOutptr)
        {
            if (nOutAvailableBytes >
                (std::numeric_limits<size_t>::max() - 1) / 2)
            {
                VSIFree(outptr);
                return nullptr;
            }
            size_t nOutBufSize = nOutAvailableBytes * 2;
            pszReallocatableBuf = static_cast<char *>(
                VSI_REALLOC_VERBOSE(outptr, nOutBufSize + 1));
            if (!pszReallocatableBuf)
            {
                VSIFree(outptr);
                return nullptr;
            }
            outptr = nullptr;
            nOutAvailableBytes = nOutBufSize;
        }
        else if (res != LIBDEFLATE_SUCCESS)
        {
            if (bAllowResizeOutptr)
                VSIFree(outptr);
            return nullptr;
        }
        else
        {
            // Nul-terminate if possible.
            if (nOutBytes < nOutAvailableBytes)
            {
                static_cast<char *>(outptr)[nOutBytes] = '\0';
            }
            return outptr;
        }
    }
#endif

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    int ret;
    // MAX_WBITS + 32 mode which detects automatically gzip vs zlib
    // encapsulation seems to be broken with
    // /opt/intel/oneapi/intelpython/latest/lib/libz.so.1 from
    // intel/oneapi-basekit Docker image
    if (nBytes > 2 && static_cast<const GByte *>(ptr)[0] == 0x1F &&
        static_cast<const GByte *>(ptr)[1] == 0x8B)
    {
        ret = inflateInit2(&strm, MAX_WBITS + 16);  // gzip
    }
    else
    {
        ret = inflateInit2(&strm, MAX_WBITS);  // zlib
    }
    if (ret != Z_OK)
    {
        if (bAllowResizeOutptr)
            VSIFree(outptr);
        VSIFree(pszReallocatableBuf);
        return nullptr;
    }

    size_t nOutBufSize = 0;
    char *pszOutBuf = nullptr;

#ifdef HAVE_LIBDEFLATE
    if (pszReallocatableBuf)
    {
        pszOutBuf = pszReallocatableBuf;
        nOutBufSize = nOutAvailableBytes;
    }
    else
#endif
        if (!outptr)
    {
        if (nBytes > (std::numeric_limits<size_t>::max() - 1) / 2)
        {
            inflateEnd(&strm);
            return nullptr;
        }
        nOutBufSize = 2 * nBytes + 1;
        pszOutBuf = static_cast<char *>(VSI_MALLOC_VERBOSE(nOutBufSize));
        if (pszOutBuf == nullptr)
        {
            inflateEnd(&strm);
            return nullptr;
        }
        pszReallocatableBuf = pszOutBuf;
        bAllowResizeOutptr = true;
    }
#ifndef HAVE_LIBDEFLATE
    else
    {
        pszOutBuf = static_cast<char *>(outptr);
        nOutBufSize = nOutAvailableBytes;
        if (bAllowResizeOutptr)
            pszReallocatableBuf = pszOutBuf;
    }
#endif

    strm.next_in = static_cast<Bytef *>(const_cast<void *>(ptr));
    strm.next_out = reinterpret_cast<Bytef *>(pszOutBuf);
    size_t nInBytesRemaining = nBytes;
    size_t nOutBytesRemaining = nOutBufSize;

    while (true)
    {
        strm.avail_in = static_cast<uInt>(std::min<size_t>(
            nInBytesRemaining, std::numeric_limits<uInt>::max()));
        const auto avail_in_before = strm.avail_in;
        strm.avail_out = static_cast<uInt>(std::min<size_t>(
            nOutBytesRemaining, std::numeric_limits<uInt>::max()));
        const auto avail_out_before = strm.avail_out;
        ret = inflate(&strm, Z_FINISH);
        nInBytesRemaining -= (avail_in_before - strm.avail_in);
        nOutBytesRemaining -= (avail_out_before - strm.avail_out);

        if (ret == Z_BUF_ERROR && strm.avail_out == 0)
        {
#ifdef HAVE_LIBDEFLATE
            CPLAssert(bAllowResizeOutptr);
#else
            if (!bAllowResizeOutptr)
            {
                VSIFree(pszReallocatableBuf);
                inflateEnd(&strm);
                return nullptr;
            }
#endif

            const size_t nAlreadyWritten = nOutBufSize - nOutBytesRemaining;
            if (nOutBufSize > (std::numeric_limits<size_t>::max() - 1) / 2)
            {
                VSIFree(pszReallocatableBuf);
                inflateEnd(&strm);
                return nullptr;
            }
            nOutBufSize = nOutBufSize * 2 + 1;
            char *pszNew = static_cast<char *>(
                VSI_REALLOC_VERBOSE(pszReallocatableBuf, nOutBufSize));
            if (!pszNew)
            {
                VSIFree(pszReallocatableBuf);
                inflateEnd(&strm);
                return nullptr;
            }
            pszOutBuf = pszNew;
            pszReallocatableBuf = pszOutBuf;
            nOutBytesRemaining = nOutBufSize - nAlreadyWritten;
            strm.next_out =
                reinterpret_cast<Bytef *>(pszOutBuf + nAlreadyWritten);
        }
        else if (ret != Z_OK || nInBytesRemaining == 0)
            break;
    }

    if (ret == Z_OK || ret == Z_STREAM_END)
    {
        size_t nOutBytes = nOutBufSize - nOutBytesRemaining;
        // Nul-terminate if possible.
        if (nOutBytes < nOutBufSize)
        {
            pszOutBuf[nOutBytes] = '\0';
        }
        inflateEnd(&strm);
        if (pnOutBytes != nullptr)
            *pnOutBytes = nOutBytes;
        return pszOutBuf;
    }
    else
    {
        VSIFree(pszReallocatableBuf);
        inflateEnd(&strm);
        return nullptr;
    }
}
