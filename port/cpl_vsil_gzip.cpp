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
#  include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "cpl_zlib_header.h" // to avoid warnings when including zlib.h

#ifdef HAVE_LIBDEFLATE
#include "libdeflate.h"
#endif

#include <algorithm>
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

CPL_CVSID("$Id$")

constexpr int Z_BUFSIZE = 65536;  // Original size is 16384
constexpr int gz_magic[2] = {0x1f, 0x8b};  // gzip magic header

// gzip flag byte.
#define ASCII_FLAG   0x01  // bit 0 set: file probably ascii text
#define HEAD_CRC     0x02  // bit 1 set: header CRC present
#define EXTRA_FIELD  0x04  // bit 2 set: extra field present
#define ORIG_NAME    0x08  // bit 3 set: original file name present
#define COMMENT      0x10  // bit 4 set: file comment present
#define RESERVED     0xE0  // bits 5..7: reserved

#define ALLOC(size) malloc(size)
#define TRYFREE(p) {if (p) free(p);}

#define CPL_VSIL_GZ_RETURN(ret)   \
        CPLError(CE_Failure, CPLE_AppDefined, \
                 "In file %s, at line %d, return %d", __FILE__, __LINE__, ret)

// #define ENABLE_DEBUG 1

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipHandle                                  */
/* ==================================================================== */
/************************************************************************/

typedef struct
{
    vsi_l_offset  posInBaseHandle;
    z_stream      stream;
    uLong         crc;
    int           transparent;
    vsi_l_offset  in;
    vsi_l_offset  out;
} GZipSnapshot;

class VSIGZipHandle final : public VSIVirtualHandle
{
    VSIVirtualHandle* m_poBaseHandle = nullptr;
#ifdef DEBUG
    vsi_l_offset      m_offset = 0;
#endif
    vsi_l_offset      m_compressed_size = 0;
    vsi_l_offset      m_uncompressed_size = 0;
    vsi_l_offset      offsetEndCompressedData = 0;
    uLong             m_expected_crc = 0;
    char             *m_pszBaseFileName = nullptr; /* optional */
    bool              m_bWriteProperties = false;
    bool              m_bCanSaveInfo = false;

    /* Fields from gz_stream structure */
    z_stream stream;
    int      z_err = Z_OK;   /* error code for last stream operation */
    int      z_eof = 0;   /* set if end of input file (but not necessarily of the uncompressed stream ! "in" must be null too ) */
    Byte     *inbuf = nullptr;  /* input buffer */
    Byte     *outbuf = nullptr; /* output buffer */
    uLong    crc = 0;     /* crc32 of uncompressed data */
    int      m_transparent = 0; /* 1 if input file is not a .gz file */
    vsi_l_offset  startOff = 0;   /* startOff of compressed data in file (header skipped) */
    vsi_l_offset  in = 0;      /* bytes into deflate or inflate */
    vsi_l_offset  out = 0;     /* bytes out of deflate or inflate */
    vsi_l_offset  m_nLastReadOffset = 0;

    GZipSnapshot* snapshots = nullptr;
    vsi_l_offset snapshot_byte_interval = 0; /* number of compressed bytes at which we create a "snapshot" */

    void check_header();
    int get_byte();
    bool gzseek( vsi_l_offset nOffset, int nWhence );
    int gzrewind ();
    uLong getLong ();

    CPL_DISALLOW_COPY_ASSIGN(VSIGZipHandle)

  public:

    VSIGZipHandle( VSIVirtualHandle* poBaseHandle,
                   const char* pszBaseFileName,
                   vsi_l_offset offset = 0,
                   vsi_l_offset compressed_size = 0,
                   vsi_l_offset uncompressed_size = 0,
                   uLong expected_crc = 0,
                   int transparent = 0 );
    ~VSIGZipHandle() override;

    bool              IsInitOK() const { return inbuf != nullptr; }

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    VSIGZipHandle*    Duplicate();
    bool              CloseBaseHandle();

    vsi_l_offset      GetLastReadOffset() { return m_nLastReadOffset; }
    const char*       GetBaseFileName() { return m_pszBaseFileName; }

    void              SetUncompressedSize( vsi_l_offset nUncompressedSize )
        { m_uncompressed_size = nUncompressedSize; }
    vsi_l_offset      GetUncompressedSize() { return m_uncompressed_size; }

    void              SaveInfo_unlocked();
    void              UnsetCanSaveInfo() { m_bCanSaveInfo = false; }
};

class VSIGZipFilesystemHandler final : public VSIFilesystemHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGZipFilesystemHandler)

    CPLMutex* hMutex = nullptr;
    VSIGZipHandle* poHandleLastGZipFile = nullptr;
    bool           m_bInSaveInfo = false;

public:
    VSIGZipFilesystemHandler() = default;
    ~VSIGZipFilesystemHandler() override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;
    VSIGZipHandle *OpenGZipReadOnly( const char *pszFilename,
                                     const char *pszAccess );
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;
    int Unlink( const char *pszFilename ) override;
    int Rename( const char *oldpath, const char *newpath ) override;
    int Mkdir( const char *pszDirname, long nMode ) override;
    int Rmdir( const char *pszDirname ) override;
    char **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;

    const char* GetOptions() override;

    void SaveInfo( VSIGZipHandle* poHandle );
    void SaveInfo_unlocked( VSIGZipHandle* poHandle );
};

/************************************************************************/
/*                            Duplicate()                               */
/************************************************************************/

VSIGZipHandle* VSIGZipHandle::Duplicate()
{
    CPLAssert (m_offset == 0);
    CPLAssert (m_compressed_size != 0);
    CPLAssert (m_pszBaseFileName != nullptr);

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( m_pszBaseFileName );

    VSIVirtualHandle* poNewBaseHandle =
        poFSHandler->Open( m_pszBaseFileName, "rb" );

    if( poNewBaseHandle == nullptr )
        return nullptr;

    VSIGZipHandle* poHandle = new VSIGZipHandle(poNewBaseHandle,
                                                m_pszBaseFileName,
                                                0,
                                                m_compressed_size,
                                                m_uncompressed_size);
    if( !(poHandle->IsInitOK()) )
    {
        delete poHandle;
        return nullptr;
    }

    poHandle->m_nLastReadOffset = m_nLastReadOffset;

    // Most important: duplicate the snapshots!

    for( unsigned int i=0;
         i < m_compressed_size / snapshot_byte_interval + 1;
         i++ )
    {
        if( snapshots[i].posInBaseHandle == 0 )
            break;

        poHandle->snapshots[i].posInBaseHandle = snapshots[i].posInBaseHandle;
        inflateCopy( &poHandle->snapshots[i].stream, &snapshots[i].stream);
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
    if( m_poBaseHandle )
        bRet = VSIFCloseL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) == 0;
    m_poBaseHandle = nullptr;
    return bRet;
}

/************************************************************************/
/*                       VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::VSIGZipHandle( VSIVirtualHandle* poBaseHandle,
                              const char* pszBaseFileName,
                              vsi_l_offset offset,
                              vsi_l_offset compressed_size,
                              vsi_l_offset uncompressed_size,
                              uLong expected_crc,
                              int transparent ) :
    m_poBaseHandle(poBaseHandle),
#ifdef DEBUG
    m_offset(offset),
#endif
    m_uncompressed_size(uncompressed_size),
    m_expected_crc(expected_crc),
    m_pszBaseFileName(pszBaseFileName ? CPLStrdup(pszBaseFileName) : nullptr),
    m_bWriteProperties(CPLTestBool(
        CPLGetConfigOption("CPL_VSIL_GZIP_WRITE_PROPERTIES", "YES"))),
    m_bCanSaveInfo(CPLTestBool(
        CPLGetConfigOption("CPL_VSIL_GZIP_SAVE_INFO", "YES"))),
    stream(),
    crc(0),
    m_transparent(transparent)
{
    if( compressed_size || transparent )
    {
        m_compressed_size = compressed_size;
    }
    else
    {
        if( VSIFSeekL(reinterpret_cast<VSILFILE*>(poBaseHandle), 0, SEEK_END) != 0 )
            CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
        m_compressed_size = VSIFTellL(reinterpret_cast<VSILFILE*>(poBaseHandle)) - offset;
        compressed_size = m_compressed_size;
    }
    offsetEndCompressedData = offset + compressed_size;

    if( VSIFSeekL(reinterpret_cast<VSILFILE*>(poBaseHandle), offset, SEEK_SET) != 0 )
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
    if( err != Z_OK || inbuf == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "inflateInit2 init failed");
        TRYFREE(inbuf);
        inbuf = nullptr;
        return;
    }
    stream.avail_out = static_cast<uInt>(Z_BUFSIZE);

    if( offset == 0 ) check_header();  // Skip the .gz header.
    startOff = VSIFTellL(reinterpret_cast<VSILFILE*>(poBaseHandle)) - stream.avail_in;

    if( transparent == 0 )
    {
        snapshot_byte_interval = std::max(
            static_cast<vsi_l_offset>(Z_BUFSIZE), compressed_size / 100);
        snapshots = static_cast<GZipSnapshot *>(
            CPLCalloc(sizeof(GZipSnapshot),
                      static_cast<size_t>(
                          compressed_size / snapshot_byte_interval + 1)));
    }
}

/************************************************************************/
/*                      SaveInfo_unlocked()                             */
/************************************************************************/

void VSIGZipHandle::SaveInfo_unlocked()
{
    if( m_pszBaseFileName && m_bCanSaveInfo )
    {
        VSIFilesystemHandler *poFSHandler =
            VSIFileManager::GetHandler( "/vsigzip/" );
        reinterpret_cast<VSIGZipFilesystemHandler*>(poFSHandler)->
                                                    SaveInfo_unlocked(this);
        m_bCanSaveInfo = false;
    }
}

