/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for gz/zip files (.gz and .zip).
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <zlib.h>

#include <algorithm>
#include <map>
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


CPL_CVSID("$Id$")

static const int Z_BUFSIZE = 65536;  // Original size is 16384
static const int gz_magic[2] = {0x1f, 0x8b};  // gzip magic header

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

class VSIGZipHandle CPL_FINAL : public VSIVirtualHandle
{
    VSIVirtualHandle* m_poBaseHandle;
#ifdef DEBUG
    vsi_l_offset      m_offset;
#endif
    vsi_l_offset      m_compressed_size;
    vsi_l_offset      m_uncompressed_size;
    vsi_l_offset      offsetEndCompressedData;
    uLong             m_expected_crc;
    char             *m_pszBaseFileName; /* optional */
    bool              m_bCanSaveInfo;

    /* Fields from gz_stream structure */
    z_stream stream;
    int      z_err;   /* error code for last stream operation */
    int      z_eof;   /* set if end of input file (but not necessarily of the uncompressed stream ! "in" must be null too ) */
    Byte     *inbuf;  /* input buffer */
    Byte     *outbuf; /* output buffer */
    uLong    crc;     /* crc32 of uncompressed data */
    int      m_transparent; /* 1 if input file is not a .gz file */
    vsi_l_offset  startOff;   /* startOff of compressed data in file (header skipped) */
    vsi_l_offset  in;      /* bytes into deflate or inflate */
    vsi_l_offset  out;     /* bytes out of deflate or inflate */
    vsi_l_offset  m_nLastReadOffset;

    GZipSnapshot* snapshots;
    vsi_l_offset snapshot_byte_interval; /* number of compressed bytes at which we create a "snapshot" */

    void check_header();
    int get_byte();
    int gzseek( vsi_l_offset nOffset, int nWhence );
    int gzrewind ();
    uLong getLong ();

  public:

    VSIGZipHandle( VSIVirtualHandle* poBaseHandle,
                   const char* pszBaseFileName,
                   vsi_l_offset offset = 0,
                   vsi_l_offset compressed_size = 0,
                   vsi_l_offset uncompressed_size = 0,
                   uLong expected_crc = 0,
                   int transparent = 0 );
    virtual ~VSIGZipHandle();

    bool              IsInitOK() const { return inbuf != NULL; }

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb )
        override;
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb )
        override;
    virtual int       Eof() override;
    virtual int       Flush() override;
    virtual int       Close() override;

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

class VSIGZipFilesystemHandler CPL_FINAL : public VSIFilesystemHandler
{
    CPLMutex* hMutex;
    VSIGZipHandle* poHandleLastGZipFile;
    bool           m_bInSaveInfo;

public:
    VSIGZipFilesystemHandler();
    virtual ~VSIGZipFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;
    VSIGZipHandle *OpenGZipReadOnly( const char *pszFilename,
                                     const char *pszAccess );
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;
    virtual int      Unlink( const char *pszFilename ) override;
    virtual int      Rename( const char *oldpath,
                             const char *newpath ) override;
    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual int      Rmdir( const char *pszDirname ) override;
    virtual char   **ReadDirEx( const char *pszDirname,
                                int nMaxFiles ) override;

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
    CPLAssert (m_pszBaseFileName != NULL);

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( m_pszBaseFileName );

    VSIVirtualHandle* poNewBaseHandle =
        poFSHandler->Open( m_pszBaseFileName, "rb" );

    if( poNewBaseHandle == NULL )
        return NULL;

    VSIGZipHandle* poHandle = new VSIGZipHandle(poNewBaseHandle,
                                                m_pszBaseFileName,
                                                0,
                                                m_compressed_size,
                                                m_uncompressed_size);
    if( !(poHandle->IsInitOK()) )
    {
        delete poHandle;
        return NULL;
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
        bRet = VSIFCloseL((VSILFILE*)m_poBaseHandle) == 0;
    m_poBaseHandle = NULL;
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
    m_expected_crc(expected_crc),
    m_pszBaseFileName(pszBaseFileName ? CPLStrdup(pszBaseFileName) : NULL),
    m_bCanSaveInfo(true),
    z_err(Z_OK),
    z_eof(0),
    outbuf(NULL),
    crc(crc32(0L, NULL, 0)),
    m_transparent(transparent),
    startOff(0),
    in(0),
    out(0),
    m_nLastReadOffset(0),
    snapshots(NULL),
    snapshot_byte_interval(0)
{
    if( compressed_size || transparent )
    {
        m_compressed_size = compressed_size;
    }
    else
    {
        if( VSIFSeekL((VSILFILE*)poBaseHandle, 0, SEEK_END) != 0 )
            CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
        m_compressed_size = VSIFTellL((VSILFILE*)poBaseHandle) - offset;
        compressed_size = m_compressed_size;
    }
    m_uncompressed_size = uncompressed_size;
    offsetEndCompressedData = offset + compressed_size;

    if( VSIFSeekL((VSILFILE*)poBaseHandle, offset, SEEK_SET) != 0 )
        CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");

    stream.zalloc = (alloc_func)NULL;
    stream.zfree = (free_func)NULL;
    stream.opaque = (voidpf)NULL;
    stream.next_in = inbuf = NULL;
    stream.next_out = outbuf = NULL;
    stream.avail_in = stream.avail_out = 0;

    inbuf = static_cast<Byte *>(ALLOC(Z_BUFSIZE));
    stream.next_in = inbuf;

    int err = inflateInit2(&(stream), -MAX_WBITS);
    // windowBits is passed < 0 to tell that there is no zlib header.
    // Note that in this case inflate *requires* an extra "dummy" byte
    // after the compressed stream in order to complete decompression and
    // return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
    // present after the compressed stream.
    if( err != Z_OK || inbuf == NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "inflateInit2 init failed");
        TRYFREE(inbuf);
        inbuf = NULL;
        return;
    }
    stream.avail_out = static_cast<uInt>(Z_BUFSIZE);

    if( offset == 0 ) check_header();  // Skip the .gz header.
    startOff = VSIFTellL((VSILFILE*)poBaseHandle) - stream.avail_in;

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

    if( stream.state != NULL )
    {
        inflateEnd(&(stream));
    }

    TRYFREE(inbuf);
    TRYFREE(outbuf);

    if( snapshots != NULL )
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
        CPL_IGNORE_RET_VAL(VSIFCloseL((VSILFILE*)m_poBaseHandle));
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
                      (VSILFILE*)m_poBaseHandle));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 VSIFTellL((VSILFILE*)m_poBaseHandle),
                 offsetEndCompressedData);
