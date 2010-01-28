/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for gz/zip files (.gz and .zip).
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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
   implemented in original gzSeek. It also implements a concept of in-memory "snapshots",
   that are a way of improving efficiency while seeking GZip files. Snapshots are
   ceated regularly when decompressing  the data a snapshot of the gzip state.
   Later we can seek directly in the compressed data to the closest snapshot in order to
   reduce the amount of data to uncompress again.

   For .gz files, an effort is done to cache the size of the uncompressed data in
   a .gz.properties file, so that we don't need to seek at the end of the file
   each time a Stat() is done.

   For .zip, only reading is supported, for .gz read and write is supported.
*/


#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include <map>

#include <zlib.h>
#include "cpl_minizip_unzip.h"

CPL_CVSID("$Id$");

#define Z_BUFSIZE 65536  /* original size is 16384 */
static int const gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

#define ALLOC(size) malloc(size)
#define TRYFREE(p) {if (p) free(p);}

#define CPL_VSIL_GZ_RETURN_MINUS_ONE()   \
        CPLError(CE_Failure, CPLE_AppDefined, "In file %s, at line %d, return -1", __FILE__, __LINE__)

#define ENABLE_DEBUG 0

/************************************************************************/
/* ==================================================================== */
/*                       VSIGZipHandle                                  */
/* ==================================================================== */
/************************************************************************/

typedef struct
{
    vsi_l_offset  uncompressed_pos;
    z_stream      stream;
    uLong         crc;
    int           transparent;
    vsi_l_offset  in;
    vsi_l_offset  out;
} GZipSnapshot;

class VSIGZipHandle : public VSIVirtualHandle
{
    VSIVirtualHandle* poBaseHandle;
    vsi_l_offset      compressed_size;
    vsi_l_offset      uncompressed_size;
    vsi_l_offset      offsetEndCompressedData;
    unsigned int      expected_crc;
    char             *pszOptionalFileName;

    /* Fields from gz_stream structure */
    z_stream stream;
    int      z_err;   /* error code for last stream operation */
    int      z_eof;   /* set if end of input file */
    Byte     *inbuf;  /* input buffer */
    Byte     *outbuf; /* output buffer */
    uLong    crc;     /* crc32 of uncompressed data */
    int      transparent; /* 1 if input file is not a .gz file */
    vsi_l_offset  startOff;   /* startOff of compressed data in file (header skipped) */
    vsi_l_offset  in;      /* bytes into deflate or inflate */
    vsi_l_offset  out;     /* bytes out of deflate or inflate */
    
    GZipSnapshot* snapshots;
    vsi_l_offset snapshot_byte_interval; /* number of compressed bytes at which we create a "snapshot" */

    void check_header();
    int get_byte();
    int gzseek( vsi_l_offset nOffset, int nWhence );
    int gzrewind ();
    uLong getLong ();

  public:

    VSIGZipHandle(VSIVirtualHandle* poBaseHandle,
                  const char* pszOptionalFileName,
                  vsi_l_offset offset = 0,
                  vsi_l_offset compressed_size = 0,
                  vsi_l_offset uncompressed_size = 0,
                  unsigned int expected_crc = 0,
                  int transparent = 0);
    ~VSIGZipHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();

    VSIGZipHandle*    Duplicate();
    void              CloseBaseHandle();
};

/************************************************************************/
/*                            Duplicate()                               */
/************************************************************************/

VSIGZipHandle* VSIGZipHandle::Duplicate()
{
    if (pszOptionalFileName == NULL)
        return NULL;

    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszOptionalFileName );

    VSIVirtualHandle* poNewBaseHandle =
        poFSHandler->Open( pszOptionalFileName, "rb" );

    if (poNewBaseHandle == NULL)
        return NULL;

    VSIGZipHandle* poHandle = new VSIGZipHandle(poNewBaseHandle,
                                                pszOptionalFileName);

    /* Most important : duplicate the snapshots ! */

    unsigned int i;
    for(i=0;i<compressed_size / snapshot_byte_interval + 1;i++)
    {
        if (snapshots[i].uncompressed_pos == 0)
            break;

        poHandle->snapshots[i].uncompressed_pos = snapshots[i].uncompressed_pos;
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

void  VSIGZipHandle::CloseBaseHandle()
{
    if (poBaseHandle)
        VSIFCloseL((FILE*)poBaseHandle);
    poBaseHandle = NULL;
}

/************************************************************************/
/*                       VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::VSIGZipHandle(VSIVirtualHandle* poBaseHandle,
                             const char* pszOptionalFileName,
                             vsi_l_offset offset,
                             vsi_l_offset compressed_size,
                             vsi_l_offset uncompressed_size,
                             unsigned int expected_crc,
                             int transparent)
{
    this->poBaseHandle = poBaseHandle;
    this->expected_crc = expected_crc;
    this->pszOptionalFileName = (pszOptionalFileName) ? CPLStrdup(pszOptionalFileName) : NULL;
    if (compressed_size)
    {
        this->compressed_size = compressed_size;
    }
    else
    {
        VSIFSeekL((FILE*)poBaseHandle, 0, SEEK_END);
        this->compressed_size = VSIFTellL((FILE*)poBaseHandle) - offset;
        compressed_size = this->compressed_size;
    }
    this->uncompressed_size = uncompressed_size;
    offsetEndCompressedData = offset + compressed_size;

    VSIFSeekL((FILE*)poBaseHandle, offset, SEEK_SET);

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    stream.next_in = inbuf = Z_NULL;
    stream.next_out = outbuf = Z_NULL;
    stream.avail_in = stream.avail_out = 0;
    z_err = Z_OK;
    z_eof = 0;
    in = 0;
    out = 0;
    crc = crc32(0L, Z_NULL, 0);
    this->transparent = transparent;

    stream.next_in  = inbuf = (Byte*)ALLOC(Z_BUFSIZE);

    int err = inflateInit2(&(stream), -MAX_WBITS);
    /* windowBits is passed < 0 to tell that there is no zlib header.
        * Note that in this case inflate *requires* an extra "dummy" byte
        * after the compressed stream in order to complete decompression and
        * return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
        * present after the compressed stream.
        */
    if (err != Z_OK || inbuf == Z_NULL) {
        CPLError(CE_Failure, CPLE_NotSupported, "inflateInit2 init failed");
    }
    stream.avail_out = Z_BUFSIZE;

    if (offset == 0) check_header(); /* skip the .gz header */
    startOff = VSIFTellL((FILE*)poBaseHandle) - stream.avail_in;

    if (transparent == 0)
    {
        snapshot_byte_interval = MAX(Z_BUFSIZE, compressed_size / 100);
        snapshots = (GZipSnapshot*)CPLCalloc(sizeof(GZipSnapshot), (size_t) (compressed_size / snapshot_byte_interval + 1));
    }
    else
    {
        snapshots = NULL;
    }
}