/************************************************************************/
/*                      ~VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::~VSIGZipHandle()
{
    if( m_pszBaseFileName && m_bCanSaveInfo )
    {
        VSIFilesystemHandler *poFSHandler =
            VSIFileManager::GetHandler( "/vsigzip/" );
        reinterpret_cast<VSIGZipFilesystemHandler*>(poFSHandler)->
            SaveInfo(this);
    }

    if( stream.state != nullptr )
    {
        inflateEnd(&(stream));
    }

    TRYFREE(inbuf);
    TRYFREE(outbuf);

    if( snapshots != nullptr )
    {
        for( size_t i=0;
             i < m_compressed_size / snapshot_byte_interval + 1;
             i++ )
        {
            if( snapshots[i].posInBaseHandle )
            {
                inflateEnd(&(snapshots[i].stream));
            }
        }
        CPLFree(snapshots);
    }
    CPLFree(m_pszBaseFileName);

    if( m_poBaseHandle )
        CPL_IGNORE_RET_VAL(VSIFCloseL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
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
    if( len < 2 )
    {
        if( len ) inbuf[0] = stream.next_in[0];
        errno = 0;
        len = static_cast<uInt>(
            VSIFReadL(inbuf + len, 1, static_cast<size_t>(Z_BUFSIZE) >> len,
                      reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)),
                 offsetEndCompressedData);
#endif
        if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) > offsetEndCompressedData )
        {
            len = len + static_cast<uInt>(
                offsetEndCompressedData - VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
            if( VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle),
                          offsetEndCompressedData, SEEK_SET) != 0 )
                z_err = Z_DATA_ERROR;
        }
        if( len == 0 )  // && ferror(file)
        {
            if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) !=
                offsetEndCompressedData )
                z_err = Z_ERRNO;
        }
        stream.avail_in += len;
        stream.next_in = inbuf;
        if( stream.avail_in < 2 )
        {
            m_transparent = stream.avail_in;
            return;
        }
    }

    // Peek ahead to check the gzip magic header.
    if( stream.next_in[0] != gz_magic[0] ||
        stream.next_in[1] != gz_magic[1]) {
        m_transparent = 1;
        return;
    }
    stream.avail_in -= 2;
    stream.next_in += 2;

    // Check the rest of the gzip header.
    const int method = get_byte();
    const int flags = get_byte();
    if( method != Z_DEFLATED || (flags & RESERVED) != 0 )
    {
        z_err = Z_DATA_ERROR;
        return;
    }

    // Discard time, xflags and OS code:
    for( len = 0; len < 6; len++ )
        CPL_IGNORE_RET_VAL(get_byte());

    if( (flags & EXTRA_FIELD) != 0 )
    {
        // Skip the extra field.
        len = static_cast<uInt>(get_byte()) & 0xFF;
        len += (static_cast<uInt>(get_byte()) & 0xFF) << 8;
        // len is garbage if EOF but the loop below will quit anyway.
        while( len != 0 && get_byte() != EOF )
        {
            --len;
        }
    }

    if( (flags & ORIG_NAME) != 0 )
    {
        // Skip the original file name.
        int c;
        while( (c = get_byte()) != 0 && c != EOF ) {}
    }
    if( (flags & COMMENT) != 0 )
    {
        // skip the .gz file comment.
        int c;
        while ((c = get_byte()) != 0 && c != EOF) {}
    }
    if( (flags & HEAD_CRC) != 0 )
    {
        // Skip the header crc.
        for( len = 0; len < 2; len++ )
            CPL_IGNORE_RET_VAL(get_byte());
    }
    z_err = z_eof ? Z_DATA_ERROR : Z_OK;
}

/************************************************************************/
/*                            get_byte()                                */
/************************************************************************/

int VSIGZipHandle::get_byte()
{
    if( z_eof ) return EOF;
    if( stream.avail_in == 0 )
    {
        errno = 0;
        stream.avail_in = static_cast<uInt>(
            VSIFReadL(inbuf, 1, static_cast<size_t>(Z_BUFSIZE),
                      reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)),
                 offsetEndCompressedData);
#endif
        if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) > offsetEndCompressedData )
        {
            stream.avail_in =
                stream.avail_in +
                static_cast<uInt>(
                    offsetEndCompressedData -
                    VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
            if( VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle),
                          offsetEndCompressedData, SEEK_SET) != 0 )
                return EOF;
        }
        if( stream.avail_in == 0 ) {
            z_eof = 1;
            if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) !=
                offsetEndCompressedData )
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

int VSIGZipHandle::gzrewind ()
{
    z_err = Z_OK;
    z_eof = 0;
    stream.avail_in = 0;
    stream.next_in = inbuf;
    crc = 0;
    if( !m_transparent )
        CPL_IGNORE_RET_VAL(inflateReset(&stream));
    in = 0;
    out = 0;
    return VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle), startOff, SEEK_SET);
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSIGZipHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    return gzseek(nOffset, nWhence) ? 0 : -1;
}

/************************************************************************/
/*                            gzseek()                                  */
/************************************************************************/

bool VSIGZipHandle::gzseek( vsi_l_offset offset, int whence )
{
    const vsi_l_offset original_offset = offset;
    const int original_nWhence = whence;

    z_eof = 0;
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Seek(" CPL_FRMT_GUIB ",%d)", offset, whence);
#endif

    if( m_transparent )
    {
        stream.avail_in = 0;
        stream.next_in = inbuf;
        if( whence == SEEK_CUR )
        {
            if( out + offset > m_compressed_size )
            {
                CPL_VSIL_GZ_RETURN(FALSE);
                return false;
            }

            offset = startOff + out + offset;
        }
        else if( whence == SEEK_SET )
        {
            if( offset > m_compressed_size )
            {
                CPL_VSIL_GZ_RETURN(FALSE);
                return false;
            }

            offset = startOff + offset;
        }
        else if( whence == SEEK_END )
        {
            // Commented test: because vsi_l_offset is unsigned (for the moment)
            // so no way to seek backward. See #1590 */
            if( offset > 0 ) // || -offset > compressed_size
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

        if( VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle), offset, SEEK_SET) < 0 )
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }

        out = offset - startOff;
        in = out;
        return true;
    }

    // whence == SEEK_END is unsuppored in original gzseek.
    if( whence == SEEK_END )
    {
        // If we known the uncompressed size, we can fake a jump to
        // the end of the stream.
        if( offset == 0 && m_uncompressed_size != 0 )
        {
            out = m_uncompressed_size;
            return true;
        }

        // We don't know the uncompressed size. This is unfortunate.
        // Do the slow version.
        static int firstWarning = 1;
        if( m_compressed_size > 10 * 1024 * 1024 && firstWarning )
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
    if( whence == SEEK_CUR )
    {
        offset += out;
    }

    // For a negative seek, rewind and use positive seek.
    if( offset >= out )
    {
        offset -= out;
    }
    else if( gzrewind() < 0 )
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    if( z_err != Z_OK && z_err != Z_STREAM_END )
    {
        CPL_VSIL_GZ_RETURN(FALSE);
        return false;
    }

    for( unsigned int i = 0;
         i < m_compressed_size / snapshot_byte_interval + 1;
         i++ )
    {
        if( snapshots[i].posInBaseHandle == 0 )
            break;
        if( snapshots[i].out <= out + offset &&
            (i == m_compressed_size / snapshot_byte_interval ||
             snapshots[i+1].out == 0 || snapshots[i+1].out > out+offset) )
        {
            if( out >= snapshots[i].out )
                break;

#ifdef ENABLE_DEBUG
            CPLDebug(
                "SNAPSHOT", "using snapshot %d : "
                "posInBaseHandle(snapshot)=" CPL_FRMT_GUIB
                " in(snapshot)=" CPL_FRMT_GUIB
                " out(snapshot)=" CPL_FRMT_GUIB
                " out=" CPL_FRMT_GUIB
                " offset=" CPL_FRMT_GUIB,
                i, snapshots[i].posInBaseHandle, snapshots[i].in,
                snapshots[i].out, out, offset);
#endif
            offset = out + offset - snapshots[i].out;
            if( VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle),
                          snapshots[i].posInBaseHandle, SEEK_SET) != 0 )
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

    if( offset != 0 && outbuf == nullptr )
    {
        outbuf = static_cast<Byte*>(ALLOC(Z_BUFSIZE));
        if( outbuf == nullptr )
        {
            CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
    }

    if( original_nWhence == SEEK_END && z_err == Z_STREAM_END )
    {
        return true;
    }

    while( offset > 0 )
    {
        int size = Z_BUFSIZE;
        if( offset < static_cast<vsi_l_offset>(Z_BUFSIZE) )
            size = static_cast<int>(offset);

        int read_size =
            static_cast<int>(Read(outbuf, 1, static_cast<uInt>(size)));
        if( read_size == 0 )
        {
            // CPL_VSIL_GZ_RETURN(FALSE);
            return false;
        }
        if( original_nWhence == SEEK_END )
        {
            if( size != read_size )
            {
                z_err = Z_STREAM_END;
                break;
            }
        }
        offset -= read_size;
    }
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "gzseek at offset " CPL_FRMT_GUIB, out);
#endif

    if( original_offset == 0 && original_nWhence == SEEK_END )
    {
        m_uncompressed_size = out;

        if( m_pszBaseFileName &&
            !STARTS_WITH_CI(m_pszBaseFileName, "/vsicurl/") &&
            m_bWriteProperties )
        {
            CPLString osCacheFilename (m_pszBaseFileName);
            osCacheFilename += ".properties";

            // Write a .properties file to avoid seeking next time.
            VSILFILE* fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "wb");
            if( fpCacheLength )
            {
                char szBuffer[32] = {};

                CPLPrintUIntBig(szBuffer, m_compressed_size, 31);
                char* pszFirstNonSpace = szBuffer;
                while( *pszFirstNonSpace == ' ' ) pszFirstNonSpace++;
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL(fpCacheLength,
                                "compressed_size=%s\n", pszFirstNonSpace));

                CPLPrintUIntBig(szBuffer, m_uncompressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while( *pszFirstNonSpace == ' ' ) pszFirstNonSpace++;
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL(fpCacheLength,
                                "uncompressed_size=%s\n", pszFirstNonSpace));

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

size_t VSIGZipHandle::Read( void * const buf, size_t const nSize,
                            size_t const nMemb )
{
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read(%p, %d, %d)", buf,
             static_cast<int>(nSize),
             static_cast<int>(nMemb));
#endif

    if( (z_eof && in == 0) || z_err == Z_STREAM_END )
    {
        z_eof = 1;
        in = 0;
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", "Read: Eof");
#endif
        return 0;  /* EOF */
    }

    const unsigned len =
        static_cast<unsigned int>(nSize) * static_cast<unsigned int>(nMemb);
    Bytef *pStart = static_cast<Bytef*>(buf);  // Start off point for crc computation.
    // == stream.next_out but not forced far (for MSDOS).
    Byte *next_out = static_cast<Byte *>(buf);
    stream.next_out = static_cast<Bytef *>(buf);
    stream.avail_out = len;

    while( stream.avail_out != 0 )
    {
        if( m_transparent )
        {
            // Copy first the lookahead bytes:
            uInt nRead = 0;
            uInt n = stream.avail_in;
            if( n > stream.avail_out )
                n = stream.avail_out;
            if( n > 0 )
            {
                memcpy (stream.next_out, stream.next_in, n);
                next_out += n;
                stream.next_out = next_out;
                stream.next_in += n;
                stream.avail_out -= n;
                stream.avail_in -= n;
                nRead += n;
            }
            if( stream.avail_out > 0 )
            {
                const uInt nToRead = static_cast<uInt>(
                    std::min(m_compressed_size - (in + nRead),
                             static_cast<vsi_l_offset>(stream.avail_out)));
                uInt nReadFromFile = static_cast<uInt>(
                    VSIFReadL(next_out, 1, nToRead, reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
                stream.avail_out -= nReadFromFile;
                nRead += nReadFromFile;
            }
            in += nRead;
            out += nRead;
            if( nRead < len )
                z_eof = 1;
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", "Read return %d", static_cast<int>(nRead / nSize));
#endif
            return static_cast<int>(nRead) / nSize;
        }
        if( stream.avail_in == 0 && !z_eof )
        {
            vsi_l_offset posInBaseHandle =
                VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle));
            if( posInBaseHandle - startOff > m_compressed_size )
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
            GZipSnapshot* snapshot =
                &snapshots[(posInBaseHandle - startOff) /
                           snapshot_byte_interval];
            if( snapshot->posInBaseHandle == 0 )
            {
                snapshot->crc =
                    crc32(crc, pStart,
                          static_cast<uInt>(stream.next_out - pStart));
#ifdef ENABLE_DEBUG
                CPLDebug("SNAPSHOT",
                         "creating snapshot %d : "
                         "posInBaseHandle=" CPL_FRMT_GUIB
                         " in=" CPL_FRMT_GUIB
                         " out=" CPL_FRMT_GUIB
                         " crc=%X",
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

                if( out > m_nLastReadOffset )
                    m_nLastReadOffset = out;
            }

            errno = 0;
            stream.avail_in = static_cast<uInt>(
                VSIFReadL(inbuf, 1, Z_BUFSIZE, reinterpret_cast<VSILFILE*>(m_poBaseHandle)));
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                     VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)),
                     offsetEndCompressedData);
#endif
            if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) > offsetEndCompressedData )
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
#endif
                stream.avail_in =
                    stream.avail_in -
                    static_cast<uInt>(VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) -
                                      offsetEndCompressedData);
                if( VSIFSeekL(reinterpret_cast<VSILFILE*>(m_poBaseHandle),
                              offsetEndCompressedData, SEEK_SET) != 0 )
                    CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