#endif
        if( VSIFTellL((VSILFILE*)m_poBaseHandle) > offsetEndCompressedData )
        {
            len = len + static_cast<uInt>(
                offsetEndCompressedData - VSIFTellL((VSILFILE*)m_poBaseHandle));
            if( VSIFSeekL((VSILFILE*)m_poBaseHandle,
                          offsetEndCompressedData, SEEK_SET) != 0 )
                z_err = Z_DATA_ERROR;
        }
        if( len == 0 )  // && ferror(file)
        {
            if( VSIFTellL((VSILFILE*)m_poBaseHandle) !=
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
        len = static_cast<uInt>(get_byte());
        len += static_cast<uInt>(get_byte()) << 8;
        // len is garbage if EOF but the loop below will quit anyway.
        while( len-- != 0 && get_byte() != EOF ) {}
    }

    int c = 0;
    if( (flags & ORIG_NAME) != 0 )
    {
        // Skip the original file name.
        while( (c = get_byte()) != 0 && c != EOF ) {}
    }
    if( (flags & COMMENT) != 0 )
    {
        // skip the .gz file comment.
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
                      (VSILFILE*)m_poBaseHandle));
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                 VSIFTellL((VSILFILE*)m_poBaseHandle),
                 offsetEndCompressedData);
#endif
        if( VSIFTellL((VSILFILE*)m_poBaseHandle) > offsetEndCompressedData )
        {
            stream.avail_in =
                stream.avail_in +
                static_cast<uInt>(
                    offsetEndCompressedData -
                    VSIFTellL((VSILFILE*)m_poBaseHandle));
            if( VSIFSeekL((VSILFILE*)m_poBaseHandle,
                          offsetEndCompressedData, SEEK_SET) != 0 )
                return EOF;
        }
        if( stream.avail_in == 0 ) {
            z_eof = 1;
            if( VSIFTellL((VSILFILE*)m_poBaseHandle) !=
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
    crc = crc32(0L, NULL, 0);
    if( !m_transparent )
        CPL_IGNORE_RET_VAL(inflateReset(&stream));
    in = 0;
    out = 0;
    return VSIFSeekL((VSILFILE*)m_poBaseHandle, startOff, SEEK_SET);
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSIGZipHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    /* The semantics of gzseek are different from ::Seek */
    /* It returns the current offset, where as ::Seek should return 0 */
    /* if successful */
    int ret = gzseek(nOffset, nWhence);
    return (ret >= 0) ? 0 : ret;
}

/************************************************************************/
/*                            gzseek()                                  */
/************************************************************************/

int VSIGZipHandle::gzseek( vsi_l_offset offset, int whence )
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
                CPL_VSIL_GZ_RETURN(-1);
                return -1L;
            }

            offset = startOff + out + offset;
        }
        else if( whence == SEEK_SET )
        {
            if( offset > m_compressed_size )
            {
                CPL_VSIL_GZ_RETURN(-1);
                return -1L;
            }

            offset = startOff + offset;
        }
        else if( whence == SEEK_END )
        {
            // Commented test: because vsi_l_offset is unsigned (for the moment)
            // so no way to seek backward. See #1590 */
            if( offset > 0 ) // || -offset > compressed_size
            {
                CPL_VSIL_GZ_RETURN(-1);
                return -1L;
            }

            offset = startOff + m_compressed_size - offset;
        }
        else
        {
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
        }

        if( VSIFSeekL((VSILFILE*)m_poBaseHandle, offset, SEEK_SET) < 0 )
        {
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
        }

        out = offset - startOff;
        in = out;
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", "return " CPL_FRMT_GUIB, in);
#endif
        return in > INT_MAX ? INT_MAX : static_cast<int>(in);
    }

    // whence == SEEK_END is unsuppored in original gzseek.
    if( whence == SEEK_END )
    {
        // If we known the uncompressed size, we can fake a jump to
        // the end of the stream.
        if( offset == 0 && m_uncompressed_size != 0 )
        {
            out = m_uncompressed_size;
            return 1;
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

    if( // whence == SEEK_END ||
        z_err == Z_ERRNO || z_err == Z_DATA_ERROR )
    {
        CPL_VSIL_GZ_RETURN(-1);
        return -1L;
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
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
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
            if( VSIFSeekL((VSILFILE*)m_poBaseHandle,
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

    if( offset != 0 && outbuf == NULL )
    {
        outbuf = static_cast<Byte*>(ALLOC(Z_BUFSIZE));
        if( outbuf == NULL )
        {
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
        }
    }

    if( original_nWhence == SEEK_END && z_err == Z_STREAM_END )
    {
#ifdef ENABLE_DEBUG
        CPLDebug("GZIP", "gzseek return " CPL_FRMT_GUIB, out);
#endif
        return static_cast<int>(out);
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
            // CPL_VSIL_GZ_RETURN(-1);
            return -1L;
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
    CPLDebug("GZIP", "gzseek return " CPL_FRMT_GUIB, out);
#endif

    if( original_offset == 0 && original_nWhence == SEEK_END )
    {
        m_uncompressed_size = out;

        if( m_pszBaseFileName &&
            !STARTS_WITH_CI(m_pszBaseFileName, "/vsicurl/") &&
            CPLTestBool(CPLGetConfigOption("CPL_VSIL_GZIP_WRITE_PROPERTIES", "YES")) )
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

    return static_cast<int>(out);
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

    if( z_err == Z_DATA_ERROR || z_err == Z_ERRNO )
    {
        z_eof = 1;  // To avoid infinite loop in reader code.
        in = 0;
        CPL_VSIL_GZ_RETURN(0);
        return 0;
    }
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
    Bytef *pStart = (Bytef*)buf;  // Start off point for crc computation.
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
                    VSIFReadL(next_out, 1, nToRead, (VSILFILE*)m_poBaseHandle));
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
                VSIFTellL((VSILFILE*)m_poBaseHandle);
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
                VSIFReadL(inbuf, 1, Z_BUFSIZE, (VSILFILE*)m_poBaseHandle));
#ifdef ENABLE_DEBUG
            CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                     VSIFTellL((VSILFILE*)m_poBaseHandle),
                     offsetEndCompressedData);
#endif
            if( VSIFTellL((VSILFILE*)m_poBaseHandle) > offsetEndCompressedData )
            {
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
#endif
                stream.avail_in =
                    stream.avail_in -
                    static_cast<uInt>(VSIFTellL((VSILFILE*)m_poBaseHandle) -
                                      offsetEndCompressedData);
                if( VSIFSeekL((VSILFILE*)m_poBaseHandle,
                              offsetEndCompressedData, SEEK_SET) != 0 )
                    CPLError(CE_Failure, CPLE_FileIO, "Seek() failed");
#ifdef ENABLE_DEBUG
                CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
#endif
            }
            if( stream.avail_in == 0 )
            {
                z_eof = 1;
                if( VSIFTellL((VSILFILE*)m_poBaseHandle) !=
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
                        crc = crc32(0L, NULL, 0);
                    }
                }
            }
        }
        if( z_err != Z_OK || z_eof )
            break;
    }
    crc = crc32(crc, pStart, static_cast<uInt>(stream.next_out - pStart));

    if( len == stream.avail_out &&
        (z_err == Z_DATA_ERROR || z_err == Z_ERRNO || z_err == Z_BUF_ERROR) )
    {
        z_eof = 1;
        in = 0;
        CPL_VSIL_GZ_RETURN(0);
        return 0;
    }
#ifdef ENABLE_DEBUG
    CPLDebug("GZIP", "Read return %d (z_err=%d, z_eof=%d)",
             static_cast<int>((len - stream.avail_out) / nSize),
             z_err, z_eof);
#endif
    return static_cast<int>(len - stream.avail_out) / nSize;
}