/************************************************************************/
/*                      ~VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::~VSIGZipHandle()
{
    if (stream.state != NULL) {
        inflateEnd(&(stream));
    }

    TRYFREE(inbuf);
    TRYFREE(outbuf);

    if (snapshots != NULL)
    {
        unsigned int i;
        for(i=0;i<compressed_size / snapshot_byte_interval + 1;i++)
        {
            if (snapshots[i].uncompressed_pos)
            {
                inflateEnd(&(snapshots[i].stream));
            }
        }
        CPLFree(snapshots);
    }
    CPLFree(pszOptionalFileName);

    if (poBaseHandle)
        VSIFCloseL((FILE*)poBaseHandle);
}

/************************************************************************/
/*                      check_header()                                  */
/************************************************************************/

void VSIGZipHandle::check_header()
{
    int method; /* method byte */
    int flags;  /* flags byte */
    uInt len;
    int c;

    /* Assure two bytes in the buffer so we can peek ahead -- handle case
    where first byte of header is at the end of the buffer after the last
    gzip segment */
    len = stream.avail_in;
    if (len < 2) {
        if (len) inbuf[0] = stream.next_in[0];
        errno = 0;
        len = (uInt)VSIFReadL(inbuf + len, 1, Z_BUFSIZE >> len, (FILE*)poBaseHandle);
        if (ENABLE_DEBUG) CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                    VSIFTellL((FILE*)poBaseHandle), offsetEndCompressedData);
        if (VSIFTellL((FILE*)poBaseHandle) > offsetEndCompressedData)
        {
            len = len + (uInt) (offsetEndCompressedData - VSIFTellL((FILE*)poBaseHandle));
            VSIFSeekL((FILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
        }
        if (len == 0 /* && ferror(file)*/)
        {
            if (VSIFTellL((FILE*)poBaseHandle) != offsetEndCompressedData)
                z_err = Z_ERRNO;
        }
        stream.avail_in += len;
        stream.next_in = inbuf;
        if (stream.avail_in < 2) {
            transparent = stream.avail_in;
            return;
        }
    }

    /* Peek ahead to check the gzip magic header */
    if (stream.next_in[0] != gz_magic[0] ||
        stream.next_in[1] != gz_magic[1]) {
        transparent = 1;
        return;
    }
    stream.avail_in -= 2;
    stream.next_in += 2;

    /* Check the rest of the gzip header */
    method = get_byte();
    flags = get_byte();
    if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
        z_err = Z_DATA_ERROR;
        return;
    }

    /* Discard time, xflags and OS code: */
    for (len = 0; len < 6; len++) (void)get_byte();

    if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
        len  =  (uInt)get_byte();
        len += ((uInt)get_byte())<<8;
        /* len is garbage if EOF but the loop below will quit anyway */
        while (len-- != 0 && get_byte() != EOF) ;
    }
    if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
        while ((c = get_byte()) != 0 && c != EOF) ;
    }
    if ((flags & COMMENT) != 0) {   /* skip the .gz file comment */
        while ((c = get_byte()) != 0 && c != EOF) ;
    }
    if ((flags & HEAD_CRC) != 0) {  /* skip the header crc */
        for (len = 0; len < 2; len++) (void)get_byte();
    }
    z_err = z_eof ? Z_DATA_ERROR : Z_OK;
}

/************************************************************************/
/*                            get_byte()                                */
/************************************************************************/