#endif
            }
            if( stream.avail_in == 0 )
            {
                z_eof = 1;
                if( VSIFTellL(reinterpret_cast<VSILFILE*>(m_poBaseHandle)) !=
                    offsetEndCompressedData )
                {
                    z_err = Z_ERRNO;
                    break;
                }
            }
            stream.next_in = inbuf;
        }
        in += stream.avail_in;
        out += stream.avail_out;
        z_err = inflate(& (stream), Z_NO_FLUSH);
        in -= stream.avail_in;
        out -= stream.avail_out;

        if( z_err == Z_STREAM_END && m_compressed_size != 2 )
        {
            // Check CRC and original size.
            crc = crc32(crc, pStart,
                        static_cast<uInt>(stream.next_out - pStart));
            pStart = stream.next_out;
            if( m_expected_crc )
            {
#ifdef ENABLE_DEBUG
                CPLDebug(
                    "GZIP",
                    "Computed CRC = %X. Expected CRC = %X",
                    static_cast<unsigned int>(crc),
                    static_cast<unsigned int>(m_expected_crc));
#endif
            }
            if( m_expected_crc != 0 && m_expected_crc != crc )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "CRC error. Got %X instead of %X",
                         static_cast<unsigned int>(crc),
                         static_cast<unsigned int>(m_expected_crc));
                z_err = Z_DATA_ERROR;
            }
            else if( m_expected_crc == 0 )
            {
                const uLong read_crc =
                    static_cast<unsigned long>(getLong());
                if( read_crc != crc )
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
                    if( z_err == Z_OK )
                    {
                        inflateReset(& (stream));
                        crc = 0;
                    }
                }
            }
        }
        if( z_err != Z_OK || z_eof )
            break;
    }
    crc = crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));

    size_t ret = (len - stream.avail_out) / nSize;
    if( z_err != Z_OK && z_err != Z_STREAM_END )
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

uLong VSIGZipHandle::getLong ()
{
    uLong x = static_cast<uLong>(get_byte()) & 0xFF;

    x += (static_cast<uLong>(get_byte()) & 0xFF) << 8;
    x += (static_cast<uLong>(get_byte()) & 0xFF) << 16;
    const int c = get_byte();
    if( c == EOF )
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

size_t VSIGZipHandle::Write( const void * /* pBuffer */,
                             size_t /* nSize */,
                             size_t /* nMemb */ )
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

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipWriteHandleMT                           */
/* ==================================================================== */
/************************************************************************/

class VSIGZipWriteHandleMT final : public VSIVirtualHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIGZipWriteHandleMT)

    VSIVirtualHandle*  poBaseHandle_ = nullptr;
    vsi_l_offset       nCurOffset_ = 0;
    uLong              nCRC_ = 0;
    int                nDeflateType_ = CPL_DEFLATE_TYPE_GZIP;
    bool               bAutoCloseBaseHandle_ = false;
    int                nThreads_ = 0;
    std::unique_ptr<CPLWorkerThreadPool> poPool_{};
    std::list<std::string*> aposBuffers_{};
    std::string*       pCurBuffer_ = nullptr;
    std::mutex         sMutex_{};
    int                nSeqNumberGenerated_ = 0;
    int                nSeqNumberExpected_ = 0;
    int                nSeqNumberExpectedCRC_ = 0;
    size_t             nChunkSize_ = 0;
    bool               bHasErrored_ = false;

    struct Job
    {
        VSIGZipWriteHandleMT *pParent_ = nullptr;
        std::string*       pBuffer_ = nullptr;
        int                nSeqNumber_ = 0;
        bool               bFinish_ = false;
        bool               bInCRCComputation_ = false;

        std::string        sCompressedData_{};
        uLong              nCRC_ = 0;
    };
    std::list<Job*>  apoFinishedJobs_{};
    std::list<Job*>  apoCRCFinishedJobs_{};
    std::list<Job*>  apoFreeJobs_{};

    static void DeflateCompress(void* inData);
    static void CRCCompute(void* inData);
    bool ProcessCompletedJobs();
    Job* GetJobObject();
#ifdef DEBUG_VERBOSE
    void DumpState();
#endif

  public:
    VSIGZipWriteHandleMT( VSIVirtualHandle* poBaseHandle,
                        int nThreads,
                        int nDeflateType,
                        bool bAutoCloseBaseHandleIn );

    ~VSIGZipWriteHandleMT() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

/************************************************************************/
/*                        VSIGZipWriteHandleMT()                        */
/************************************************************************/

VSIGZipWriteHandleMT::VSIGZipWriteHandleMT(  VSIVirtualHandle* poBaseHandle,
                        int nThreads,
                        int nDeflateType,
                        bool bAutoCloseBaseHandleIn ):
    poBaseHandle_(poBaseHandle),
    nDeflateType_(nDeflateType),
    bAutoCloseBaseHandle_(bAutoCloseBaseHandleIn),
    nThreads_(nThreads)
{
    const char* pszChunkSize = CPLGetConfigOption
        ("CPL_VSIL_DEFLATE_CHUNK_SIZE", "1024K");
    nChunkSize_ = static_cast<size_t>(atoi(pszChunkSize));
    if( strchr(pszChunkSize, 'K') )
        nChunkSize_ *= 1024;
    else if( strchr(pszChunkSize, 'M') )
        nChunkSize_ *= 1024 * 1024;
    nChunkSize_ = std::max(static_cast<size_t>(32 * 1024),
                    std::min(static_cast<size_t>(UINT_MAX), nChunkSize_));

    for( int i = 0; i < 1 + nThreads_; i++ )
        aposBuffers_.emplace_back( new std::string() );

    if( nDeflateType == CPL_DEFLATE_TYPE_GZIP )
    {
        char header[11] = {};

        // Write a very simple .gz header:
        snprintf( header, sizeof(header),
                    "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
                Z_DEFLATED, 0 /*flags*/, 0, 0, 0, 0 /*time*/, 0 /*xflags*/,
                0x03 );
        poBaseHandle_->Write( header, 1, 10 );
    }
}

/************************************************************************/
/*                       ~VSIGZipWriteHandleMT()                        */
/************************************************************************/