/************************************************************************/
/*                              getLong()                               */
/************************************************************************/

uLong VSIGZipHandle::getLong ()
{
    uLong x = static_cast<uLong>(get_byte());

    x += static_cast<uLong>(get_byte()) << 8;
    x += static_cast<uLong>(get_byte()) << 16;
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
/*                       VSIGZipWriteHandle                             */
/* ==================================================================== */
/************************************************************************/

class VSIGZipWriteHandle CPL_FINAL : public VSIVirtualHandle
{
    VSIVirtualHandle*  m_poBaseHandle;
    z_stream           sStream;
    Byte              *pabyInBuf;
    Byte              *pabyOutBuf;
    bool               bCompressActive;
    vsi_l_offset       nCurOffset;
    uLong              nCRC;
    bool               bRegularZLib;
    bool               bAutoCloseBaseHandle;

  public:
    VSIGZipWriteHandle( VSIVirtualHandle* poBaseHandle, bool bRegularZLib,
                        bool bAutoCloseBaseHandleIn );

    virtual ~VSIGZipWriteHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb )
        override;
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb )
        override;
    virtual int       Eof() override;
    virtual int       Flush() override;
    virtual int       Close() override;
};

/************************************************************************/
/*                         VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::VSIGZipWriteHandle( VSIVirtualHandle *poBaseHandle,
                                        bool bRegularZLibIn,
                                        bool bAutoCloseBaseHandleIn ) :
    m_poBaseHandle(poBaseHandle),
    pabyInBuf(static_cast<Byte *>(CPLMalloc( Z_BUFSIZE ))),
    pabyOutBuf(static_cast<Byte *>(CPLMalloc( Z_BUFSIZE ))),
    bCompressActive(false),
    nCurOffset(0),
    nCRC(crc32(0L, NULL, 0)),
    bRegularZLib(bRegularZLibIn),
    bAutoCloseBaseHandle(bAutoCloseBaseHandleIn)
{
    sStream.zalloc = (alloc_func)NULL;
    sStream.zfree = (free_func)NULL;
    sStream.opaque = (voidpf)NULL;
    sStream.next_in = NULL;
    sStream.next_out = NULL;
    sStream.avail_in = sStream.avail_out = 0;

    sStream.next_in = pabyInBuf;

    if( deflateInit2( &sStream, Z_DEFAULT_COMPRESSION,
                      Z_DEFLATED, bRegularZLib ? MAX_WBITS : -MAX_WBITS, 8,
                      Z_DEFAULT_STRATEGY ) != Z_OK )
    {
        bCompressActive = false;
    }
    else
    {
        if( !bRegularZLib )
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
                                         int bRegularZLibIn,
                                         int bAutoCloseBaseHandle )
{
    return new VSIGZipWriteHandle( poBaseHandle,
                                   CPL_TO_BOOL(bRegularZLibIn),
                                   CPL_TO_BOOL(bAutoCloseBaseHandle) );
}

/************************************************************************/
/*                        ~VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::~VSIGZipWriteHandle()

{
    if( bCompressActive )
        Close();

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

        CPL_IGNORE_RET_VAL( deflate( &sStream, Z_FINISH ) );

        const size_t nOutBytes =
            static_cast<uInt>(Z_BUFSIZE) - sStream.avail_out;

        if( m_poBaseHandle->Write( pabyOutBuf, 1, nOutBytes ) < nOutBytes )
            return EOF;

        deflateEnd( &sStream );

        if( !bRegularZLib )
        {
            const GUInt32 anTrailer[2] = {
                CPL_LSBWORD32(static_cast<GUInt32>(nCRC)),
                CPL_LSBWORD32(static_cast<GUInt32>(nCurOffset))
            };

            m_poBaseHandle->Write( anTrailer, 1, 8 );
        }

        if( bAutoCloseBaseHandle )
        {
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
    int nBytesToWrite = static_cast<int>(nSize * nMemb);
    int nNextByte = 0;

    nCRC = crc32(nCRC, (const Bytef *)pBuffer, nBytesToWrite);

    if( !bCompressActive )
        return 0;

    while( nNextByte < nBytesToWrite )
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = static_cast<uInt>(Z_BUFSIZE);

        if( sStream.avail_in > 0 )
            memmove( pabyInBuf, sStream.next_in, sStream.avail_in );

        const int nNewBytesToWrite = std::min(
            static_cast<int>(Z_BUFSIZE-sStream.avail_in),
            nBytesToWrite - nNextByte);
        memcpy( pabyInBuf + sStream.avail_in,
                ((Byte *) pBuffer) + nNextByte,
                nNewBytesToWrite );

        sStream.next_in = pabyInBuf;
        sStream.avail_in += nNewBytesToWrite;

        CPL_IGNORE_RET_VAL( deflate( &sStream, Z_NO_FLUSH ) );

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
/*                   VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::VSIGZipFilesystemHandler()
{
    hMutex = NULL;

    poHandleLastGZipFile = NULL;
    m_bInSaveInfo = false;
}

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

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
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
    CPLAssert(poHandle->GetBaseFileName() != NULL);

    if( poHandleLastGZipFile == NULL ||
        strcmp(poHandleLastGZipFile->GetBaseFileName(),
               poHandle->GetBaseFileName()) != 0 ||
        poHandle->GetLastReadOffset() >
            poHandleLastGZipFile->GetLastReadOffset() )
    {
        VSIGZipHandle* poTmp = poHandleLastGZipFile;
        poHandleLastGZipFile = NULL;
        if( poTmp )
        {
            poTmp->UnsetCanSaveInfo();
            delete poTmp;
        }
        CPLAssert(poHandleLastGZipFile == NULL);
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
                                                  bool /* bSetError */ )
{
    if( !STARTS_WITH_CI(pszFilename, "/vsigzip/") )
        return NULL;

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( pszFilename + strlen("/vsigzip/"));

/* -------------------------------------------------------------------- */
/*      Is this an attempt to write a new file without update (w+)      */
/*      access?  If so, create a writable handle for the underlying     */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszAccess, 'w') != NULL )
    {
        if( strchr(pszAccess, '+') != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Write+update (w+) not supported for /vsigzip, "
                     "only read-only or write-only.");
            return NULL;
        }

        VSIVirtualHandle* poVirtualHandle =
            poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "wb" );

        if( poVirtualHandle == NULL )
            return NULL;

        return new VSIGZipWriteHandle( poVirtualHandle,
                                       strchr(pszAccess, 'z') != NULL,
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

    return NULL;
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
    if( poHandleLastGZipFile != NULL &&
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

    if( poVirtualHandle == NULL )
        return NULL;

    unsigned char signature[2] = { '\0', '\0' };
    if( VSIFReadL(signature, 1, 2, (VSILFILE*)poVirtualHandle) != 2 ||
        signature[0] != gz_magic[0] || signature[1] != gz_magic[1] )
    {
        poVirtualHandle->Close();
        delete poVirtualHandle;
        return NULL;
    }

    if( poHandleLastGZipFile )
    {
        poHandleLastGZipFile->UnsetCanSaveInfo();
        delete poHandleLastGZipFile;
        poHandleLastGZipFile = NULL;
    }

    VSIGZipHandle* poHandle =
        new VSIGZipHandle(poVirtualHandle, pszFilename + strlen("/vsigzip/"));
    if( !(poHandle->IsInitOK()) )
    {
        delete poHandle;
        return NULL;
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

    if( poHandleLastGZipFile != NULL &&
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
            while( (pszLine = CPLReadLineL(fpCacheLength)) != NULL )
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
    return NULL;
}

//! @endcond
/************************************************************************/
/*                   VSIInstallGZipFileHandler()                        */
/************************************************************************/

/**
 * \brief Install GZip file system handler.
 *
 * A special file handler is installed that allows reading on-the-fly and
 * writing in GZip (.gz) files.
 *
 * All portions of the file system underneath the base
 * path "/vsigzip/" will be handled by this driver.
 *
 * Additional documentation is to be found at:
 * http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
 *
 * @since GDAL 1.6.0
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

class VSIZipEntryFileOffset CPL_FINAL : public VSIArchiveEntryFileOffset
{
public:
        unz_file_pos m_file_pos;

        explicit VSIZipEntryFileOffset( unz_file_pos file_pos )
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

class VSIZipReader CPL_FINAL : public VSIArchiveReader
{
    private:
        unzFile unzF;
        unz_file_pos file_pos;
        GUIntBig nNextFileSize;
        CPLString osNextFileName;
        GIntBig nModifiedTime;

        void SetInfo();

    public:
        explicit VSIZipReader( const char* pszZipFileName );
        virtual ~VSIZipReader();

        int IsValid() { return unzF != NULL; }

        unzFile GetUnzFileHandle() { return unzF; }

        virtual int GotoFirstFile() override;
        virtual int GotoNextFile() override;
        virtual VSIArchiveEntryFileOffset* GetFileOffset() override
            { return new VSIZipEntryFileOffset(file_pos); }
        virtual GUIntBig GetFileSize() override { return nNextFileSize; }
        virtual CPLString GetFileName() override { return osNextFileName; }
        virtual GIntBig GetModifiedTime() override { return nModifiedTime; }
        virtual int GotoFileOffset( VSIArchiveEntryFileOffset* pOffset )
            override;
};

/************************************************************************/
/*                           VSIZipReader()                             */
/************************************************************************/

VSIZipReader::VSIZipReader( const char* pszZipFileName ) :
    nNextFileSize(0),
    nModifiedTime(0)
{
    unzF = cpl_unzOpen(pszZipFileName);
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

void VSIZipReader::SetInfo()
{
    char fileName[8193] = {};
    unz_file_info file_info;
    cpl_unzGetCurrentFileInfo( unzF, &file_info, fileName, sizeof(fileName) - 1,
                               NULL, 0, NULL, 0 );
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
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSIZipReader::GotoNextFile()
{
    if( cpl_unzGoToNextFile(unzF) != UNZ_OK )
        return FALSE;

    SetInfo();

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSIZipReader::GotoFirstFile()
{
    if( cpl_unzGoToFirstFile(unzF) != UNZ_OK )
        return FALSE;

    SetInfo();

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

    SetInfo();

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*                       VSIZipFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

class VSIZipWriteHandle;

class VSIZipFilesystemHandler CPL_FINAL : public VSIArchiveFilesystemHandler
{
    std::map<CPLString, VSIZipWriteHandle*> oMapZipWriteHandles;
    VSIVirtualHandle *OpenForWrite_unlocked( const char *pszFilename,
                                            const char *pszAccess );

public:
    virtual ~VSIZipFilesystemHandler();

    virtual const char* GetPrefix() override { return "/vsizip"; }
    virtual std::vector<CPLString> GetExtensions() override;
    virtual VSIArchiveReader* CreateReader( const char* pszZipFileName )
        override;

    virtual VSIVirtualHandle *Open( const char *pszFilename,
                                    const char *pszAccess,
                                    bool bSetError ) override;

    virtual VSIVirtualHandle *OpenForWrite( const char *pszFilename,
                                            const char *pszAccess );

    virtual int      Mkdir( const char *pszDirname, long nMode ) override;
    virtual char   **ReadDirEx( const char *pszDirname, int nMaxFiles )
        override;
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf,
                           int nFlags ) override;

    void RemoveFromMap( VSIZipWriteHandle* poHandle );
};

/************************************************************************/
/* ==================================================================== */
/*                       VSIZipWriteHandle                              */
/* ==================================================================== */
/************************************************************************/

class VSIZipWriteHandle CPL_FINAL : public VSIVirtualHandle
{
   VSIZipFilesystemHandler *m_poFS;
   void                    *m_hZIP;
   VSIZipWriteHandle       *poChildInWriting;
   VSIZipWriteHandle       *m_poParent;
   bool                     bAutoDeleteParent;
   vsi_l_offset             nCurOffset;

  public:
    VSIZipWriteHandle( VSIZipFilesystemHandler* poFS,
                       void *hZIP,
                       VSIZipWriteHandle* poParent );

    virtual ~VSIZipWriteHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence ) override;
    virtual vsi_l_offset Tell() override;
    virtual size_t    Read( void *pBuffer, size_t nSize,
                            size_t nMemb ) override;
    virtual size_t    Write( const void *pBuffer, size_t nSize,
                             size_t nMemb ) override;
    virtual int       Eof() override;
    virtual int       Flush() override;
    virtual int       Close() override;

    void  StartNewFile( VSIZipWriteHandle* poSubFile );
    void  StopCurrentFile();
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

    // Add to zip FS handler extensions array additional extensions
    // listed in CPL_VSIL_ZIP_ALLOWED_EXTENSIONS config option.
    // The extensions are divided by commas.
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", NULL);
    if( pszAllowedExtensions )
    {
        char** papszExtensions =
            CSLTokenizeString2(pszAllowedExtensions, ", ", 0);
        for( int i = 0; papszExtensions[i] != NULL; i++ )
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
        return NULL;
    }

    if( !poReader->GotoFirstFile() )
    {
        delete poReader;
        return NULL;
    }

    return poReader;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle* VSIZipFilesystemHandler::Open( const char *pszFilename,
                                                 const char *pszAccess,
                                                 bool /* bSetError */ )
{

    if( strchr(pszAccess, 'w') != NULL )
    {
        return OpenForWrite(pszFilename, pszAccess);
    }

    if( strchr(pszAccess, '+') != NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Random access not supported for /vsizip");
        return NULL;
    }

    CPLString osZipInFileName;
    char* zipFilename = SplitFilename(pszFilename, osZipInFileName, TRUE);
    if( zipFilename == NULL )
        return NULL;

    {
        CPLMutexHolder oHolder(&hMutex);
        if( oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return NULL;
        }
    }

    VSIArchiveReader* poReader = OpenArchiveFile(zipFilename, osZipInFileName);
    if( poReader == NULL )
    {
        CPLFree(zipFilename);
        return NULL;
    }

    VSIFilesystemHandler *poFSHandler =
        VSIFileManager::GetHandler( zipFilename);

    VSIVirtualHandle* poVirtualHandle =
        poFSHandler->Open( zipFilename, "rb" );

    CPLFree(zipFilename);
    zipFilename = NULL;

    if( poVirtualHandle == NULL )
    {
        delete poReader;
        return NULL;
    }

    unzFile unzF = reinterpret_cast<VSIZipReader*>(poReader)->
                                                            GetUnzFileHandle();

    if( cpl_unzOpenCurrentFile(unzF) != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzOpenCurrentFile() failed");
        delete poReader;
        return NULL;
    }

    uLong64 pos = cpl_unzGetCurrentFileZStreamPos(unzF);

    unz_file_info file_info;
    if( cpl_unzGetCurrentFileInfo (unzF, &file_info, NULL, 0, NULL, 0, NULL, 0)
        != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "cpl_unzGetCurrentFileInfo() failed");
        cpl_unzCloseCurrentFile(unzF);
        delete poReader;
        return NULL;
    }

    cpl_unzCloseCurrentFile(unzF);

    delete poReader;

    VSIGZipHandle* poGZIPHandle =
        new VSIGZipHandle(poVirtualHandle,
                          NULL,
                          pos,
                          file_info.compressed_size,
                          file_info.uncompressed_size,
                          file_info.crc,
                          file_info.compression_method == 0);
    if( !(poGZIPHandle->IsInitOK()) )
    {
        delete poGZIPHandle;
        return NULL;
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
    if( poZIPHandle == NULL )
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
    if( zipFilename == NULL )
        return NULL;

    {
        CPLMutexHolder oHolder(&hMutex);

        if( oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return NULL;
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
    if( zipFilename == NULL )
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
    if( zipFilename == NULL )
        return NULL;
    CPLString osZipFilename = zipFilename;
    CPLFree(zipFilename);
    zipFilename = NULL;

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
        if( strchr(pszAccess, '+') != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Random access not supported for writable file in /vsizip");
            return NULL;
        }

        VSIZipWriteHandle* poZIPHandle = oMapZipWriteHandles[osZipFilename];

        if( poZIPHandle->GetChildInWriting() != NULL )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Cannot create %s while another file is being "
                "written in the .zip",
                osZipInFileName.c_str());
            return NULL;
        }

        poZIPHandle->StopCurrentFile();

        // Re-add path separator when creating directories.
        char chLastChar = pszFilename[strlen(pszFilename) - 1];
        if( chLastChar == '/' || chLastChar == '\\' )
            osZipInFileName += chLastChar;

        if( CPLCreateFileInZip(poZIPHandle->GetHandle(),
                               osZipInFileName, NULL) != CE_None )
            return NULL;

        VSIZipWriteHandle* poChildHandle =
            new VSIZipWriteHandle(this, NULL, poZIPHandle);

        poZIPHandle->StartNewFile(poChildHandle);

        return poChildHandle;
    }
    else
    {
        char** papszOptions = NULL;
        if( (strchr(pszAccess, '+') && osZipInFileName.empty()) ||
             !osZipInFileName.empty() )
        {
            VSIStatBufL sBuf;
            if( VSIStatExL(osZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0 )
                papszOptions = CSLAddNameValue(papszOptions, "APPEND", "TRUE");
        }

        void* hZIP = CPLCreateZip(osZipFilename, papszOptions);
        CSLDestroy(papszOptions);

        if( hZIP == NULL )
            return NULL;

        oMapZipWriteHandles[osZipFilename] =
            new VSIZipWriteHandle(this, hZIP, NULL);

        if( !osZipInFileName.empty() )
        {
            VSIZipWriteHandle* poRes = reinterpret_cast<VSIZipWriteHandle*>(
                OpenForWrite_unlocked(pszFilename, pszAccess));
            if( poRes == NULL )
            {
                delete oMapZipWriteHandles[osZipFilename];
                return NULL;
            }

            poRes->SetAutoDeleteParent();

            return poRes;
        }

        return oMapZipWriteHandles[osZipFilename];
    }
}

/************************************************************************/
/*                          VSIZipWriteHandle()                         */
/************************************************************************/

VSIZipWriteHandle::VSIZipWriteHandle( VSIZipFilesystemHandler* poFS,
                                      void* hZIP,
                                      VSIZipWriteHandle* poParent ) :
    m_poFS(poFS),
    m_hZIP(hZIP),
    poChildInWriting(NULL),
    m_poParent(poParent),
    bAutoDeleteParent(false),
    nCurOffset(0)
{}

/************************************************************************/
/*                         ~VSIZipWriteHandle()                         */
/************************************************************************/

VSIZipWriteHandle::~VSIZipWriteHandle()
{
    Close();
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
    if( m_poParent == NULL )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "VSIFWriteL() is not supported on "
            "main Zip file or closed subfiles");
        return 0;
    }

    if( CPLWriteFileInZip( m_poParent->m_hZIP, pBuffer,
                           static_cast<int>(nSize * nMemb) ) != CE_None )
        return 0;

    nCurOffset += static_cast<int>(nSize * nMemb);

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
    if( m_poParent )
    {
        CPLCloseFileInZip(m_poParent->m_hZIP);
        m_poParent->poChildInWriting = NULL;
        if( bAutoDeleteParent )
            delete m_poParent;
        m_poParent = NULL;
    }
    if( poChildInWriting )
    {
        poChildInWriting->Close();
        poChildInWriting = NULL;
    }
    if( m_hZIP )
    {
        CPLCloseZip(m_hZIP);
        m_hZIP = NULL;

        m_poFS->RemoveFromMap(this);
    }

    return 0;
}

/************************************************************************/
/*                           StopCurrentFile()                          */
/************************************************************************/

void VSIZipWriteHandle::StopCurrentFile()
{
    if( poChildInWriting )
        poChildInWriting->Close();
    poChildInWriting = NULL;
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

/**
 * \brief Install ZIP file system handler.
 *
 * A special file handler is installed that allows reading on-the-fly in ZIP
 * (.zip) archives.
 *
 * All portions of the file system underneath the base path "/vsizip/" will be
 * handled by this driver.
 *
 * The syntax to open a file inside a zip file is
 * /vsizip/path/to/the/file.zip/path/inside/the/zip/file were
 * path/to/the/file.zip is relative or absolute and path/inside/the/zip/file is
 * the relative path to the file inside the archive.
 *
 * Starting with GDAL 2.2, an alternate syntax is available so as to enable
 * chaining and not being dependent on .zip extension :
 * /vsizip/{/path/to/the/archive}/path/inside/the/zip/file.
 * Note that /path/to/the/archive may also itself this alternate syntax.
 *
 * If the path is absolute, it should begin with a / on a Unix-like OS (or C:\
 * on Windows), so the line looks like /vsizip//home/gdal/...  For example
 * gdalinfo /vsizip/myarchive.zip/subdir1/file1.tif
 *
 * Syntactic sugar : if the .zip file contains only one file located at its
 * root, just mentioning "/vsizip/path/to/the/file.zip" will work
 *
 * VSIStatL() will return the uncompressed size in st_size member and file
 * nature- file or directory - in st_mode member.
 *
 * Directory listing is available through VSIReadDir().
 *
 * Since GDAL 1.8.0, write capabilities are available. They allow creating
 * a new zip file and adding new files to an already existing (or just created)
 * zip file. Read and write operations cannot be interleaved : the new zip must
 * be closed before being re-opened for read.
 *
 * Additional documentation is to be found at
 * http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
 *
 * @since GDAL 1.6.0
 */

void VSIInstallZipFileHandler()
{
    VSIFileManager::InstallHandler( "/vsizip/", new VSIZipFilesystemHandler() );
}

/************************************************************************/
/*                         CPLZLibDeflate()                             */
/************************************************************************/

/**
 * \brief Compress a buffer with ZLib DEFLATE compression.
 *
 * @param ptr input buffer.
 * @param nBytes size of input buffer in bytes.
 * @param nLevel ZLib compression level (-1 for default). Currently unused
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
                      CPL_UNUSED int nLevel,
                      void* outptr,
                      size_t nOutAvailableBytes,
                      size_t* pnOutBytes )
{
    z_stream strm;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;
    int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if( ret != Z_OK )
    {
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }

    size_t nTmpSize = 0;
    void* pTmp;
    if( outptr == NULL )
    {
        nTmpSize = 8 + nBytes * 2;
        pTmp = VSIMalloc(nTmpSize);
        if( pTmp == NULL )
        {
            deflateEnd(&strm);
            if( pnOutBytes != NULL )
                *pnOutBytes = 0;
            return NULL;
        }
    }
    else
    {
        pTmp = outptr;
        nTmpSize = nOutAvailableBytes;
    }

    strm.avail_in = static_cast<uInt>(nBytes);
    strm.next_in = (Bytef*) ptr;
    strm.avail_out = static_cast<uInt>(nTmpSize);
    strm.next_out = (Bytef*) pTmp;
    ret = deflate(&strm, Z_FINISH);
    if( ret != Z_STREAM_END )
    {
        if( pTmp != outptr )
            VSIFree(pTmp);
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }
    if( pnOutBytes != NULL )
        *pnOutBytes = nTmpSize - strm.avail_out;
    deflateEnd(&strm);
    return pTmp;
}

/************************************************************************/
/*                         CPLZLibInflate()                             */
/************************************************************************/

/**
 * \brief Uncompress a buffer compressed with ZLib DEFLATE compression.
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
    z_stream strm;
    strm.zalloc = NULL;
    strm.zfree = NULL;
    strm.opaque = NULL;
    strm.avail_in = static_cast<uInt>(nBytes);
    strm.next_in = (Bytef*) ptr;
    int ret = inflateInit(&strm);
    if( ret != Z_OK )
    {
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }

    size_t nTmpSize = 0;
    char* pszTmp = NULL;
    if( outptr == NULL )
    {
        nTmpSize = 2 * nBytes;
        pszTmp = static_cast<char *>(VSIMalloc(nTmpSize + 1));
        if( pszTmp == NULL )
        {
            inflateEnd(&strm);
            if( pnOutBytes != NULL )
                *pnOutBytes = 0;
            return NULL;
        }
    }
    else
    {
        pszTmp = static_cast<char *>(outptr);
        nTmpSize = nOutAvailableBytes;
    }

    strm.avail_out = static_cast<uInt>(nTmpSize);
    strm.next_out = reinterpret_cast<Bytef *>(pszTmp);

    while( true )
    {
        ret = inflate(&strm, Z_FINISH);
        if( ret == Z_BUF_ERROR )
        {
            if( outptr == pszTmp )
            {
                inflateEnd(&strm);
                if( pnOutBytes != NULL )
                    *pnOutBytes = 0;
                return NULL;
            }

            size_t nAlreadyWritten = nTmpSize - strm.avail_out;
            nTmpSize = nTmpSize * 2;
            char* pszTmpNew =
                static_cast<char *>(VSIRealloc(pszTmp, nTmpSize + 1));
            if( pszTmpNew == NULL )
            {
                VSIFree(pszTmp);
                inflateEnd(&strm);
                if( pnOutBytes != NULL )
                    *pnOutBytes = 0;
                return NULL;
            }
            pszTmp = pszTmpNew;
            strm.avail_out = static_cast<uInt>(nTmpSize - nAlreadyWritten);
            strm.next_out = (Bytef*) (pszTmp + nAlreadyWritten);
        }
        else
            break;
    }

    if( ret == Z_OK || ret == Z_STREAM_END )
    {
        size_t nOutBytes = nTmpSize - strm.avail_out;
        // Nul-terminate if possible.
        if( outptr != pszTmp || nOutBytes < nTmpSize )
            pszTmp[nOutBytes] = '\0';
        inflateEnd(&strm);
        if( pnOutBytes != NULL )
            *pnOutBytes = nOutBytes;
        return pszTmp;
    }
    else
    {
        if( outptr != pszTmp )
            VSIFree(pszTmp);
        inflateEnd(&strm);
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }
}