int VSIGZipHandle::get_byte()
{
    if (z_eof) return EOF;
    if (stream.avail_in == 0) {
        errno = 0;
        stream.avail_in = (uInt)VSIFReadL(inbuf, 1, Z_BUFSIZE, (FILE*)poBaseHandle);
        if (ENABLE_DEBUG) CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                   VSIFTellL((FILE*)poBaseHandle), offsetEndCompressedData);
        if (VSIFTellL((FILE*)poBaseHandle) > offsetEndCompressedData)
        {
            stream.avail_in = stream.avail_in + (uInt) (offsetEndCompressedData - VSIFTellL((FILE*)poBaseHandle));
            VSIFSeekL((FILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
        }
        if (stream.avail_in == 0) {
            z_eof = 1;
            if (VSIFTellL((FILE*)poBaseHandle) != offsetEndCompressedData)
                z_err = Z_ERRNO;
            /*if (ferror(file)) z_err = Z_ERRNO;*/
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
    crc = crc32(0L, Z_NULL, 0);
    if (!transparent) (void)inflateReset(&stream);
    in = 0;
    out = 0;
    return VSIFSeekL((FILE*)poBaseHandle, startOff, SEEK_SET);
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

int VSIGZipHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    /* The semantics of gzseek is different from ::Seek */
    /* It returns the current offset, where as ::Seek shoud return 0 */
    /* if successfull */
    int ret = gzseek(nOffset, nWhence);
    return (ret >= 0) ? 0 : ret;
}

/************************************************************************/
/*                            gzseek()                                  */
/************************************************************************/

int VSIGZipHandle::gzseek( vsi_l_offset offset, int whence )
{
    vsi_l_offset original_offset = offset;
    int original_nWhence = whence;

    if (ENABLE_DEBUG) CPLDebug("GZIP", "Seek(" CPL_FRMT_GUIB ",%d)", offset, whence);

    if (transparent)
    {
        stream.avail_in = 0;
        stream.next_in = inbuf;
        if (whence == SEEK_CUR)
        {
            if (out + offset < 0 || out + offset > compressed_size)
            {
                CPL_VSIL_GZ_RETURN_MINUS_ONE();
                return -1L;
            }

            offset = startOff + out + offset;
        }
        else if (whence == SEEK_SET)
        {
            if (offset < 0 || offset > compressed_size)
            {
                CPL_VSIL_GZ_RETURN_MINUS_ONE();
                return -1L;
            }

            offset = startOff + offset;
        }
        else if (whence == SEEK_END)
        {
            /* Commented test : because vsi_l_offset is unsigned (for the moment) */
            /* so no way to seek backward. See #1590 */
            if (offset > 0 /*|| -offset > compressed_size*/)
            {
                CPL_VSIL_GZ_RETURN_MINUS_ONE();
                return -1L;
            }

            offset = startOff + compressed_size - offset;
        }
        else
        {
            CPL_VSIL_GZ_RETURN_MINUS_ONE();
            return -1L;
        }
        if (VSIFSeekL((FILE*)poBaseHandle, offset, SEEK_SET) < 0)
        {
            CPL_VSIL_GZ_RETURN_MINUS_ONE();
            return -1L;
        }

        in = out = offset - startOff;
        if (ENABLE_DEBUG) CPLDebug("GZIP", "return " CPL_FRMT_GUIB, offset);
        return (int) in;
    }

    /* whence == SEEK_END is unsuppored in original gzseek. */
    if (whence == SEEK_END)
    {
        /* If we known the uncompressed size, we can fake a jump to */
        /* the end of the stream */
        if (offset == 0 && uncompressed_size != 0)
        {
            out = uncompressed_size;
            return 1;
        }

        /* We don't know the uncompressed size. This is unfortunate. Let's do the slow version... */
        static int firstWarning = 1;
        if (compressed_size > 10 * 1024 * 1024 && firstWarning)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "VSIFSeekL(xxx, SEEK_END) may be really slow on GZip streams.");
            firstWarning = 0;
        }
        
        whence = SEEK_CUR;
        offset = 1024 * 1024 * 1024;
        offset *= 1024 * 1024;
    }

    if (/*whence == SEEK_END ||*/
        z_err == Z_ERRNO || z_err == Z_DATA_ERROR) {
        CPL_VSIL_GZ_RETURN_MINUS_ONE();
        return -1L;
    }

    /* Rest of function is for reading only */

    /* compute absolute position */
    if (whence == SEEK_CUR) {
        offset += out;
    }
    if (offset < 0) {
        CPL_VSIL_GZ_RETURN_MINUS_ONE();
        return -1L;
    }

    /* For a negative seek, rewind and use positive seek */
    if (offset >= out) {
        offset -= out;
    } else if (gzrewind() < 0) {
            CPL_VSIL_GZ_RETURN_MINUS_ONE();
            return -1L;
    }
    
    unsigned int i;
    for(i=0;i<compressed_size / snapshot_byte_interval + 1;i++)
    {
        if (snapshots[i].uncompressed_pos == 0)
            break;
        if (snapshots[i].out <= out + offset &&
            (i == compressed_size / snapshot_byte_interval || snapshots[i+1].out == 0 || snapshots[i+1].out > out+offset))
        {
            if (out >= snapshots[i].out)
                break;

            if (ENABLE_DEBUG)
                CPLDebug("SNAPSHOT", "using snapshot %d : uncompressed_pos(snapshot)=" CPL_FRMT_GUIB
                                                        " in(snapshot)=" CPL_FRMT_GUIB
                                                        " out(snapshot)=" CPL_FRMT_GUIB
                                                        " out=" CPL_FRMT_GUIB
                                                        " offset=" CPL_FRMT_GUIB,
                         i, snapshots[i].uncompressed_pos, snapshots[i].in, snapshots[i].out, out, offset);
            offset = out + offset - snapshots[i].out;
            VSIFSeekL((FILE*)poBaseHandle, snapshots[i].uncompressed_pos, SEEK_SET);
            inflateEnd(&stream);
            inflateCopy(&stream, &snapshots[i].stream);
            crc = snapshots[i].crc;
            transparent = snapshots[i].transparent;
            in = snapshots[i].in;
            out = snapshots[i].out;
            break;
        }
    }

    /* offset is now the number of bytes to skip. */

    if (offset != 0 && outbuf == Z_NULL) {
        outbuf = (Byte*)ALLOC(Z_BUFSIZE);
        if (outbuf == Z_NULL) {
            CPL_VSIL_GZ_RETURN_MINUS_ONE();
            return -1L;
        }
    }

    if (original_nWhence == SEEK_END && z_err == Z_STREAM_END)
    {
        if (ENABLE_DEBUG) CPLDebug("GZIP", "gzseek return " CPL_FRMT_GUIB, out);
        return (int) out;
    }

    while (offset > 0)  {
        int size = Z_BUFSIZE;
        if (offset < Z_BUFSIZE) size = (int)offset;

        int read_size = Read(outbuf, 1, (uInt)size);
        if (read_size == 0) {
            CPL_VSIL_GZ_RETURN_MINUS_ONE();
            return -1L;
        }
        if (original_nWhence == SEEK_END)
        {
            if (size != read_size)
            {
                z_err = Z_STREAM_END;
                break;
            }
        }
        offset -= read_size;
    }
    if (ENABLE_DEBUG) CPLDebug("GZIP", "gzseek return " CPL_FRMT_GUIB, out);

    if (original_offset == 0 && original_nWhence == SEEK_END)
        uncompressed_size = out;

    return (int) out;
}

/************************************************************************/
/*                              Tell()                                  */
/************************************************************************/

vsi_l_offset VSIGZipHandle::Tell()
{
    if (ENABLE_DEBUG) CPLDebug("GZIP", "Tell() = " CPL_FRMT_GUIB, out);
    return out;
}

/************************************************************************/
/*                              Read()                                  */
/************************************************************************/

size_t VSIGZipHandle::Read( void *buf, size_t nSize, size_t nMemb )
{
    if (ENABLE_DEBUG) CPLDebug("GZIP", "Read(%p, %d, %d)", buf, (int)nSize, (int)nMemb);

    unsigned len = nSize * nMemb;

    Bytef *pStart = (Bytef*)buf; /* startOffing point for crc computation */
    Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */

    if  (z_err == Z_DATA_ERROR || z_err == Z_ERRNO)
    {
        CPL_VSIL_GZ_RETURN_MINUS_ONE();
        return 0;
    }
    if  (z_err == Z_STREAM_END)
    {
        if (ENABLE_DEBUG) CPLDebug("GZIP", "Read: Eof");
        return 0;  /* EOF */
    }

    next_out = (Byte*)buf;
    stream.next_out = (Bytef*)buf;
    stream.avail_out = len;

    while  (stream.avail_out != 0) {

        if  (transparent) {
            /* Copy first the lookahead bytes: */
            uInt n = stream.avail_in;
            if (n > stream.avail_out) n = stream.avail_out;
            if (n > 0) {
                memcpy (stream.next_out, stream.next_in, n);
                next_out += n;
                stream.next_out = next_out;
                stream.next_in   += n;
                stream.avail_out -= n;
                stream.avail_in  -= n;
            }
            if  (stream.avail_out > 0) {
                stream.avail_out -=
                    (uInt)VSIFReadL(next_out, 1, stream.avail_out, (FILE*)poBaseHandle);
            }
            len -= stream.avail_out;
            in  += len;
            out += len;
            if (len == 0) z_eof = 1;
            if (ENABLE_DEBUG) CPLDebug("GZIP", "Read return %d", (int)(len / nSize));
            return (int)len / nSize;
        }
        if  (stream.avail_in == 0 && !z_eof)
        {
            vsi_l_offset uncompressed_pos = VSIFTellL((FILE*)poBaseHandle);
            GZipSnapshot* snapshot = &snapshots[(uncompressed_pos - startOff) / snapshot_byte_interval];
            if (uncompressed_pos >= 0 && snapshot->uncompressed_pos == 0)
            {
                snapshot->crc = crc32 (crc, pStart, (uInt) (stream.next_out - pStart));
                if (ENABLE_DEBUG)
                    CPLDebug("SNAPSHOT",
                             "creating snapshot %d : uncompressed_pos=" CPL_FRMT_GUIB
                                                   " in=" CPL_FRMT_GUIB
                                                   " out=" CPL_FRMT_GUIB
                                                   " crc=%X",
                          (int)((uncompressed_pos - startOff) / snapshot_byte_interval),
                          uncompressed_pos, in, out, (unsigned int)snapshot->crc);
                snapshot->uncompressed_pos = uncompressed_pos;
                inflateCopy(&snapshot->stream, &stream);
                snapshot->transparent = transparent;
                snapshot->in = in;
                snapshot->out = out;
            }

            errno = 0;
            stream.avail_in = (uInt)VSIFReadL(inbuf, 1, Z_BUFSIZE, (FILE*)poBaseHandle);
            if (ENABLE_DEBUG)
                CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                 VSIFTellL((FILE*)poBaseHandle), offsetEndCompressedData);
            if (VSIFTellL((FILE*)poBaseHandle) > offsetEndCompressedData)
            {
                if (ENABLE_DEBUG) CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
                stream.avail_in = stream.avail_in + (uInt) (offsetEndCompressedData - VSIFTellL((FILE*)poBaseHandle));
                VSIFSeekL((FILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
                if (ENABLE_DEBUG) CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
            }
            if  (stream.avail_in == 0) {
                z_eof = 1;
                if (VSIFTellL((FILE*)poBaseHandle) != offsetEndCompressedData)
                {
                    z_err = Z_ERRNO;
                    break;
                }
                /*if (ferror (file)) {
                    z_err = Z_ERRNO;
                    break;
                }*/
            }
            stream.next_in = inbuf;
        }
        in += stream.avail_in;
        out += stream.avail_out;
        z_err = inflate(& (stream), Z_NO_FLUSH);
        in -= stream.avail_in;
        out -= stream.avail_out;
        
        if  (z_err == Z_STREAM_END) {
            /* Check CRC and original size */
            crc = crc32 (crc, pStart, (uInt) (stream.next_out - pStart));
            pStart = stream.next_out;
            if (expected_crc)
            {
                if (ENABLE_DEBUG) CPLDebug("GZIP", "Computed CRC = %X. Expected CRC = %X", (unsigned int)crc, expected_crc);
            }
            if (expected_crc != 0 && expected_crc != crc)
            {
                CPLError(CE_Failure, CPLE_FileIO, "CRC error. Got %X instead of %X", (unsigned int)crc, expected_crc);
                z_err = Z_DATA_ERROR;
            }
            else if (expected_crc == 0)
            {
                unsigned int read_crc = getLong();
                if (read_crc != crc)
                {
                    CPLError(CE_Failure, CPLE_FileIO, "CRC error. Got %X instead of %X", (unsigned int)crc, read_crc);
                    z_err = Z_DATA_ERROR;
                }
                else
                {
                    (void)getLong();
                    /* The uncompressed length returned by above getlong() may be
                    * different from out in case of concatenated .gz files.
                    * Check for such files:
                    */
                    check_header();
                    if  (z_err == Z_OK) {
                        inflateReset(& (stream));
                        crc = crc32(0L, Z_NULL, 0);
                    }
                }
            }
        }
        if  (z_err != Z_OK || z_eof) break;
    }
    crc = crc32 (crc, pStart, (uInt) (stream.next_out - pStart));

    if (len == stream.avail_out &&
            (z_err == Z_DATA_ERROR || z_err == Z_ERRNO))
    {
        CPL_VSIL_GZ_RETURN_MINUS_ONE();
        return 0;
    }
    if (ENABLE_DEBUG)
        CPLDebug("GZIP", "Read return %d (z_err=%d, z_eof=%d)",
                (int)((len - stream.avail_out) / nSize), z_err, z_eof);
    return (int)(len - stream.avail_out) / nSize;
}

/************************************************************************/
/*                              getLong()                               */
/************************************************************************/

uLong VSIGZipHandle::getLong ()
{
    uLong x = (uLong)get_byte();
    int c;

    x += ((uLong)get_byte())<<8;
    x += ((uLong)get_byte())<<16;
    c = get_byte();
    if (c == EOF) z_err = Z_DATA_ERROR;
    x += ((uLong)c)<<24;
    return x;
}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

size_t VSIGZipHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    CPLError(CE_Failure, CPLE_NotSupported, "VSIFWriteL is not supported on GZip streams\n");
    return 0;
}

/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/


int VSIGZipHandle::Eof()
{
    if (ENABLE_DEBUG) CPLDebug("GZIP", "Eof()");
    if (z_eof) return 1;
    return z_err == Z_STREAM_END;
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

class VSIGZipWriteHandle : public VSIVirtualHandle
{
    VSIVirtualHandle*  poBaseHandle;
    z_stream           sStream;
    Byte              *pabyInBuf;
    Byte              *pabyOutBuf;
    bool               bCompressActive;
    vsi_l_offset       nCurOffset;
    GUInt32            nCRC;

  public:

    VSIGZipWriteHandle(VSIVirtualHandle* poBaseHandle);

    ~VSIGZipWriteHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
};

/************************************************************************/
/*                         VSIGZipWriteHandle()                         */
/************************************************************************/

VSIGZipWriteHandle::VSIGZipWriteHandle( VSIVirtualHandle *poBaseHandle )

{
    nCurOffset = 0;

    this->poBaseHandle = poBaseHandle;

    nCRC = crc32(0L, Z_NULL, 0);
    sStream.zalloc = (alloc_func)0;
    sStream.zfree = (free_func)0;
    sStream.opaque = (voidpf)0;
    sStream.next_in = Z_NULL;
    sStream.next_out = Z_NULL;
    sStream.avail_in = sStream.avail_out = 0;

    pabyInBuf = (Byte *) CPLMalloc( Z_BUFSIZE );
    sStream.next_in  = pabyInBuf;

    pabyOutBuf = (Byte *) CPLMalloc( Z_BUFSIZE );

    if( deflateInit2( &sStream, Z_DEFAULT_COMPRESSION,
                      Z_DEFLATED, -MAX_WBITS, 8,
                      Z_DEFAULT_STRATEGY ) != Z_OK )
        bCompressActive = false;
    else
    {
        char header[11];

        /* Write a very simple .gz header:
         */
        sprintf( header, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
                 Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, 
                 0x03 );
        poBaseHandle->Write( header, 1, 10 );

        bCompressActive = true;
    }
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
    if( bCompressActive )
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = Z_BUFSIZE;

        deflate( &sStream, Z_FINISH );

        size_t nOutBytes = Z_BUFSIZE - sStream.avail_out;

        if( poBaseHandle->Write( pabyOutBuf, 1, nOutBytes ) < nOutBytes )
            return EOF;

        deflateEnd( &sStream );

        GUInt32 anTrailer[2];

        anTrailer[0] = CPL_LSBWORD32( nCRC );
        anTrailer[1] = CPL_LSBWORD32( (GUInt32) nCurOffset );

        poBaseHandle->Write( anTrailer, 1, 8 );
        poBaseHandle->Close();

        delete poBaseHandle;

        bCompressActive = false;
    }

    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Read( void *pBuffer, size_t nSize, size_t nMemb )

{
    CPLError(CE_Failure, CPLE_NotSupported, "VSIFReadL is not supported on GZip write streams\n");
    return 0;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Write( const void *pBuffer, 
                                  size_t nSize, size_t nMemb )

{
    int  nBytesToWrite, nNextByte;

    nBytesToWrite = (int) (nSize * nMemb);
    nNextByte = 0;

    nCRC = crc32(nCRC, (const Bytef *)pBuffer, nBytesToWrite);

    if( !bCompressActive )
        return 0;

    while( nNextByte < nBytesToWrite )
    {
        sStream.next_out = pabyOutBuf;
        sStream.avail_out = Z_BUFSIZE;

        if( sStream.avail_in > 0 )
            memmove( pabyInBuf, sStream.next_in, sStream.avail_in );

        int nNewBytesToWrite = MIN((int) (Z_BUFSIZE-sStream.avail_in),
                                   nBytesToWrite - nNextByte);
        memcpy( pabyInBuf + sStream.avail_in, 
                ((Byte *) pBuffer) + nNextByte, 
                nNewBytesToWrite );
        
        sStream.next_in = pabyInBuf;
        sStream.avail_in += nNewBytesToWrite;

        deflate( &sStream, Z_NO_FLUSH );

        size_t nOutBytes = Z_BUFSIZE - sStream.avail_out;

        if( nOutBytes > 0 )
        {
            if( poBaseHandle->Write( pabyOutBuf, 1, nOutBytes ) < nOutBytes )
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
                 "Seeking on writable compressed data streams not supported." );
               
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

class VSIGZipFilesystemHandler : public VSIFilesystemHandler 
{
    void* hMutex;
    char* pszLastStatedFileName;
    VSIGZipHandle* poHandleLastStateFile;
    VSIStatBufL statBuf;

public:
    VSIGZipFilesystemHandler();
    ~VSIGZipFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );

    void  CacheLastStatedFile( const char *pszFilename,
                               VSIGZipHandle* poHandle,
                               VSIStatBufL* pStatBuf);
};


/************************************************************************/
/*                   VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::VSIGZipFilesystemHandler()
{
    hMutex = NULL;

    pszLastStatedFileName = NULL;
    poHandleLastStateFile = NULL;
}

/************************************************************************/
/*                  ~VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::~VSIGZipFilesystemHandler()
{
    CPLFree(pszLastStatedFileName);
    if (poHandleLastStateFile)
        delete poHandleLastStateFile;

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}


/************************************************************************/
/*                      CacheLastStatedFile()                           */
/************************************************************************/

void VSIGZipFilesystemHandler::CacheLastStatedFile( const char *pszFilename,
                                                    VSIGZipHandle* poHandle,
                                                    VSIStatBufL* pStatBuf)
{
    CPLMutexHolder oHolder(&hMutex);

    CPLFree(pszLastStatedFileName);
    if (poHandleLastStateFile)
        delete poHandleLastStateFile;

    poHandleLastStateFile = poHandle;
    /* There's no use in keeping the base handle open for that cached */
    /* VSIGZipHandle object */
    poHandle->CloseBaseHandle();
    pszLastStatedFileName = CPLStrdup(pszFilename);
    memcpy(&statBuf, pStatBuf, sizeof(VSIStatBufL));
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIGZipFilesystemHandler::Open( const char *pszFilename, 
                                                  const char *pszAccess)
{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename + strlen("/vsigzip/"));

/* -------------------------------------------------------------------- */
/*      Is this an attempt to write a new file without update (w+)      */
/*      access?  If so, create a writable handle for the underlying     */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    if (strchr(pszAccess, 'w') != NULL )
    {
        if( strchr(pszAccess, '+') != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Write+update (w+) not supported for /vsigzip, only read-only or write-only.");
            return NULL;
        }

        VSIVirtualHandle* poVirtualHandle =
            poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "wb" );

        if (poVirtualHandle == NULL)
            return NULL;

        else
            return new VSIGZipWriteHandle( poVirtualHandle );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we are in the read access case.                       */
/* -------------------------------------------------------------------- */
    CPLMutexHolder oHolder(&hMutex);

    if (pszLastStatedFileName != NULL &&
        strcmp(pszFilename, pszLastStatedFileName) == 0 &&
        EQUAL(pszAccess, "rb"))
    {
            VSIVirtualHandle* poHandle = poHandleLastStateFile->Duplicate();
            if (poHandle)
                return poHandle;
    }

    unsigned char signature[2];

    VSIVirtualHandle* poVirtualHandle =
        poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "rb" );

    if (poVirtualHandle == NULL)
        return NULL;

    if (VSIFReadL(signature, 1, 2, (FILE*)poVirtualHandle) != 2)
        return NULL;

    if (signature[0] != gz_magic[0] || signature[1] != gz_magic[1])
        return NULL;

    CPLFree(pszLastStatedFileName);
    pszLastStatedFileName = NULL;
    if (poHandleLastStateFile)
        delete poHandleLastStateFile;
    poHandleLastStateFile = NULL;

    return new VSIGZipHandle(poVirtualHandle, pszFilename + strlen("/vsigzip/"));
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf )
{
    CPLMutexHolder oHolder(&hMutex);

    if (pszLastStatedFileName != NULL &&
        strcmp(pszFilename, pszLastStatedFileName) == 0)
    {
        memcpy(pStatBuf, &statBuf, sizeof(VSIStatBufL));
        return 0;
    }

    /* Begin by doing a stat on the real file */
    int ret = VSIStatL(pszFilename+strlen("/vsigzip/"), pStatBuf);

    if (ret == 0)
    {
        CPLString osCacheFilename(pszFilename+strlen("/vsigzip/"));
        osCacheFilename += ".properties";

        /* Can we save a bit of seeking by using a .properties file ? */
        FILE* fpCacheLength = VSIFOpen(osCacheFilename.c_str(), "rt");
        if (fpCacheLength)
        {
            char szBuffer[80];
            szBuffer[79] = 0;
            GUIntBig nCompressedSize = 0;
            GUIntBig nUncompressedSize = 0;
            while (CPLFGets(szBuffer, 79, fpCacheLength))
            {
                if (EQUALN(szBuffer, "compressed_size=", strlen("compressed_size=")))
                {
                    char* pszBuffer = szBuffer + strlen("compressed_size=");
                    nCompressedSize = 
                            CPLScanUIntBig(pszBuffer, strlen(pszBuffer));
                }
                else if (EQUALN(szBuffer, "uncompressed_size=", strlen("uncompressed_size=")))
                {
                    char* pszBuffer = szBuffer + strlen("uncompressed_size=");
                    nUncompressedSize =
                             CPLScanUIntBig(pszBuffer, strlen(pszBuffer));
                }
            }

            VSIFClose(fpCacheLength);

            if (nCompressedSize == (GUIntBig) pStatBuf->st_size)
            {
                /* Patch with the uncompressed size */
                pStatBuf->st_size = (long)nUncompressedSize;
                return ret;
            }
        }

        /* No, then seek at the end of the data (slow) */
        VSIGZipHandle* poHandle =
                (VSIGZipHandle*)VSIGZipFilesystemHandler::Open(pszFilename, "rb");
        if (poHandle)
        {
            GUIntBig uncompressed_size;
            GUIntBig compressed_size = (GUIntBig) pStatBuf->st_size;
            poHandle->Seek(0, SEEK_END);
            uncompressed_size = (GUIntBig) poHandle->Tell();
            poHandle->Seek(0, SEEK_SET);

            /* Patch with the uncompressed size */
            pStatBuf->st_size = (long)uncompressed_size;
            CacheLastStatedFile(pszFilename, poHandle, pStatBuf);

            /* Write a .properties file to avoid seeking next time */
            fpCacheLength = VSIFOpen(osCacheFilename.c_str(), "wt");
            if (fpCacheLength)
            {
                char* pszFirstNonSpace;
                char szBuffer[32];
                szBuffer[31] = 0;

                CPLPrintUIntBig(szBuffer, compressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ') pszFirstNonSpace ++;
                VSIFPrintf(fpCacheLength, "compressed_size=%s\n", pszFirstNonSpace);

                CPLPrintUIntBig(szBuffer, uncompressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ') pszFirstNonSpace ++;
                VSIFPrintf(fpCacheLength, "uncompressed_size=%s\n", pszFirstNonSpace);

                VSIFClose(fpCacheLength);
            }
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

int VSIGZipFilesystemHandler::Unlink( const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIGZipFilesystemHandler::Rename( const char *oldpath, const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Mkdir( const char *pszDirname, long nMode )
{
    return -1;
}
/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Rmdir( const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSIGZipFilesystemHandler::ReadDir( const char *pszDirname )
{
    return NULL;
}

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
 * Additional documentation is to be found at http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
 *
 */

void VSIInstallGZipFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsigzip/", new VSIGZipFilesystemHandler );
}



/************************************************************************/
/* ==================================================================== */
/*                       VSIZipFilesystemHandler                  */
/* ==================================================================== */
/************************************************************************/

typedef struct
{
    char         *fileName;
    vsi_l_offset  uncompressed_size;
    unz_file_pos  file_pos;
    int           bIsDir;
} ZIPEntry;

typedef struct
{
    int nEntries;
    ZIPEntry* entries;
} ZIPContent;

class VSIZipFilesystemHandler : public VSIFilesystemHandler 
{
    void* hMutex;
    /* We use a cache that contains the list of files containes in a ZIP file as */
    /* unzip.c is quite inefficient in listing them. This speeds up access to ZIP files */
    /* containing ~1000 files like a CADRG product */
    std::map<CPLString,ZIPContent*>   oFileList;

public:
    VSIZipFilesystemHandler();
    ~VSIZipFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );

    const ZIPContent* GetContentOfZip(const char* zipFilename, unzFile unzF = NULL);
    char* SplitFilename(const char *pszFilename, CPLString &osZipInFileName);
    unzFile OpenZIPFile(const char* zipFilename, const char* zipInFileName);
    int FindFileInZip(const char* zipFilename, const char* zipInFileName, const ZIPEntry** zipEntry);
};

/************************************************************************/
/*                    VSIZipFilesystemHandler()                        */
/************************************************************************/

VSIZipFilesystemHandler::VSIZipFilesystemHandler()
{
    hMutex = NULL;
}

/************************************************************************/
/*                    ~VSIZipFilesystemHandler()                        */
/************************************************************************/

VSIZipFilesystemHandler::~VSIZipFilesystemHandler()

{
    std::map<CPLString,ZIPContent*>::const_iterator iter;

    for( iter = oFileList.begin(); iter != oFileList.end(); ++iter )
    {
        ZIPContent* content = iter->second;
        int i;
        for(i=0;i<content->nEntries;i++)
        {
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
/*                         GetContentOfZip()                            */
/************************************************************************/

const ZIPContent* VSIZipFilesystemHandler::GetContentOfZip(const char* zipFilename, unzFile unzF)
{
    CPLMutexHolder oHolder( &hMutex );

    if (oFileList.find(zipFilename) != oFileList.end() )
    {
        return oFileList[zipFilename];
    }

    int bMustCloseUnzf = (unzF == NULL);
    if (unzF == NULL)
    {
        unzF = cpl_unzOpen(zipFilename);
        if (!unzF)
            return NULL;
    }

    if (cpl_unzGoToFirstFile(unzF) != UNZ_OK)
    {
        if (bMustCloseUnzf)
            cpl_unzClose(unzF);
        return NULL;
    }

    ZIPContent* content = new ZIPContent;
    content->nEntries = 0;
    content->entries = NULL;
    oFileList[zipFilename] = content;

    do
    {
        char fileName[512];
        unz_file_info file_info;
        cpl_unzGetCurrentFileInfo (unzF, &file_info, fileName, 512, NULL, 0, NULL, 0);
        content->entries = (ZIPEntry*)CPLRealloc(content->entries, sizeof(ZIPEntry) * (content->nEntries + 1));
        content->entries[content->nEntries].fileName = CPLStrdup(fileName);
        content->entries[content->nEntries].uncompressed_size = file_info.uncompressed_size;
        content->entries[content->nEntries].bIsDir =
                strlen(fileName) > 0 &&
                (fileName[strlen(fileName)-1] == '/' || fileName[strlen(fileName)-1] == '\\');
        if (content->entries[content->nEntries].bIsDir)
        {
            /* Remove trailing slash */
            content->entries[content->nEntries].fileName[strlen(fileName)-1] = 0;
        }
        cpl_unzGetFilePos(unzF, &(content->entries[content->nEntries].file_pos));
        if (ENABLE_DEBUG)
            CPLDebug("ZIP", "[%d] %s : " CPL_FRMT_GUIB " bytes", content->nEntries+1,
                 content->entries[content->nEntries].fileName,
                 content->entries[content->nEntries].uncompressed_size);
        content->nEntries++;
    } while(cpl_unzGoToNextFile(unzF) == UNZ_OK);

    if (bMustCloseUnzf)
        cpl_unzClose(unzF);

    return content;
}

/************************************************************************/
/*                           FindFileInZip()                            */
/************************************************************************/

int VSIZipFilesystemHandler::FindFileInZip(const char* zipFilename,
                                           const char* zipInFileName,
                                           const ZIPEntry** zipEntry)
{
    if (zipInFileName == NULL)
        return FALSE;

    const ZIPContent* content = GetContentOfZip(zipFilename);
    if (content)
    {
        int i;
        for(i=0;i<content->nEntries;i++)
        {
            if (strcmp(zipInFileName, content->entries[i].fileName) == 0)
            {
                if (zipEntry)
                    *zipEntry = &content->entries[i];
                return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************/
/*                           SplitFilename()                            */
/************************************************************************/

char* VSIZipFilesystemHandler::SplitFilename(const char *pszFilename,
                                             CPLString &osZipInFileName)
{
    int i = 0;

    /* Allow natural chaining of VSI drivers without requiring double slash */
    if (strncmp(pszFilename, "/vsizip/vsi", strlen("/vsizip/vsi")) == 0)
        pszFilename += strlen("/vsizip");
    else
        pszFilename += strlen("/vsizip/");

    while(pszFilename[i])
    {
        if (EQUALN(pszFilename + i, ".zip", 4))
        {
            VSIStatBufL statBuf;
            char* zipFilename = CPLStrdup(pszFilename);
            int bZipFileExists = FALSE;
            zipFilename[i + 4] = 0;

            {
                CPLMutexHolder oHolder( &hMutex );

                if (oFileList.find(zipFilename) != oFileList.end() )
                {
                    bZipFileExists = TRUE;
                }
            }

            if (!bZipFileExists)
            {
                VSIFilesystemHandler *poFSHandler = 
                    VSIFileManager::GetHandler( zipFilename );
                if (poFSHandler->Stat(zipFilename, &statBuf) == 0 &&
                    !VSI_ISDIR(statBuf.st_mode))
                {
                    bZipFileExists = TRUE;
                }
            }

            if (bZipFileExists)
            {
                if (pszFilename[i + 4] != 0)
                {
                    char* pszZipInFileName = CPLStrdup(pszFilename + i + 5);

                    /* Replace a/../b by b and foo/a/../b by foo/b */
                    while(TRUE)
                    {
                        char* pszPrevDir = strstr(pszZipInFileName, "/../");
                        if (pszPrevDir == NULL || pszPrevDir == pszZipInFileName)
                            break;

                        char* pszPrevSlash = pszPrevDir - 1;
                        while(pszPrevSlash != pszZipInFileName &&
                                *pszPrevSlash != '/')
                            pszPrevSlash --;
                        if (pszPrevSlash == pszZipInFileName)
                            memmove(pszZipInFileName, pszPrevDir + 4, strlen(pszPrevDir + 4) + 1);
                        else
                            memmove(pszPrevSlash + 1, pszPrevDir + 4, strlen(pszPrevDir + 4) + 1);
                    }

                    osZipInFileName = pszZipInFileName;
                    CPLFree(pszZipInFileName);
                }
                else
                    osZipInFileName = "";

                /* Remove trailing slash */
                if (strlen(osZipInFileName))
                {
                    char lastC = osZipInFileName[strlen(osZipInFileName) - 1];
                    if (lastC == '\\' || lastC == '/')
                        osZipInFileName[strlen(osZipInFileName) - 1] = 0;
                }

                return zipFilename;
            }
            CPLFree(zipFilename);
        }
        i++;
    }
    return NULL;
}

/************************************************************************/
/*                             OpenZIPFile()                            */
/************************************************************************/

unzFile VSIZipFilesystemHandler::OpenZIPFile(const char* zipFilename, 
                                             const char* zipInFileName)
{
    unzFile unzF = cpl_unzOpen(zipFilename);

    if (unzF == NULL)
    {
        return NULL;
    }

    if (zipInFileName == NULL || strlen(zipInFileName) == 0)
    {
        if (cpl_unzGoToFirstFile(unzF) != UNZ_OK)
        {
            cpl_unzClose(unzF);
            return NULL;
        }

        char fileName[512];
        int isSecondFile = FALSE;
        cpl_unzGetCurrentFileInfo (unzF, NULL, fileName, 512, NULL, 0, NULL, 0);
        if (fileName[strlen(fileName)-1] == '/' || fileName[strlen(fileName)-1] == '\\')
        {
            if (cpl_unzGoToNextFile(unzF) != UNZ_OK)
            {
                cpl_unzClose(unzF);
                return NULL;
            }
            isSecondFile = TRUE;
        }

        if (cpl_unzGoToNextFile(unzF) != UNZ_END_OF_LIST_OF_FILE)
        {
            CPLString msg;
            msg.Printf("Support only 1 file in ZIP file %s when no explicit in-zip filename is specified",
                       zipFilename);
            const ZIPContent* content = GetContentOfZip(zipFilename, unzF);
            if (content)
            {
                int i;
                msg += "\nYou could try one of the following :\n";
                for(i=0;i<content->nEntries;i++)
                {
                    msg += CPLString().Printf("  /vsizip/%s/%s\n", zipFilename, content->entries[i].fileName);
                }
            }

            CPLError(CE_Failure, CPLE_NotSupported, "%s", msg.c_str());

            cpl_unzClose(unzF);
            return NULL;
        }

        cpl_unzGoToFirstFile(unzF);
        if (isSecondFile)
            cpl_unzGoToNextFile(unzF);
    }
    else
    {
        const ZIPEntry* zipEntry = NULL;
        if (FindFileInZip(zipFilename, zipInFileName, &zipEntry) == FALSE ||
            zipEntry->bIsDir == TRUE)
        {
            cpl_unzClose(unzF);
            return NULL;
        }
        cpl_unzGoToFilePos(unzF, (unz_file_pos*)&(zipEntry->file_pos));
    }
    return unzF;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

VSIVirtualHandle* VSIZipFilesystemHandler::Open( const char *pszFilename, 
                                                 const char *pszAccess)
{
    char* zipFilename;
    CPLString osZipInFileName;

    if (strchr(pszAccess, 'w') != NULL ||
        strchr(pszAccess, '+') != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only read-only mode is supported for /vsizip");
        return NULL;
    }

    zipFilename = SplitFilename(pszFilename, osZipInFileName);
    if (zipFilename == NULL)
        return NULL;

    unzFile unzF = OpenZIPFile(zipFilename, osZipInFileName);
    if (unzF == NULL)
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

    if (poVirtualHandle == NULL)
    {
        return NULL;
    }

    cpl_unzOpenCurrentFile(unzF);

    uLong64 pos = cpl_unzGetCurrentFileZStreamPos(unzF);

    unz_file_info file_info;
    cpl_unzGetCurrentFileInfo (unzF, &file_info, NULL, 0, NULL, 0, NULL, 0);

    cpl_unzCloseCurrentFile(unzF);

    cpl_unzClose(unzF);

    return new VSIGZipHandle(poVirtualHandle,
                             NULL,
                             pos,
                             file_info.compressed_size,
                             file_info.uncompressed_size,
                             file_info.crc,
                             file_info.compression_method == 0);
}

/************************************************************************/
/*                                 Stat()                               */
/************************************************************************/

int VSIZipFilesystemHandler::Stat( const char *pszFilename, VSIStatBufL *pStatBuf )
{
    int ret;
    CPLString osZipInFileName;
    char* zipFilename = SplitFilename(pszFilename, osZipInFileName);
    if (zipFilename == NULL)
        return -1;

    if (strlen(osZipInFileName) != 0)
    {
        if (ENABLE_DEBUG) CPLDebug("ZIP", "Looking for %s %s\n",
                                    zipFilename, osZipInFileName.c_str());

        const ZIPEntry* zipEntry = NULL;
        if (FindFileInZip(zipFilename, osZipInFileName, &zipEntry) == FALSE)
        {
            ret = -1;
        }
        else
        {
            /* Patching st_size with uncompressed file size */
            pStatBuf->st_size = (long)zipEntry->uncompressed_size;
            if (zipEntry->bIsDir)
                pStatBuf->st_mode = S_IFDIR;
            else
                pStatBuf->st_mode = S_IFREG;
            ret = 0;
        }
    }
    else
    {
        unzFile unzF = OpenZIPFile(zipFilename, NULL);
        if (unzF)
        {
            cpl_unzOpenCurrentFile(unzF);

            unz_file_info file_info;
            cpl_unzGetCurrentFileInfo (unzF, &file_info, NULL, 0, NULL, 0, NULL, 0);

            /* Patching st_size with uncompressed file size */
            pStatBuf->st_size = (long)file_info.uncompressed_size;
            pStatBuf->st_mode = S_IFREG;

            cpl_unzCloseCurrentFile(unzF);

            cpl_unzClose(unzF);
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }

    CPLFree(zipFilename);
    return ret;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

int VSIZipFilesystemHandler::Unlink( const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                             Rename()                                 */
/************************************************************************/

int VSIZipFilesystemHandler::Rename( const char *oldpath, const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                             Mkdir()                                  */
/************************************************************************/

int VSIZipFilesystemHandler::Mkdir( const char *pszDirname, long nMode )
{
    return -1;
}

/************************************************************************/
/*                             Rmdir()                                  */
/************************************************************************/

int VSIZipFilesystemHandler::Rmdir( const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSIZipFilesystemHandler::ReadDir( const char *pszDirname )
{
    CPLString osInZipSubDir;
    char* zipFilename = SplitFilename(pszDirname, osInZipSubDir);
    if (zipFilename == NULL)
        return NULL;
    int lenInZipSubDir = strlen(osInZipSubDir);

    char **papszDir = NULL;
    
    const ZIPContent* content = GetContentOfZip(zipFilename);
    if (!content)
    {
        CPLFree(zipFilename);
        return NULL;
    }

    if (ENABLE_DEBUG) CPLDebug("ZIP", "Read dir %s", pszDirname);
    int i;
    for(i=0;i<content->nEntries;i++)
    {
        const char* fileName = content->entries[i].fileName;
        /* Only list entries at the same level of inZipSubDir */
        if (lenInZipSubDir != 0 &&
            strncmp(fileName, osInZipSubDir, lenInZipSubDir) == 0 &&
            (fileName[lenInZipSubDir] == '/' || fileName[lenInZipSubDir] == '\\') &&
            fileName[lenInZipSubDir + 1] != 0)
        {
            const char* slash = strchr(fileName + lenInZipSubDir + 1, '/');
            if (slash == NULL)
                slash = strchr(fileName + lenInZipSubDir + 1, '\\');
            if (slash == NULL || slash[1] == 0)
            {
                char* tmpFileName = CPLStrdup(fileName);
                if (slash != NULL)
                {
                    tmpFileName[strlen(tmpFileName)-1] = 0;
                }
                if (ENABLE_DEBUG)
                    CPLDebug("ZIP", "Add %s as in directory %s\n",
                            tmpFileName + lenInZipSubDir + 1, pszDirname);
                papszDir = CSLAddString(papszDir, tmpFileName + lenInZipSubDir + 1);
                CPLFree(tmpFileName);
            }
        }
        else if (lenInZipSubDir == 0 &&
                 strchr(fileName, '/') == NULL && strchr(fileName, '\\') == NULL)
        {
            /* Only list toplevel files and directories */
            if (ENABLE_DEBUG) CPLDebug("ZIP", "Add %s as in directory %s\n", fileName, pszDirname);
            papszDir = CSLAddString(papszDir, fileName);
        }
    }

    CPLFree(zipFilename);
    return papszDir;
}

/************************************************************************/
/*                    VSIInstallZipFileHandler()                        */
/************************************************************************/


/**
 * \brief Install ZIP file system handler. 
 *
 * A special file handler is installed that allows reading on-the-fly in ZIP (.zip) archives.
 * All portions of the file system underneath the base
 * path "/vsizip/" will be handled by this driver.
 *
 * Additional documentation is to be found at http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
 *
 */

void VSIInstallZipFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsizip/", new VSIZipFilesystemHandler() );
}