VSIGZipWriteHandleMT::~VSIGZipWriteHandleMT()

{
    VSIGZipWriteHandleMT::Close();
    for( auto& psJob: apoFinishedJobs_ )
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for( auto& psJob: apoCRCFinishedJobs_ )
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for( auto& psJob: apoFreeJobs_ )
    {
        delete psJob->pBuffer_;
        delete psJob;
    }
    for( auto& pstr: aposBuffers_ )
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
    if( !poBaseHandle_ )
        return 0;

    int nRet = 0;

    if( !pCurBuffer_ )
        pCurBuffer_ = new std::string();

    {
        auto psJob = GetJobObject();
        psJob->bFinish_ = true;
        psJob->pParent_ = this;
        psJob->pBuffer_ = pCurBuffer_;
        pCurBuffer_ = nullptr;
        psJob->nSeqNumber_ = nSeqNumberGenerated_;
        VSIGZipWriteHandleMT::DeflateCompress( psJob );
    }

    if( poPool_ )
    {
        poPool_->WaitCompletion(0);
    }
    if( !ProcessCompletedJobs() )
    {
        nRet = -1;
    }
    else
    {
        CPLAssert(apoFinishedJobs_.empty());
        if( nDeflateType_ == CPL_DEFLATE_TYPE_GZIP )
        {
            if( poPool_ )
            {
                poPool_->WaitCompletion(0);
            }
            ProcessCompletedJobs();
        }
        CPLAssert(apoCRCFinishedJobs_.empty());
    }

    if( nDeflateType_ == CPL_DEFLATE_TYPE_GZIP )
    {
        const GUInt32 anTrailer[2] = {
            CPL_LSBWORD32(static_cast<GUInt32>(nCRC_)),
            CPL_LSBWORD32(static_cast<GUInt32>(nCurOffset_))
        };

        if( poBaseHandle_->Write( anTrailer, 1, 8 ) < 8 )
        {
            nRet = -1;
        }
    }

    if( bAutoCloseBaseHandle_ )
    {
        int nRetClose = poBaseHandle_->Close();
        if( nRet == 0 )
            nRet = nRetClose;

        delete poBaseHandle_;
    }
    poBaseHandle_ = nullptr;

    return nRet;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIGZipWriteHandleMT::Read( void * /* pBuffer */,
                                 size_t /* nSize */,
                                 size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL is not supported on GZip write streams");
    return 0;
}

/************************************************************************/
/*                        DeflateCompress()                             */
/************************************************************************/

void VSIGZipWriteHandleMT::DeflateCompress(void* inData)
{
    Job* psJob = static_cast<Job*>(inData);

    CPLAssert( psJob->pBuffer_);

    z_stream           sStream;
    memset(&sStream, 0, sizeof(sStream));
    sStream.zalloc = nullptr;
    sStream.zfree = nullptr;
    sStream.opaque = nullptr;

    sStream.avail_in = static_cast<uInt>(psJob->pBuffer_->size());
    sStream.next_in = reinterpret_cast<Bytef*>(&(*psJob->pBuffer_)[0]);

    int ret = deflateInit2( &sStream, Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        (psJob->pParent_->nDeflateType_ == CPL_DEFLATE_TYPE_ZLIB) ?
            MAX_WBITS : -MAX_WBITS, 8,
        Z_DEFAULT_STRATEGY );
    CPLAssertAlwaysEval( ret == Z_OK );

    size_t nRealSize = 0;

    while( sStream.avail_in > 0 )
    {
        psJob->sCompressedData_.resize(nRealSize + Z_BUFSIZE);
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);
        sStream.next_out = reinterpret_cast<Bytef*>(
            &psJob->sCompressedData_[0]) + nRealSize;

        const int zlibRet = deflate( &sStream, Z_NO_FLUSH );
        CPLAssertAlwaysEval( zlibRet == Z_OK );

        nRealSize += static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;
    }

    psJob->sCompressedData_.resize(nRealSize + Z_BUFSIZE);
    sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);
    sStream.next_out = reinterpret_cast<Bytef*>(
        &psJob->sCompressedData_[0]) + nRealSize;

    // Do a Z_SYNC_FLUSH and Z_FULL_FLUSH, so as to have two markers when
    // independent as pigz 2.3.4 or later. The following 9 byte sequence will be
    // found: 0x00 0x00 0xff 0xff 0x00 0x00 0x00 0xff 0xff
    // Z_FULL_FLUSH only is sufficient, but it is not obvious if a
    // 0x00 0x00 0xff 0xff marker in the codestream is just a SYNC_FLUSH (
    // without dictionary reset) or a FULL_FLUSH (with dictionary reset)
    {
        const int zlibRet = deflate( &sStream, Z_SYNC_FLUSH );
        CPLAssertAlwaysEval( zlibRet == Z_OK );
    }

    {
        const int zlibRet = deflate( &sStream, Z_FULL_FLUSH );
        CPLAssertAlwaysEval( zlibRet == Z_OK );
    }

    if( psJob->bFinish_ )
    {
        const int zlibRet = deflate( &sStream, Z_FINISH );
        CPLAssertAlwaysEval( zlibRet == Z_STREAM_END );
    }

    nRealSize += static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;
    psJob->sCompressedData_.resize(nRealSize);

    deflateEnd( &sStream );

    {
        std::lock_guard<std::mutex> oLock(psJob->pParent_->sMutex_);
        psJob->pParent_->apoFinishedJobs_.push_back(psJob);
    }
}

/************************************************************************/
/*                          CRCCompute()                                */
/************************************************************************/

void VSIGZipWriteHandleMT::CRCCompute(void* inData)
{
    Job* psJob = static_cast<Job*>(inData);
    psJob->bInCRCComputation_ = true;
    psJob->nCRC_ = crc32(0U,
        reinterpret_cast<const Bytef*>(psJob->pBuffer_->data()),
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
    fprintf(stderr, "Finished jobs (expected = %d):\n", nSeqNumberExpected_); // ok
    for(const auto* psJob: apoFinishedJobs_ )
    {
        fprintf(stderr,  "seq number=%d, bInCRCComputation = %d\n",  // ok
                psJob->nSeqNumber_, psJob->bInCRCComputation_ ? 1 : 0);
    }
    fprintf(stderr, "Finished CRC jobs (expected = %d):\n",  // ok
            nSeqNumberExpectedCRC_);
    for(const auto* psJob: apoFinishedJobs_ )
    {
        fprintf(stderr,  "seq number=%d\n",  // ok
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
    while( do_it_again )
    {
        do_it_again = false;
        if( nDeflateType_ == CPL_DEFLATE_TYPE_GZIP )
        {
            for( auto iter = apoFinishedJobs_.begin();
                    iter != apoFinishedJobs_.end(); ++iter )
            {
                auto psJob = *iter;

                if( !psJob->bInCRCComputation_ )
                {
                    psJob->bInCRCComputation_ = true;
                    sMutex_.unlock();
                    if( poPool_ )
                    {
                        poPool_->SubmitJob( VSIGZipWriteHandleMT::CRCCompute,
                                            psJob );
                    }
                    else
                    {
                        CRCCompute(psJob);
                    }
                    sMutex_.lock();
                }
            }
        }

        for( auto iter = apoFinishedJobs_.begin();
                iter != apoFinishedJobs_.end(); ++iter )
        {
            auto psJob = *iter;
            if( psJob->nSeqNumber_ == nSeqNumberExpected_ )
            {
                apoFinishedJobs_.erase(iter);

                sMutex_.unlock();

                const size_t nToWrite = psJob->sCompressedData_.size();
                bool bError =
                    poBaseHandle_->Write( psJob->sCompressedData_.data(), 1,
                                          nToWrite) < nToWrite;
                sMutex_.lock();
                nSeqNumberExpected_ ++;

                if( nDeflateType_ != CPL_DEFLATE_TYPE_GZIP )
                {
                    aposBuffers_.push_back(psJob->pBuffer_);
                    psJob->pBuffer_ = nullptr;

                    apoFreeJobs_.push_back(psJob);
                }

                if( bError )
                {
                    return false;
                }

                do_it_again = true;
                break;
            }
        }

        if( nDeflateType_ == CPL_DEFLATE_TYPE_GZIP )
        {
            for( auto iter = apoCRCFinishedJobs_.begin();
                    iter != apoCRCFinishedJobs_.end(); ++iter )
            {
                auto psJob = *iter;
                if( psJob->nSeqNumber_ == nSeqNumberExpectedCRC_ )
                {
                    apoCRCFinishedJobs_.erase(iter);

                    nCRC_ = crc32_combine(nCRC_, psJob->nCRC_,
                                    static_cast<uLong>(psJob->pBuffer_->size()));

                    nSeqNumberExpectedCRC_ ++;

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

VSIGZipWriteHandleMT::Job* VSIGZipWriteHandleMT::GetJobObject()
{
    {
        std::lock_guard<std::mutex> oLock(sMutex_);
        if( !apoFreeJobs_.empty() )
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

size_t VSIGZipWriteHandleMT::Write( const void * const pBuffer,
                                    size_t const nSize, size_t const nMemb )

{
    if( bHasErrored_ )
        return 0;

    const char* pszBuffer = static_cast<const char*>(pBuffer);
    size_t nBytesToWrite = nSize * nMemb;
    while( nBytesToWrite > 0 )
    {
        if( pCurBuffer_ == nullptr )
        {
            while(true)
            {
                {
                    std::lock_guard<std::mutex> oLock(sMutex_);
                    if( !aposBuffers_.empty() )
                    {
                        pCurBuffer_ = aposBuffers_.back();
                        aposBuffers_.pop_back();
                        break;
                    }
                }
                if( poPool_ )
                {
                    poPool_->WaitEvent();
                }
                if( !ProcessCompletedJobs() )
                {
                    bHasErrored_ = true;
                    return 0;
                }
            }
            pCurBuffer_->clear();
        }
        size_t nConsumed = std::min( nBytesToWrite,
                                     nChunkSize_ - pCurBuffer_->size() );
        pCurBuffer_->append(pszBuffer, nConsumed);
        nCurOffset_ += nConsumed;
        pszBuffer += nConsumed;
        nBytesToWrite -= nConsumed;
        if( pCurBuffer_->size() == nChunkSize_ )
        {
            if( poPool_ == nullptr )
            {
                poPool_.reset(new CPLWorkerThreadPool());
                if( !poPool_->Setup(nThreads_, nullptr, nullptr, false) )
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
            nSeqNumberGenerated_ ++;
            pCurBuffer_ = nullptr;
            poPool_->SubmitJob( VSIGZipWriteHandleMT::DeflateCompress, psJob );
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

int VSIGZipWriteHandleMT::Seek( vsi_l_offset nOffset, int nWhence )

{
    if( nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR) )
        return 0;
    else if( nWhence == SEEK_SET && nOffset == nCurOffset_ )
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

    VSIVirtualHandle*  m_poBaseHandle = nullptr;
    z_stream           sStream;
    Byte              *pabyInBuf = nullptr;
    Byte              *pabyOutBuf = nullptr;
    bool               bCompressActive = false;
    vsi_l_offset       nCurOffset = 0;
    uLong              nCRC = 0;
    int                nDeflateType = CPL_DEFLATE_TYPE_GZIP;
    bool               bAutoCloseBaseHandle = false;

  public:
    VSIGZipWriteHandle( VSIVirtualHandle* poBaseHandle, int nDeflateType,
                        bool bAutoCloseBaseHandleIn );

    ~VSIGZipWriteHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;
};

/************************************************************************/
/*                         VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::VSIGZipWriteHandle( VSIVirtualHandle *poBaseHandle,
                                        int nDeflateTypeIn,
                                        bool bAutoCloseBaseHandleIn ) :
    m_poBaseHandle(poBaseHandle),
    sStream(),
    pabyInBuf(static_cast<Byte *>(CPLMalloc( Z_BUFSIZE ))),
    pabyOutBuf(static_cast<Byte *>(CPLMalloc( Z_BUFSIZE ))),
    nCRC(crc32(0L, nullptr, 0)),
    nDeflateType(nDeflateTypeIn),
    bAutoCloseBaseHandle(bAutoCloseBaseHandleIn)
{
    sStream.zalloc = nullptr;
    sStream.zfree = nullptr;
    sStream.opaque = nullptr;
    sStream.next_in = nullptr;
    sStream.next_out = nullptr;
    sStream.avail_in = sStream.avail_out = 0;

    sStream.next_in = pabyInBuf;

    if( deflateInit2( &sStream, Z_DEFAULT_COMPRESSION,
                      Z_DEFLATED,
                      ( nDeflateType == CPL_DEFLATE_TYPE_ZLIB) ?
                        MAX_WBITS : -MAX_WBITS, 8,
                      Z_DEFAULT_STRATEGY ) != Z_OK )
    {
        bCompressActive = false;
    }
    else
    {
        if( nDeflateType == CPL_DEFLATE_TYPE_GZIP )
        {
            char header[11] = {};

            // Write a very simple .gz header:
            snprintf( header, sizeof(header),
                      "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
                    Z_DEFLATED, 0 /*flags*/, 0, 0, 0, 0 /*time*/, 0 /*xflags*/,
                    0x03 );
            m_poBaseHandle->Write( header, 1, 10 );
        }

        bCompressActive = true;
    }
}

/************************************************************************/
/*                       VSICreateGZipWritable()                        */
/************************************************************************/

VSIVirtualHandle* VSICreateGZipWritable( VSIVirtualHandle* poBaseHandle,
                                         int nDeflateTypeIn,
                                         int bAutoCloseBaseHandle )
{
    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if( pszThreads )
    {
        int nThreads = 0;
        if( EQUAL(pszThreads, "ALL_CPUS") )
            nThreads = CPLGetNumCPUs();
        else
            nThreads = atoi(pszThreads);
        nThreads = std::max(1, std::min(128, nThreads));
        if( nThreads > 1 )
        {
            // coverity[tainted_data]
            return new VSIGZipWriteHandleMT( poBaseHandle,
                                                nThreads,
                                                nDeflateTypeIn,
                                                CPL_TO_BOOL(bAutoCloseBaseHandle) );
        }
    }
    return new VSIGZipWriteHandle( poBaseHandle,
                                   nDeflateTypeIn,
                                   CPL_TO_BOOL(bAutoCloseBaseHandle) );
}

/************************************************************************/
/*                        ~VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::~VSIGZipWriteHandle()

{
    if( bCompressActive )
        VSIGZipWriteHandle::Close();

    CPLFree( pabyInBuf );
    CPLFree( pabyOutBuf );
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int VSIGZipWriteHandle::Close()

{
    int nRet = 0;
    if( bCompressActive )
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);

        const int zlibRet = deflate( &sStream, Z_FINISH );
        CPLAssertAlwaysEval( zlibRet == Z_STREAM_END );

        const size_t nOutBytes =
            static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;

        deflateEnd( &sStream );

        if( m_poBaseHandle->Write( pabyOutBuf, 1, nOutBytes ) < nOutBytes )
        {
            nRet = -1;
        }

        if( nRet == 0 && nDeflateType == CPL_DEFLATE_TYPE_GZIP )
        {
            const GUInt32 anTrailer[2] = {
                CPL_LSBWORD32(static_cast<GUInt32>(nCRC)),
                CPL_LSBWORD32(static_cast<GUInt32>(nCurOffset))
            };

            if( m_poBaseHandle->Write( anTrailer, 1, 8 ) < 8 )
            {
                nRet = -1;
            }
        }

        if( bAutoCloseBaseHandle )
        {
            if( nRet == 0 )
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

size_t VSIGZipWriteHandle::Read( void * /* pBuffer */,
                                 size_t /* nSize */,
                                 size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL is not supported on GZip write streams");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Write( const void * const pBuffer,
                                  size_t const nSize, size_t const nMemb )

{
    size_t nBytesToWrite = nSize * nMemb;

    {
        size_t nOffset = 0;
        while( nOffset < nBytesToWrite )
        {
            uInt nChunk = static_cast<uInt>(
                std::min(static_cast<size_t>(UINT_MAX),
                         nBytesToWrite - nOffset));
            nCRC = crc32(nCRC,
                         reinterpret_cast<const Bytef *>(pBuffer) + nOffset,
                         nChunk);
            nOffset += nChunk;
        }
    }

    if( !bCompressActive )
        return 0;

    size_t nNextByte = 0;
    while( nNextByte < nBytesToWrite )
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);

        if( sStream.avail_in > 0 )
            memmove( pabyInBuf, sStream.next_in, sStream.avail_in );

        const uInt nNewBytesToWrite = static_cast<uInt>(std::min(
            static_cast<size_t>(Z_BUFSIZE-sStream.avail_in),
            nBytesToWrite - nNextByte));
        memcpy( pabyInBuf + sStream.avail_in,
                reinterpret_cast<const Byte *>(pBuffer) + nNextByte,
                nNewBytesToWrite );

        sStream.next_in = pabyInBuf;
        sStream.avail_in += nNewBytesToWrite;

        const int zlibRet = deflate( &sStream, Z_NO_FLUSH );
        CPLAssertAlwaysEval( zlibRet == Z_OK );

        const size_t nOutBytes =
            static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;

        if( nOutBytes > 0 )
        {
            if( m_poBaseHandle->Write( pabyOutBuf, 1, nOutBytes ) < nOutBytes )
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

int VSIGZipWriteHandle::Seek( vsi_l_offset nOffset, int nWhence )

{
    if( nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR) )
        return 0;
    else if( nWhence == SEEK_SET && nOffset == nCurOffset )
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
    if( poHandleLastGZipFile )
    {
        poHandleLastGZipFile->UnsetCanSaveInfo();
        delete poHandleLastGZipFile;
    }

    if( hMutex != nullptr )
        CPLDestroyMutex( hMutex );
    hMutex = nullptr;
}

/************************************************************************/
/*                            SaveInfo()                                */
/************************************************************************/

void VSIGZipFilesystemHandler::SaveInfo( VSIGZipHandle* poHandle )
{
    CPLMutexHolder oHolder(&hMutex);
    SaveInfo_unlocked(poHandle);
}

void VSIGZipFilesystemHandler::SaveInfo_unlocked( VSIGZipHandle* poHandle )
{
    if( m_bInSaveInfo )
        return;
    m_bInSaveInfo = true;

    CPLAssert( poHandle != poHandleLastGZipFile );
    CPLAssert(poHandle->GetBaseFileName() != nullptr);

    if( poHandleLastGZipFile == nullptr ||
        strcmp(poHandleLastGZipFile->GetBaseFileName(),
               poHandle->GetBaseFileName()) != 0 ||
        poHandle->GetLastReadOffset() >
            poHandleLastGZipFile->GetLastReadOffset() )
    {
        VSIGZipHandle* poTmp = poHandleLastGZipFile;
        poHandleLastGZipFile = nullptr;
        if( poTmp )
        {
            poTmp->UnsetCanSaveInfo();
            delete poTmp;
        }
        CPLAssert(poHandleLastGZipFile == nullptr);
        poHandleLastGZipFile = poHandle->Duplicate();
        if( poHandleLastGZipFile )
            poHandleLastGZipFile->CloseBaseHandle();
    }
    m_bInSaveInfo = false;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIGZipFilesystemHandler::Open( const char *pszFilename,
                                                  const char *pszAccess,
                                                  bool /* bSetError */,
                                                  CSLConstList /* papszOptions */ )
{
    if( !STARTS_WITH_CI(pszFilename, "/vsigzip/") )
        return nullptr;

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename + strlen("/vsigzip/"));

/* -------------------------------------------------------------------- */
/*      Is this an attempt to write a new file without update (w+)      */
/*      access?  If so, create a writable handle for the underlying     */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszAccess, 'w') != nullptr )
    {
        if( strchr(pszAccess, '+') != nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Write+update (w+) not supported for /vsigzip, "
                     "only read-only or write-only.");
            return nullptr;
        }

        VSIVirtualHandle* poVirtualHandle =
            poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "wb" );

        if( poVirtualHandle == nullptr )
            return nullptr;

        return VSICreateGZipWritable( poVirtualHandle,
                                       strchr(pszAccess, 'z') != nullptr,
                                       TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we are in the read access case.                       */
/* -------------------------------------------------------------------- */

    VSIGZipHandle* poGZIPHandle = OpenGZipReadOnly(pszFilename, pszAccess);
    if( poGZIPHandle )
        // Wrap the VSIGZipHandle inside a buffered reader that will
        // improve dramatically performance when doing small backward
        // seeks.
        return VSICreateBufferedReaderHandle(poGZIPHandle);

    return nullptr;
}

/************************************************************************/
/*                          OpenGZipReadOnly()                          */
/************************************************************************/

VSIGZipHandle* VSIGZipFilesystemHandler::OpenGZipReadOnly(
    const char *pszFilename, const char *pszAccess)
{
    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename + strlen("/vsigzip/"));

    CPLMutexHolder oHolder(&hMutex);

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Disable caching in fuzzing mode as the /vsigzip/ file is likely to
    // change very often
    // TODO: filename-based logic isn't enough. We should probably check
    // timestamp and/or file size.
    if( poHandleLastGZipFile != nullptr &&
        strcmp(pszFilename + strlen("/vsigzip/"),
               poHandleLastGZipFile->GetBaseFileName()) == 0 &&
        EQUAL(pszAccess, "rb") )
    {
        VSIGZipHandle* poHandle = poHandleLastGZipFile->Duplicate();
        if( poHandle )
            return poHandle;
    }
#else
    CPL_IGNORE_RET_VAL(pszAccess);
#endif

    VSIVirtualHandle* poVirtualHandle =
        poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "rb" );

    if( poVirtualHandle == nullptr )
        return nullptr;

    unsigned char signature[2] = { '\0', '\0' };
    if( VSIFReadL(signature, 1, 2, reinterpret_cast<VSILFILE*>(poVirtualHandle)) != 2 ||
        signature[0] != gz_magic[0] || signature[1] != gz_magic[1] )
    {
        poVirtualHandle->Close();
        delete poVirtualHandle;
        return nullptr;
    }

    if( poHandleLastGZipFile )
    {
        poHandleLastGZipFile->UnsetCanSaveInfo();
        delete poHandleLastGZipFile;
        poHandleLastGZipFile = nullptr;
    }

    VSIGZipHandle* poHandle =
        new VSIGZipHandle(poVirtualHandle, pszFilename + strlen("/vsigzip/"));
    if( !(poHandle->IsInitOK()) )
    {
        delete poHandle;
        return nullptr;
    }
    return poHandle;
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Stat( const char *pszFilename,
                                    VSIStatBufL *pStatBuf,
                                    int nFlags )
{
    if( !STARTS_WITH_CI(pszFilename, "/vsigzip/") )
        return -1;

    CPLMutexHolder oHolder(&hMutex);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if( poHandleLastGZipFile != nullptr &&
        strcmp(pszFilename+strlen("/vsigzip/"),
               poHandleLastGZipFile->GetBaseFileName()) == 0 )
    {
        if( poHandleLastGZipFile->GetUncompressedSize() != 0 )
        {
            pStatBuf->st_mode = S_IFREG;
            pStatBuf->st_size = poHandleLastGZipFile->GetUncompressedSize();
            return 0;
        }
    }

    // Begin by doing a stat on the real file.
    int ret = VSIStatExL(pszFilename+strlen("/vsigzip/"), pStatBuf, nFlags);

    if( ret == 0 && (nFlags & VSI_STAT_SIZE_FLAG) )
    {
        CPLString osCacheFilename(pszFilename + strlen("/vsigzip/"));
        osCacheFilename += ".properties";

        // Can we save a bit of seeking by using a .properties file?
        VSILFILE* fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "rb");
        if( fpCacheLength )
        {
            const char* pszLine;
            GUIntBig nCompressedSize = 0;
            GUIntBig nUncompressedSize = 0;
            while( (pszLine = CPLReadLineL(fpCacheLength)) != nullptr )
            {
                if( STARTS_WITH_CI(pszLine, "compressed_size=") )
                {
                    const char* pszBuffer =
                        pszLine + strlen("compressed_size=");
                    nCompressedSize =
                        CPLScanUIntBig(pszBuffer,
                                       static_cast<int>(strlen(pszBuffer)));
                }
                else if( STARTS_WITH_CI(pszLine, "uncompressed_size=") )
                {
                    const char* pszBuffer =
                        pszLine + strlen("uncompressed_size=");
                    nUncompressedSize =
                        CPLScanUIntBig(pszBuffer,
                                       static_cast<int>(strlen(pszBuffer)));
                }
            }

            CPL_IGNORE_RET_VAL(VSIFCloseL(fpCacheLength));

            if( nCompressedSize == static_cast<GUIntBig>(pStatBuf->st_size) )
            {
                // Patch with the uncompressed size.
                pStatBuf->st_size = nUncompressedSize;

                VSIGZipHandle* poHandle =
                    VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename,
                                                               "rb");
                if( poHandle )
                {
                    poHandle->SetUncompressedSize(nUncompressedSize);
                    SaveInfo_unlocked(poHandle);
                    delete poHandle;
                }

                return ret;
            }
        }

        // No, then seek at the end of the data (slow).
        VSIGZipHandle* poHandle =
                VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename, "rb");
        if( poHandle )
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

int VSIGZipFilesystemHandler::Unlink( const char * /* pszFilename */ )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIGZipFilesystemHandler::Rename( const char * /* oldpath */,
                                      const char * /* newpath */ )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Mkdir( const char * /* pszDirname */,
                                     long /* nMode */ )
{
    return -1;
}
/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Rmdir( const char * /* pszDirname */ )
{
    return -1;
}

/************************************************************************/
/*                             ReadDirEx()                                */
/************************************************************************/

char** VSIGZipFilesystemHandler::ReadDirEx( const char * /*pszDirname*/,
                                            int /* nMaxFiles */ )
{
    return nullptr;
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSIGZipFilesystemHandler::GetOptions()
{
    return
    "<Options>"
    "  <Option name='GDAL_NUM_THREADS' type='string' "
        "description='Number of threads for compression. Either a integer or ALL_CPUS'/>"
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
    VSIFileManager::InstallHandler( "/vsigzip/", new VSIGZipFilesystemHandler );
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

        explicit VSIZipEntryFileOffset( unz_file_pos file_pos ):
            m_file_pos()
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
    explicit VSIZipReader( const char* pszZipFileName );
    ~VSIZipReader() override;

    int IsValid() { return unzF != nullptr; }

    unzFile GetUnzFileHandle() { return unzF; }

    int GotoFirstFile() override;
    int GotoNextFile() override;
    VSIArchiveEntryFileOffset* GetFileOffset() override
        { return new VSIZipEntryFileOffset(file_pos); }
    GUIntBig GetFileSize() override { return nNextFileSize; }
    CPLString GetFileName() override { return osNextFileName; }
    GIntBig GetModifiedTime() override { return nModifiedTime; }
    int GotoFileOffset( VSIArchiveEntryFileOffset* pOffset ) override;
};

/************************************************************************/
/*                           VSIZipReader()                             */
/************************************************************************/

VSIZipReader::VSIZipReader( const char* pszZipFileName ):
    unzF(cpl_unzOpen(pszZipFileName)),
    file_pos()
{
    file_pos.pos_in_zip_directory = 0;
    file_pos.num_of_file = 0;
}

/************************************************************************/
/*                          ~VSIZipReader()                             */
/************************************************************************/

VSIZipReader::~VSIZipReader()
{
    if( unzF )
        cpl_unzClose(unzF);
}

/************************************************************************/
/*                              SetInfo()                               */
/************************************************************************/

bool VSIZipReader::SetInfo()
{
    char fileName[8193] = {};
    unz_file_info file_info;
    if( UNZ_OK !=
        cpl_unzGetCurrentFileInfo(
            unzF, &file_info, fileName, sizeof(fileName) - 1,
            nullptr, 0, nullptr, 0))
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
    if( cpl_unzGoToNextFile(unzF) != UNZ_OK )
        return FALSE;

    if( !SetInfo() )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSIZipReader::GotoFirstFile()
{
    if( cpl_unzGoToFirstFile(unzF) != UNZ_OK )
        return FALSE;

    if( !SetInfo() )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSIZipReader::GotoFileOffset( VSIArchiveEntryFileOffset* pOffset )
{
    VSIZipEntryFileOffset* pZipEntryOffset =
                        reinterpret_cast<VSIZipEntryFileOffset*>(pOffset);
    if( cpl_unzGoToFilePos(unzF, &(pZipEntryOffset->m_file_pos)) != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GotoFileOffset failed");
        return FALSE;
    }

    if( !SetInfo() )
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

    std::map<CPLString, VSIZipWriteHandle*> oMapZipWriteHandles{};
    VSIVirtualHandle *OpenForWrite_unlocked( const char *pszFilename,
                                            const char *pszAccess );

  public:
    VSIZipFilesystemHandler() = default;
    ~VSIZipFilesystemHandler() override;

    const char* GetPrefix() override { return "/vsizip"; }
    std::vector<CPLString> GetExtensions() override;
    VSIArchiveReader* CreateReader( const char* pszZipFileName ) override;

    VSIVirtualHandle *Open( const char *pszFilename,
                            const char *pszAccess,
                            bool bSetError,
                            CSLConstList /* papszOptions */ ) override;

    VSIVirtualHandle *OpenForWrite( const char *pszFilename,
                                    const char *pszAccess );

    int Mkdir( const char *pszDirname, long nMode ) override;
    char **ReadDirEx( const char *pszDirname, int nMaxFiles ) override;
    int Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
              int nFlags ) override;

    const char* GetOptions() override;

    void RemoveFromMap( VSIZipWriteHandle* poHandle );
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
   void                    *m_hZIP = nullptr;
   VSIZipWriteHandle       *poChildInWriting = nullptr;
   VSIZipWriteHandle       *m_poParent = nullptr;
   bool                     bAutoDeleteParent = false;
   vsi_l_offset             nCurOffset = 0;

  public:
    VSIZipWriteHandle( VSIZipFilesystemHandler* poFS,
                       void *hZIP,
                       VSIZipWriteHandle* poParent );

    ~VSIZipWriteHandle() override;

    int Seek( vsi_l_offset nOffset, int nWhence ) override;
    vsi_l_offset Tell() override;
    size_t Read( void *pBuffer, size_t nSize, size_t nMemb ) override;
    size_t Write( const void *pBuffer, size_t nSize, size_t nMemb ) override;
    int Eof() override;
    int Flush() override;
    int Close() override;

    void StartNewFile( VSIZipWriteHandle* poSubFile );
    void StopCurrentFile();
    void* GetHandle() { return m_hZIP; }
    VSIZipWriteHandle* GetChildInWriting() { return poChildInWriting; }
    void SetAutoDeleteParent() { bAutoDeleteParent = true; }
};

/************************************************************************/
/*                      ~VSIZipFilesystemHandler()                      */
/************************************************************************/

VSIZipFilesystemHandler::~VSIZipFilesystemHandler()
{
    for( std::map<CPLString, VSIZipWriteHandle*>::const_iterator iter =
             oMapZipWriteHandles.begin();
         iter != oMapZipWriteHandles.end();
         ++iter )
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
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", nullptr);
    if( pszAllowedExtensions )
    {
        char** papszExtensions =
            CSLTokenizeString2(pszAllowedExtensions, ", ", 0);
        for( int i = 0; papszExtensions[i] != nullptr; i++ )
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

VSIArchiveReader* VSIZipFilesystemHandler::CreateReader(
    const char* pszZipFileName )
{
    VSIZipReader* poReader = new VSIZipReader(pszZipFileName);

    if( !poReader->IsValid() )
    {
        delete poReader;
        return nullptr;
    }

    if( !poReader->GotoFirstFile() )
    {
        delete poReader;
        return nullptr;
    }

    return poReader;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle* VSIZipFilesystemHandler::Open( const char *pszFilename,
                                                 const char *pszAccess,
                                                 bool /* bSetError */,
                                                 CSLConstList /* papszOptions */ )
{

    if( strchr(pszAccess, 'w') != nullptr )
    {
        return OpenForWrite(pszFilename, pszAccess);
    }

    if( strchr(pszAccess, '+') != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read-write random access not supported for /vsizip");
        return nullptr;
    }

    CPLString osZipInFileName;
    char* zipFilename = SplitFilename(pszFilename, osZipInFileName, TRUE);
    if( zipFilename == nullptr )
        return nullptr;

    {
        CPLMutexHolder oHolder(&hMutex);
        if( oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return nullptr;
        }
    }

    VSIArchiveReader* poReader = OpenArchiveFile(zipFilename, osZipInFileName);
    if( poReader == nullptr )
    {
        CPLFree(zipFilename);
        return nullptr;
    }

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( zipFilename);

    VSIVirtualHandle* poVirtualHandle =
        poFSHandler->Open( zipFilename, "rb" );

    CPLFree(zipFilename);
    zipFilename = nullptr;

    if( poVirtualHandle == nullptr )
    {
        delete poReader;
        return nullptr;
    }

    unzFile unzF = reinterpret_cast<VSIZipReader*>(poReader)->
                                                            GetUnzFileHandle();

    if( cpl_unzOpenCurrentFile(unzF) != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzOpenCurrentFile() failed");
        delete poReader;
        delete poVirtualHandle;
        return nullptr;
    }

    uLong64 pos = cpl_unzGetCurrentFileZStreamPos(unzF);

    unz_file_info file_info;
    if( cpl_unzGetCurrentFileInfo (unzF, &file_info, nullptr, 0, nullptr, 0, nullptr, 0)
        != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzGetCurrentFileInfo() failed");
        cpl_unzCloseCurrentFile(unzF);
        delete poReader;
        delete poVirtualHandle;
        return nullptr;
    }

    cpl_unzCloseCurrentFile(unzF);

    delete poReader;

    VSIGZipHandle* poGZIPHandle =
        new VSIGZipHandle(poVirtualHandle,
                          nullptr,
                          pos,
                          file_info.compressed_size,
                          file_info.uncompressed_size,
                          file_info.crc,
                          file_info.compression_method == 0);
    if( !(poGZIPHandle->IsInitOK()) )
    {
        delete poGZIPHandle;
        return nullptr;
    }

    // Wrap the VSIGZipHandle inside a buffered reader that will
    // improve dramatically performance when doing small backward
    // seeks.
    return VSICreateBufferedReaderHandle(poGZIPHandle);
}

/************************************************************************/
/*                                Mkdir()                               */
/************************************************************************/

int VSIZipFilesystemHandler::Mkdir( const char *pszDirname,
                                    long /* nMode */ )
{
    CPLString osDirname = pszDirname;
    if( !osDirname.empty() && osDirname.back() != '/' )
        osDirname += "/";
    VSIVirtualHandle* poZIPHandle = OpenForWrite(osDirname, "wb");
    if( poZIPHandle == nullptr )
        return -1;
    delete poZIPHandle;
    return 0;
}

/************************************************************************/
/*                               ReadDirEx()                            */
/************************************************************************/

char **VSIZipFilesystemHandler::ReadDirEx( const char *pszDirname,
                                           int nMaxFiles )
{
    CPLString osInArchiveSubDir;
    char* zipFilename = SplitFilename(pszDirname, osInArchiveSubDir, TRUE);
    if( zipFilename == nullptr )
        return nullptr;

    {
        CPLMutexHolder oHolder(&hMutex);

        if( oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
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

int VSIZipFilesystemHandler::Stat( const char *pszFilename,
                                   VSIStatBufL *pStatBuf, int nFlags )
{
    CPLString osInArchiveSubDir;

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    char* zipFilename = SplitFilename(pszFilename, osInArchiveSubDir, TRUE);
    if( zipFilename == nullptr )
        return -1;

    {
        CPLMutexHolder oHolder(&hMutex);

        if( oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
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

void VSIZipFilesystemHandler::RemoveFromMap( VSIZipWriteHandle* poHandle )
{
    CPLMutexHolder oHolder( &hMutex );

    for( std::map<CPLString, VSIZipWriteHandle*>::iterator iter =
             oMapZipWriteHandles.begin();
         iter != oMapZipWriteHandles.end();
         ++iter )
    {
        if( iter->second == poHandle )
        {
            oMapZipWriteHandles.erase(iter);
            break;
        }
    }
}

/************************************************************************/
/*                             OpenForWrite()                           */
/************************************************************************/

VSIVirtualHandle *
VSIZipFilesystemHandler::OpenForWrite( const char *pszFilename,
                                       const char *pszAccess )
{
    CPLMutexHolder oHolder( &hMutex );
    return OpenForWrite_unlocked(pszFilename, pszAccess);
}

VSIVirtualHandle *
VSIZipFilesystemHandler::OpenForWrite_unlocked( const char *pszFilename,
                                                const char *pszAccess )
{
    CPLString osZipInFileName;

    char* zipFilename = SplitFilename(pszFilename, osZipInFileName, FALSE);
    if( zipFilename == nullptr )
        return nullptr;
    CPLString osZipFilename = zipFilename;
    CPLFree(zipFilename);
    zipFilename = nullptr;

    // Invalidate cached file list.
    std::map<CPLString,VSIArchiveContent*>::iterator iter =
        oFileList.find(osZipFilename);
    if( iter != oFileList.end() )
    {
        delete iter->second;

        oFileList.erase(iter);
    }

    if( oMapZipWriteHandles.find(osZipFilename) != oMapZipWriteHandles.end() )
    {
        if( strchr(pszAccess, '+') != nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Random access not supported for writable file in /vsizip");
            return nullptr;
        }

        VSIZipWriteHandle* poZIPHandle = oMapZipWriteHandles[osZipFilename];

        if( poZIPHandle->GetChildInWriting() != nullptr )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Cannot create %s while another file is being "
                "written in the .zip",
                osZipInFileName.c_str());
            return nullptr;
        }

        poZIPHandle->StopCurrentFile();

        // Re-add path separator when creating directories.
        char chLastChar = pszFilename[strlen(pszFilename) - 1];
        if( chLastChar == '/' || chLastChar == '\\' )
            osZipInFileName += chLastChar;

        if( CPLCreateFileInZip(poZIPHandle->GetHandle(),
                               osZipInFileName, nullptr) != CE_None )
            return nullptr;

        VSIZipWriteHandle* poChildHandle =
            new VSIZipWriteHandle(this, nullptr, poZIPHandle);

        poZIPHandle->StartNewFile(poChildHandle);

        return poChildHandle;
    }
    else
    {
        char** papszOptions = nullptr;
        if( (strchr(pszAccess, '+') && osZipInFileName.empty()) ||
             !osZipInFileName.empty() )
        {
            VSIStatBufL sBuf;
            if( VSIStatExL(osZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0 )
                papszOptions = CSLAddNameValue(papszOptions, "APPEND", "TRUE");
        }

        void* hZIP = CPLCreateZip(osZipFilename, papszOptions);
        CSLDestroy(papszOptions);

        if( hZIP == nullptr )
            return nullptr;

        auto poHandle = new VSIZipWriteHandle(this, hZIP, nullptr);
        oMapZipWriteHandles[osZipFilename] = poHandle;

        if( !osZipInFileName.empty() )
        {
            VSIZipWriteHandle* poRes = reinterpret_cast<VSIZipWriteHandle*>(
                OpenForWrite_unlocked(pszFilename, pszAccess));
            if( poRes == nullptr )
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

const char* VSIZipFilesystemHandler::GetOptions()
{
    return
    "<Options>"
    "  <Option name='GDAL_NUM_THREADS' type='string' "
        "description='Number of threads for compression. Either a integer or ALL_CPUS'/>"
    "  <Option name='CPL_VSIL_DEFLATE_CHUNK_SIZE' type='string' "
        "description='Chunk of uncompressed data for parallelization. "
        "Use K(ilobytes) or M(egabytes) suffix' default='1M'/>"
    "</Options>";
}


/************************************************************************/
/*                          VSIZipWriteHandle()                         */
/************************************************************************/

VSIZipWriteHandle::VSIZipWriteHandle( VSIZipFilesystemHandler* poFS,
                                      void* hZIP,
                                      VSIZipWriteHandle* poParent ) :
    m_poFS(poFS),
    m_hZIP(hZIP),
    m_poParent(poParent)
{}

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

int VSIZipWriteHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    if( nOffset == 0 && (nWhence == SEEK_END || nWhence == SEEK_CUR) )
        return 0;
    if( nOffset == nCurOffset && nWhence == SEEK_SET )
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

size_t VSIZipWriteHandle::Read( void * /* pBuffer */,
                                size_t /* nSize */,
                                size_t /* nMemb */ )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL() is not supported on writable Zip files");
    return 0;
}

/************************************************************************/
/*                               Write()                                 */
/************************************************************************/

size_t VSIZipWriteHandle::Write( const void *pBuffer, size_t nSize,
                                 size_t nMemb )
{
    if( m_poParent == nullptr )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "VSIFWriteL() is not supported on "
            "main Zip file or closed subfiles");
        return 0;
    }

    const GByte* pabyBuffer = static_cast<const GByte*>(pBuffer);
    size_t nBytesToWrite = nSize * nMemb;
    size_t nWritten = 0;
    while( nWritten < nBytesToWrite )
    {
        int nToWrite = static_cast<int>(
            std::min( static_cast<size_t>(INT_MAX), nBytesToWrite ));
        if( CPLWriteFileInZip( m_poParent->m_hZIP, pabyBuffer,
            nToWrite ) != CE_None )
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
    if( m_poParent )
    {
        CPLCloseFileInZip(m_poParent->m_hZIP);
        m_poParent->poChildInWriting = nullptr;
        if( bAutoDeleteParent )
        {
            if( m_poParent->Close() != 0 )
                nRet = -1;
            delete m_poParent;
        }
        m_poParent = nullptr;
    }
    if( poChildInWriting )
    {
        if( poChildInWriting->Close() != 0 )
            nRet = -1;
        poChildInWriting = nullptr;
    }
    if( m_hZIP )
    {
        if( CPLCloseZip(m_hZIP) != CE_None )
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
    if( poChildInWriting )
        poChildInWriting->Close();
    poChildInWriting = nullptr;
}

/************************************************************************/
/*                           StartNewFile()                             */
/************************************************************************/

void VSIZipWriteHandle::StartNewFile( VSIZipWriteHandle* poSubFile )
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
    VSIFileManager::InstallHandler( "/vsizip/", new VSIZipFilesystemHandler() );
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

void* CPLZLibDeflate( const void* ptr,
                      size_t nBytes,
                      int nLevel,
                      void* outptr,
                      size_t nOutAvailableBytes,
                      size_t* pnOutBytes )
{
    if( pnOutBytes != nullptr )
        *pnOutBytes = 0;

    size_t nTmpSize = 0;
    void* pTmp;
#ifdef HAVE_LIBDEFLATE
    struct libdeflate_compressor* enc = libdeflate_alloc_compressor(nLevel < 0 ? 7 : nLevel);
    if( enc == nullptr )
    {
        return nullptr;
    }
#endif
    if( outptr == nullptr )
    {
#ifdef HAVE_LIBDEFLATE
        nTmpSize = libdeflate_zlib_compress_bound(enc, nBytes);
#else
        nTmpSize = 32 + nBytes * 2;
#endif
        pTmp = VSIMalloc(nTmpSize);
        if( pTmp == nullptr )
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
    size_t nCompressedBytes = libdeflate_zlib_compress(
                    enc, ptr, nBytes, pTmp, nTmpSize);
    libdeflate_free_compressor(enc);
    if( nCompressedBytes == 0 )
    {
        if( pTmp != outptr )
            VSIFree(pTmp);
        return nullptr;
    }
    if( pnOutBytes != nullptr )
        *pnOutBytes = nCompressedBytes;
#else
    z_stream strm;
    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    int ret = deflateInit(&strm, nLevel < 0 ? Z_DEFAULT_COMPRESSION : nLevel);
    if( ret != Z_OK )
    {
        if( pTmp != outptr )
            VSIFree(pTmp);
        return nullptr;
    }

    strm.avail_in = static_cast<uInt>(nBytes);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(ptr));
    strm.avail_out = static_cast<uInt>(nTmpSize);
    strm.next_out = reinterpret_cast<Bytef*>(pTmp);
    ret = deflate(&strm, Z_FINISH);
    if( ret != Z_STREAM_END )
    {
        if( pTmp != outptr )
            VSIFree(pTmp);
        return nullptr;
    }
    if( pnOutBytes != nullptr )
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

void* CPLZLibInflate( const void* ptr, size_t nBytes,
                      void* outptr, size_t nOutAvailableBytes,
                      size_t* pnOutBytes )
{
    if( pnOutBytes != nullptr )
        *pnOutBytes = 0;

#ifdef HAVE_LIBDEFLATE
    if( outptr )
    {
        struct libdeflate_decompressor* dec = libdeflate_alloc_decompressor();
        if( dec == nullptr )
        {
            return nullptr;
        }
        enum libdeflate_result res;
        if( nBytes > 2 &&
            static_cast<const GByte*>(ptr)[0] == 0x1F &&
            static_cast<const GByte*>(ptr)[1] == 0x8B )
        {
            res = libdeflate_gzip_decompress(
                dec, ptr, nBytes, outptr, nOutAvailableBytes, pnOutBytes);
        }
        else
        {
            res = libdeflate_zlib_decompress(
                dec, ptr, nBytes, outptr, nOutAvailableBytes, pnOutBytes);
        }
        libdeflate_free_decompressor(dec);
        if( res != LIBDEFLATE_SUCCESS )
        {
            return nullptr;
        }
        return outptr;
    }
#endif

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = static_cast<uInt>(nBytes);
    strm.next_in = static_cast<Bytef*>(const_cast<void*>(ptr));
    int ret;
    // MAX_WBITS + 32 mode which detects automatically gzip vs zlib encapsulation
    // seems to be broken with /opt/intel/oneapi/intelpython/latest/lib/libz.so.1
    // from intel/oneapi-basekit Docker image
    if(  nBytes > 2 &&
         static_cast<const GByte*>(ptr)[0] == 0x1F &&
         static_cast<const GByte*>(ptr)[1] == 0x8B )
    {
        ret = inflateInit2(&strm, MAX_WBITS + 16); // gzip
    }
    else
    {
        ret = inflateInit2(&strm, MAX_WBITS); // zlib
    }
    if( ret != Z_OK )
    {
        return nullptr;
    }

    size_t nTmpSize = 0;
    char* pszTmp = nullptr;
#ifndef HAVE_LIBDEFLATE
    if( outptr == nullptr )
#endif
    {
        nTmpSize = 2 * nBytes;
        pszTmp = static_cast<char *>(VSIMalloc(nTmpSize + 1));
        if( pszTmp == nullptr )
        {
            inflateEnd(&strm);
            return nullptr;
        }
    }
#ifndef HAVE_LIBDEFLATE
    else
    {
        pszTmp = static_cast<char *>(outptr);
        nTmpSize = nOutAvailableBytes;
    }
#endif

    strm.avail_out = static_cast<uInt>(nTmpSize);
    strm.next_out = reinterpret_cast<Bytef *>(pszTmp);

    while( true )
    {
        ret = inflate(&strm, Z_FINISH);
        if( ret == Z_BUF_ERROR )
        {
#ifndef HAVE_LIBDEFLATE
            if( outptr == pszTmp )
            {
                inflateEnd(&strm);
                return nullptr;
            }
#endif

            size_t nAlreadyWritten = nTmpSize - strm.avail_out;
            nTmpSize = nTmpSize * 2;
            char* pszTmpNew =
                static_cast<char *>(VSIRealloc(pszTmp, nTmpSize + 1));
            if( pszTmpNew == nullptr )
            {
                VSIFree(pszTmp);
                inflateEnd(&strm);
                return nullptr;
            }
            pszTmp = pszTmpNew;
            strm.avail_out = static_cast<uInt>(nTmpSize - nAlreadyWritten);
            strm.next_out = reinterpret_cast<Bytef*>(pszTmp + nAlreadyWritten);
        }
        else
            break;
    }

    if( ret == Z_OK || ret == Z_STREAM_END )
    {
        size_t nOutBytes = nTmpSize - strm.avail_out;
        // Nul-terminate if possible.
#ifndef HAVE_LIBDEFLATE
        if( outptr != pszTmp || nOutBytes < nTmpSize )
#endif
        {
            pszTmp[nOutBytes] = '\0';
        }
        inflateEnd(&strm);
        if( pnOutBytes != nullptr )
            *pnOutBytes = nOutBytes;
        return pszTmp;
    }
    else
    {
#ifndef HAVE_LIBDEFLATE
        if( outptr != pszTmp )
#endif
        {
            VSIFree(pszTmp);
        }
        inflateEnd(&strm);
        return nullptr;
    }
}
