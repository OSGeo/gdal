/******************************************************************************
 * $Id$
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

   For .zip and .gz, both reading and writing are supported, but just one mode at a time
   (read-only or write-only)
*/


#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include <map>

#include <zlib.h>
#include "cpl_minizip_unzip.h"
#include "cpl_time.h"

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

#define CPL_VSIL_GZ_RETURN(ret)   \
        CPLError(CE_Failure, CPLE_AppDefined, "In file %s, at line %d, return %d", __FILE__, __LINE__, ret)

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
    vsi_l_offset      offset;
    vsi_l_offset      compressed_size;
    vsi_l_offset      uncompressed_size;
    vsi_l_offset      offsetEndCompressedData;
    unsigned int      expected_crc;
    char             *pszBaseFileName; /* optional */
    int               bCanSaveInfo;

    /* Fields from gz_stream structure */
    z_stream stream;
    int      z_err;   /* error code for last stream operation */
    int      z_eof;   /* set if end of input file (but not necessarily of the uncompressed stream ! "in" must be null too ) */
    Byte     *inbuf;  /* input buffer */
    Byte     *outbuf; /* output buffer */
    uLong    crc;     /* crc32 of uncompressed data */
    int      transparent; /* 1 if input file is not a .gz file */
    vsi_l_offset  startOff;   /* startOff of compressed data in file (header skipped) */
    vsi_l_offset  in;      /* bytes into deflate or inflate */
    vsi_l_offset  out;     /* bytes out of deflate or inflate */
    vsi_l_offset  nLastReadOffset;
    
    GZipSnapshot* snapshots;
    vsi_l_offset snapshot_byte_interval; /* number of compressed bytes at which we create a "snapshot" */

    void check_header();
    int get_byte();
    int gzseek( vsi_l_offset nOffset, int nWhence );
    int gzrewind ();
    uLong getLong ();

  public:

    VSIGZipHandle(VSIVirtualHandle* poBaseHandle,
                  const char* pszBaseFileName,
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

    vsi_l_offset      GetLastReadOffset() { return nLastReadOffset; }
    const char*       GetBaseFileName() { return pszBaseFileName; }

    void              SetUncompressedSize(vsi_l_offset nUncompressedSize) { uncompressed_size = nUncompressedSize; }
    vsi_l_offset      GetUncompressedSize() { return uncompressed_size; }
    
    void              SaveInfo_unlocked();
};


class VSIGZipFilesystemHandler : public VSIFilesystemHandler 
{
    CPLMutex* hMutex;
    VSIGZipHandle* poHandleLastGZipFile;
    
public:
    VSIGZipFilesystemHandler();
    ~VSIGZipFilesystemHandler();

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);
    VSIGZipHandle *OpenGZipReadOnly( const char *pszFilename, 
                                     const char *pszAccess);
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );
    virtual int      Unlink( const char *pszFilename );
    virtual int      Rename( const char *oldpath, const char *newpath );
    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual int      Rmdir( const char *pszDirname );
    virtual char   **ReadDir( const char *pszDirname );

    void  SaveInfo( VSIGZipHandle* poHandle );
    void  SaveInfo_unlocked( VSIGZipHandle* poHandle );
};


/************************************************************************/
/*                            Duplicate()                               */
/************************************************************************/

VSIGZipHandle* VSIGZipHandle::Duplicate()
{
    CPLAssert (offset == 0);
    CPLAssert (compressed_size != 0);
    CPLAssert (pszBaseFileName != NULL);

    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszBaseFileName );

    VSIVirtualHandle* poNewBaseHandle =
        poFSHandler->Open( pszBaseFileName, "rb" );

    if (poNewBaseHandle == NULL)
        return NULL;

    VSIGZipHandle* poHandle = new VSIGZipHandle(poNewBaseHandle,
                                                pszBaseFileName,
                                                0,
                                                compressed_size,
                                                uncompressed_size);

    poHandle->nLastReadOffset = nLastReadOffset;

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
        VSIFCloseL((VSILFILE*)poBaseHandle);
    poBaseHandle = NULL;
}

/************************************************************************/
/*                       VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::VSIGZipHandle(VSIVirtualHandle* poBaseHandle,
                             const char* pszBaseFileName,
                             vsi_l_offset offset,
                             vsi_l_offset compressed_size,
                             vsi_l_offset uncompressed_size,
                             unsigned int expected_crc,
                             int transparent)
{
    this->poBaseHandle = poBaseHandle;
    this->expected_crc = expected_crc;
    this->pszBaseFileName = (pszBaseFileName) ? CPLStrdup(pszBaseFileName) : NULL;
    bCanSaveInfo = TRUE;
    this->offset = offset;
    if (compressed_size || transparent)
    {
        this->compressed_size = compressed_size;
    }
    else
    {
        VSIFSeekL((VSILFILE*)poBaseHandle, 0, SEEK_END);
        this->compressed_size = VSIFTellL((VSILFILE*)poBaseHandle) - offset;
        compressed_size = this->compressed_size;
    }
    this->uncompressed_size = uncompressed_size;
    offsetEndCompressedData = offset + compressed_size;

    VSIFSeekL((VSILFILE*)poBaseHandle, offset, SEEK_SET);

    nLastReadOffset = 0;
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
    startOff = VSIFTellL((VSILFILE*)poBaseHandle) - stream.avail_in;

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
/*                      SaveInfo_unlocked()                             */
/************************************************************************/

void VSIGZipHandle::SaveInfo_unlocked()
{
    if (pszBaseFileName && bCanSaveInfo)
    {
        VSIFilesystemHandler *poFSHandler = 
            VSIFileManager::GetHandler( "/vsigzip/" );
        ((VSIGZipFilesystemHandler*)poFSHandler)->SaveInfo_unlocked(this);
        bCanSaveInfo = FALSE;
    }
}

/************************************************************************/
/*                      ~VSIGZipHandle()                                */
/************************************************************************/

VSIGZipHandle::~VSIGZipHandle()
{
    
    if (pszBaseFileName && bCanSaveInfo)
    {
        VSIFilesystemHandler *poFSHandler = 
            VSIFileManager::GetHandler( "/vsigzip/" );
        ((VSIGZipFilesystemHandler*)poFSHandler)->SaveInfo(this);
    }
    
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
    CPLFree(pszBaseFileName);

    if (poBaseHandle)
        VSIFCloseL((VSILFILE*)poBaseHandle);
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
        len = (uInt)VSIFReadL(inbuf + len, 1, Z_BUFSIZE >> len, (VSILFILE*)poBaseHandle);
        if (ENABLE_DEBUG) CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                    VSIFTellL((VSILFILE*)poBaseHandle), offsetEndCompressedData);
        if (VSIFTellL((VSILFILE*)poBaseHandle) > offsetEndCompressedData)
        {
            len = len + (uInt) (offsetEndCompressedData - VSIFTellL((VSILFILE*)poBaseHandle));
            VSIFSeekL((VSILFILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
        }
        if (len == 0 /* && ferror(file)*/)
        {
            if (VSIFTellL((VSILFILE*)poBaseHandle) != offsetEndCompressedData)
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
        stream.avail_in = (uInt)VSIFReadL(inbuf, 1, Z_BUFSIZE, (VSILFILE*)poBaseHandle);
        if (ENABLE_DEBUG) CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                   VSIFTellL((VSILFILE*)poBaseHandle), offsetEndCompressedData);
        if (VSIFTellL((VSILFILE*)poBaseHandle) > offsetEndCompressedData)
        {
            stream.avail_in = stream.avail_in + (uInt) (offsetEndCompressedData - VSIFTellL((VSILFILE*)poBaseHandle));
            VSIFSeekL((VSILFILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
        }
        if (stream.avail_in == 0) {
            z_eof = 1;
            if (VSIFTellL((VSILFILE*)poBaseHandle) != offsetEndCompressedData)
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
    return VSIFSeekL((VSILFILE*)poBaseHandle, startOff, SEEK_SET);
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
    vsi_l_offset original_offset = offset;
    int original_nWhence = whence;

    z_eof = 0;
    if (ENABLE_DEBUG) CPLDebug("GZIP", "Seek(" CPL_FRMT_GUIB ",%d)", offset, whence);

    if (transparent)
    {
        stream.avail_in = 0;
        stream.next_in = inbuf;
        if (whence == SEEK_CUR)
        {
            if (out + offset > compressed_size)
            {
                CPL_VSIL_GZ_RETURN(-1);
                return -1L;
            }

            offset = startOff + out + offset;
        }
        else if (whence == SEEK_SET)
        {
            if (offset > compressed_size)
            {
                CPL_VSIL_GZ_RETURN(-1);
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
                CPL_VSIL_GZ_RETURN(-1);
                return -1L;
            }

            offset = startOff + compressed_size - offset;
        }
        else
        {
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
        }
        if (VSIFSeekL((VSILFILE*)poBaseHandle, offset, SEEK_SET) < 0)
        {
            CPL_VSIL_GZ_RETURN(-1);
            return -1L;
        }

        in = out = offset - startOff;
        if (ENABLE_DEBUG) CPLDebug("GZIP", "return " CPL_FRMT_GUIB, in);
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
        CPL_VSIL_GZ_RETURN(-1);
        return -1L;
    }

    /* Rest of function is for reading only */

    /* compute absolute position */
    if (whence == SEEK_CUR) {
        offset += out;
    }

    /* For a negative seek, rewind and use positive seek */
    if (offset >= out) {
        offset -= out;
    } else if (gzrewind() < 0) {
            CPL_VSIL_GZ_RETURN(-1);
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
            VSIFSeekL((VSILFILE*)poBaseHandle, snapshots[i].uncompressed_pos, SEEK_SET);
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
            CPL_VSIL_GZ_RETURN(-1);
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
            //CPL_VSIL_GZ_RETURN(-1);
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
    {
        uncompressed_size = out;

        if (pszBaseFileName)
        {
            CPLString osCacheFilename (pszBaseFileName);
            osCacheFilename += ".properties";

            /* Write a .properties file to avoid seeking next time */
            VSILFILE* fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "wb");
            if (fpCacheLength)
            {
                char* pszFirstNonSpace;
                char szBuffer[32];
                szBuffer[31] = 0;

                CPLPrintUIntBig(szBuffer, compressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ') pszFirstNonSpace ++;
                VSIFPrintfL(fpCacheLength, "compressed_size=%s\n", pszFirstNonSpace);

                CPLPrintUIntBig(szBuffer, uncompressed_size, 31);
                pszFirstNonSpace = szBuffer;
                while (*pszFirstNonSpace == ' ') pszFirstNonSpace ++;
                VSIFPrintfL(fpCacheLength, "uncompressed_size=%s\n", pszFirstNonSpace);

                VSIFCloseL(fpCacheLength);
            }
        }
    }

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
        z_eof = 1; /* to avoid infinite loop in reader code */
        in = 0;
        CPL_VSIL_GZ_RETURN(0);
        return 0;
    }
    if  ((z_eof && in == 0) || z_err == Z_STREAM_END)
    {
        z_eof = 1;
        in = 0;
        if (ENABLE_DEBUG) CPLDebug("GZIP", "Read: Eof");
        return 0;  /* EOF */
    }

    next_out = (Byte*)buf;
    stream.next_out = (Bytef*)buf;
    stream.avail_out = len;

    while  (stream.avail_out != 0) {

        if  (transparent) {
            /* Copy first the lookahead bytes: */
            uInt nRead = 0;
            uInt n = stream.avail_in;
            if (n > stream.avail_out) n = stream.avail_out;
            if (n > 0) {
                memcpy (stream.next_out, stream.next_in, n);
                next_out += n;
                stream.next_out = next_out;
                stream.next_in   += n;
                stream.avail_out -= n;
                stream.avail_in  -= n;
                nRead += n;
            }
            if  (stream.avail_out > 0) {
                uInt nToRead = (uInt) MIN(compressed_size - (in + nRead), stream.avail_out);
                uInt nReadFromFile =
                    (uInt)VSIFReadL(next_out, 1, nToRead, (VSILFILE*)poBaseHandle);
                stream.avail_out -= nReadFromFile;
                nRead += nReadFromFile;
            }
            in  += nRead;
            out += nRead;
            if (nRead < len) z_eof = 1;
            if (ENABLE_DEBUG) CPLDebug("GZIP", "Read return %d", (int)(nRead / nSize));
            return (int)nRead / nSize;
        }
        if  (stream.avail_in == 0 && !z_eof)
        {
            vsi_l_offset uncompressed_pos = VSIFTellL((VSILFILE*)poBaseHandle);
            GZipSnapshot* snapshot = &snapshots[(uncompressed_pos - startOff) / snapshot_byte_interval];
            if (snapshot->uncompressed_pos == 0)
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

                if (out > nLastReadOffset)
                    nLastReadOffset = out;
            }

            errno = 0;
            stream.avail_in = (uInt)VSIFReadL(inbuf, 1, Z_BUFSIZE, (VSILFILE*)poBaseHandle);
            if (ENABLE_DEBUG)
                CPLDebug("GZIP", CPL_FRMT_GUIB " " CPL_FRMT_GUIB,
                                 VSIFTellL((VSILFILE*)poBaseHandle), offsetEndCompressedData);
            if (VSIFTellL((VSILFILE*)poBaseHandle) > offsetEndCompressedData)
            {
                if (ENABLE_DEBUG) CPLDebug("GZIP", "avail_in before = %d", stream.avail_in);
                stream.avail_in = stream.avail_in + (uInt) (offsetEndCompressedData - VSIFTellL((VSILFILE*)poBaseHandle));
                VSIFSeekL((VSILFILE*)poBaseHandle, offsetEndCompressedData, SEEK_SET);
                if (ENABLE_DEBUG) CPLDebug("GZIP", "avail_in after = %d", stream.avail_in);
            }
            if  (stream.avail_in == 0) {
                z_eof = 1;
                if (VSIFTellL((VSILFILE*)poBaseHandle) != offsetEndCompressedData)
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

        if  (z_err == Z_STREAM_END && compressed_size != 2 ) {
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
        z_eof = 1;
        in = 0;
        CPL_VSIL_GZ_RETURN(0);
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

size_t VSIGZipHandle::Write( CPL_UNUSED const void *pBuffer,
                             CPL_UNUSED size_t nSize,
                             CPL_UNUSED size_t nMemb )
{
    CPLError(CE_Failure, CPLE_NotSupported, "VSIFWriteL is not supported on GZip streams");
    return 0;
}

/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/


int VSIGZipHandle::Eof()
{
    if (ENABLE_DEBUG) CPLDebug("GZIP", "Eof()");
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

class VSIGZipWriteHandle : public VSIVirtualHandle
{
    VSIVirtualHandle*  poBaseHandle;
    z_stream           sStream;
    Byte              *pabyInBuf;
    Byte              *pabyOutBuf;
    bool               bCompressActive;
    vsi_l_offset       nCurOffset;
    GUInt32            nCRC;
    int                bRegularZLib;
    int                bAutoCloseBaseHandle;

  public:

    VSIGZipWriteHandle(VSIVirtualHandle* poBaseHandle, int bRegularZLib, int bAutoCloseBaseHandleIn);

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

VSIGZipWriteHandle::VSIGZipWriteHandle( VSIVirtualHandle *poBaseHandle,
                                        int bRegularZLibIn,
                                        int bAutoCloseBaseHandleIn )

{
    nCurOffset = 0;

    this->poBaseHandle = poBaseHandle;
    bRegularZLib = bRegularZLibIn;
    bAutoCloseBaseHandle = bAutoCloseBaseHandleIn;

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
                      Z_DEFLATED, (bRegularZLib) ? MAX_WBITS : -MAX_WBITS, 8,
                      Z_DEFAULT_STRATEGY ) != Z_OK )
        bCompressActive = false;
    else
    {
        if (!bRegularZLib)
        {
            char header[11];

            /* Write a very simple .gz header:
            */
            sprintf( header, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
                    Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/,
                    0x03 );
            poBaseHandle->Write( header, 1, 10 );
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
    return new VSIGZipWriteHandle( poBaseHandle, bRegularZLibIn, bAutoCloseBaseHandle );
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

        if( !bRegularZLib )
        {
            GUInt32 anTrailer[2];

            anTrailer[0] = CPL_LSBWORD32( nCRC );
            anTrailer[1] = CPL_LSBWORD32( (GUInt32) nCurOffset );

            poBaseHandle->Write( anTrailer, 1, 8 );
        }

        if( bAutoCloseBaseHandle )
        {
            poBaseHandle->Close();

            delete poBaseHandle;
        }

        bCompressActive = false;
    }

    return 0;
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t VSIGZipWriteHandle::Read( CPL_UNUSED void *pBuffer,
                                 CPL_UNUSED size_t nSize,
                                 CPL_UNUSED size_t nMemb )
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


/************************************************************************/
/*                   VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::VSIGZipFilesystemHandler()
{
    hMutex = NULL;

    poHandleLastGZipFile = NULL;
}

/************************************************************************/
/*                  ~VSIGZipFilesystemHandler()                         */
/************************************************************************/

VSIGZipFilesystemHandler::~VSIGZipFilesystemHandler()
{
    if (poHandleLastGZipFile)
        delete poHandleLastGZipFile;

    if( hMutex != NULL )
        CPLDestroyMutex( hMutex );
    hMutex = NULL;
}

/************************************************************************/
/*                            SaveInfo()                                */
/************************************************************************/

void VSIGZipFilesystemHandler::SaveInfo(  VSIGZipHandle* poHandle )
{
    CPLMutexHolder oHolder(&hMutex);
    SaveInfo_unlocked(poHandle);
}

void VSIGZipFilesystemHandler::SaveInfo_unlocked(  VSIGZipHandle* poHandle )
{
    CPLAssert(poHandle->GetBaseFileName() != NULL);

    if (poHandleLastGZipFile &&
        strcmp(poHandleLastGZipFile->GetBaseFileName(), poHandle->GetBaseFileName()) == 0)
    {
        if (poHandle->GetLastReadOffset() > poHandleLastGZipFile->GetLastReadOffset())
        {
            VSIGZipHandle* poTmp = poHandleLastGZipFile;
            poHandleLastGZipFile = NULL;
            poTmp->SaveInfo_unlocked();
            delete poTmp;
            poHandleLastGZipFile = poHandle->Duplicate();
            poHandleLastGZipFile->CloseBaseHandle();
        }
    }
    else
    {
        VSIGZipHandle* poTmp = poHandleLastGZipFile;
        poHandleLastGZipFile = NULL;
        if( poTmp )
        {
            poTmp->SaveInfo_unlocked();
            delete poTmp;
        }
        poHandleLastGZipFile = poHandle->Duplicate();
        poHandleLastGZipFile->CloseBaseHandle();
    }
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
            return new VSIGZipWriteHandle( poVirtualHandle, strchr(pszAccess, 'z') != NULL, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we are in the read access case.                       */
/* -------------------------------------------------------------------- */

    VSIGZipHandle* poGZIPHandle = OpenGZipReadOnly(pszFilename, pszAccess);
    if (poGZIPHandle)
        /* Wrap the VSIGZipHandle inside a buffered reader that will */
        /* improve dramatically performance when doing small backward */
        /* seeks */
        return VSICreateBufferedReaderHandle(poGZIPHandle);
    else
        return NULL;
}

/************************************************************************/
/*                          OpenGZipReadOnly()                          */
/************************************************************************/

VSIGZipHandle* VSIGZipFilesystemHandler::OpenGZipReadOnly( const char *pszFilename, 
                                                      const char *pszAccess)
{
    VSIFilesystemHandler *poFSHandler = 
        VSIFileManager::GetHandler( pszFilename + strlen("/vsigzip/"));

    CPLMutexHolder oHolder(&hMutex);

    if (poHandleLastGZipFile != NULL &&
        strcmp(pszFilename + strlen("/vsigzip/"), poHandleLastGZipFile->GetBaseFileName()) == 0 &&
        EQUAL(pszAccess, "rb"))
    {
        VSIGZipHandle* poHandle = poHandleLastGZipFile->Duplicate();
        if (poHandle)
            return poHandle;
    }

    unsigned char signature[2];

    VSIVirtualHandle* poVirtualHandle =
        poFSHandler->Open( pszFilename + strlen("/vsigzip/"), "rb" );

    if (poVirtualHandle == NULL)
        return NULL;

    if (VSIFReadL(signature, 1, 2, (VSILFILE*)poVirtualHandle) != 2 ||
        signature[0] != gz_magic[0] || signature[1] != gz_magic[1])
    {
        delete poVirtualHandle;
        return NULL;
    }

    if (poHandleLastGZipFile)
    {
        poHandleLastGZipFile->SaveInfo_unlocked();
        delete poHandleLastGZipFile;
        poHandleLastGZipFile = NULL;
    }

    return new VSIGZipHandle(poVirtualHandle, pszFilename + strlen("/vsigzip/"));
}

/************************************************************************/
/*                                Stat()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Stat( const char *pszFilename,
                                    VSIStatBufL *pStatBuf,
                                    int nFlags )
{
    CPLMutexHolder oHolder(&hMutex);

    memset(pStatBuf, 0, sizeof(VSIStatBufL));

    if (poHandleLastGZipFile != NULL &&
        strcmp(pszFilename+strlen("/vsigzip/"), poHandleLastGZipFile->GetBaseFileName()) == 0)
    {
        if (poHandleLastGZipFile->GetUncompressedSize() != 0)
        {
            pStatBuf->st_mode = S_IFREG;
            pStatBuf->st_size = poHandleLastGZipFile->GetUncompressedSize();
            return 0;
        }
    }

    /* Begin by doing a stat on the real file */
    int ret = VSIStatExL(pszFilename+strlen("/vsigzip/"), pStatBuf, nFlags);

    if (ret == 0 && (nFlags & VSI_STAT_SIZE_FLAG))
    {
        CPLString osCacheFilename(pszFilename+strlen("/vsigzip/"));
        osCacheFilename += ".properties";

        /* Can we save a bit of seeking by using a .properties file ? */
        VSILFILE* fpCacheLength = VSIFOpenL(osCacheFilename.c_str(), "rb");
        if (fpCacheLength)
        {
            const char* pszLine;
            GUIntBig nCompressedSize = 0;
            GUIntBig nUncompressedSize = 0;
            while ((pszLine = CPLReadLineL(fpCacheLength)) != NULL)
            {
                if (EQUALN(pszLine, "compressed_size=", strlen("compressed_size=")))
                {
                    const char* pszBuffer = pszLine + strlen("compressed_size=");
                    nCompressedSize = 
                            CPLScanUIntBig(pszBuffer, strlen(pszBuffer));
                }
                else if (EQUALN(pszLine, "uncompressed_size=", strlen("uncompressed_size=")))
                {
                    const char* pszBuffer = pszLine + strlen("uncompressed_size=");
                    nUncompressedSize =
                             CPLScanUIntBig(pszBuffer, strlen(pszBuffer));
                }
            }

            VSIFCloseL(fpCacheLength);

            if (nCompressedSize == (GUIntBig) pStatBuf->st_size)
            {
                /* Patch with the uncompressed size */
                pStatBuf->st_size = nUncompressedSize;

                VSIGZipHandle* poHandle =
                    VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename, "rb");
                if (poHandle)
                {
                    poHandle->SetUncompressedSize(nUncompressedSize);
                    SaveInfo_unlocked(poHandle);
                    delete poHandle;
                }

                return ret;
            }
        }

        /* No, then seek at the end of the data (slow) */
        VSIGZipHandle* poHandle =
                VSIGZipFilesystemHandler::OpenGZipReadOnly(pszFilename, "rb");
        if (poHandle)
        {
            GUIntBig uncompressed_size;
            poHandle->Seek(0, SEEK_END);
            uncompressed_size = (GUIntBig) poHandle->Tell();
            poHandle->Seek(0, SEEK_SET);

            /* Patch with the uncompressed size */
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

int VSIGZipFilesystemHandler::Unlink( CPL_UNUSED const char *pszFilename )
{
    return -1;
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

int VSIGZipFilesystemHandler::Rename( CPL_UNUSED const char *oldpath,
                                      CPL_UNUSED const char *newpath )
{
    return -1;
}

/************************************************************************/
/*                               Mkdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Mkdir( CPL_UNUSED const char *pszDirname,
                                     CPL_UNUSED long nMode )
{
    return -1;
}
/************************************************************************/
/*                               Rmdir()                                */
/************************************************************************/

int VSIGZipFilesystemHandler::Rmdir( CPL_UNUSED const char *pszDirname )
{
    return -1;
}

/************************************************************************/
/*                             ReadDir()                                */
/************************************************************************/

char** VSIGZipFilesystemHandler::ReadDir( CPL_UNUSED const char *pszDirname )
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
 * @since GDAL 1.6.0
 */

void VSIInstallGZipFileHandler(void)
{
    VSIFileManager::InstallHandler( "/vsigzip/", new VSIGZipFilesystemHandler );
}


/************************************************************************/
/* ==================================================================== */
/*                         VSIZipEntryFileOffset                        */
/* ==================================================================== */
/************************************************************************/

class VSIZipEntryFileOffset : public VSIArchiveEntryFileOffset
{
public:
        unz_file_pos file_pos;

        VSIZipEntryFileOffset(unz_file_pos file_pos)
        {
            this->file_pos.pos_in_zip_directory = file_pos.pos_in_zip_directory;
            this->file_pos.num_of_file = file_pos.num_of_file;
        }
};

/************************************************************************/
/* ==================================================================== */
/*                             VSIZipReader                             */
/* ==================================================================== */
/************************************************************************/

class VSIZipReader : public VSIArchiveReader
{
    private:
        unzFile unzF;
        unz_file_pos file_pos;
        GUIntBig nNextFileSize;
        CPLString osNextFileName;
        GIntBig nModifiedTime;

        void SetInfo();

    public:
        VSIZipReader(const char* pszZipFileName);
        virtual ~VSIZipReader();

        int IsValid() { return unzF != NULL; }

        unzFile GetUnzFileHandle() { return unzF; }

        virtual int GotoFirstFile();
        virtual int GotoNextFile();
        virtual VSIArchiveEntryFileOffset* GetFileOffset() { return new VSIZipEntryFileOffset(file_pos); }
        virtual GUIntBig GetFileSize() { return nNextFileSize; }
        virtual CPLString GetFileName() { return osNextFileName; }
        virtual GIntBig GetModifiedTime() { return nModifiedTime; }
        virtual int GotoFileOffset(VSIArchiveEntryFileOffset* pOffset);
};


/************************************************************************/
/*                           VSIZipReader()                             */
/************************************************************************/

VSIZipReader::VSIZipReader(const char* pszZipFileName)
{
    unzF = cpl_unzOpen(pszZipFileName);
    nNextFileSize = 0;
    nModifiedTime = 0;
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

void VSIZipReader::SetInfo()
{
    char fileName[8193];
    unz_file_info file_info;
    cpl_unzGetCurrentFileInfo (unzF, &file_info, fileName, sizeof(fileName) - 1, NULL, 0, NULL, 0);
    fileName[sizeof(fileName) - 1] = '\0';
    osNextFileName = fileName;
    nNextFileSize = file_info.uncompressed_size;
    struct tm brokendowntime;
    brokendowntime.tm_sec = file_info.tmu_date.tm_sec;
    brokendowntime.tm_min = file_info.tmu_date.tm_min;
    brokendowntime.tm_hour = file_info.tmu_date.tm_hour;
    brokendowntime.tm_mday = file_info.tmu_date.tm_mday;
    brokendowntime.tm_mon = file_info.tmu_date.tm_mon;
    brokendowntime.tm_year = file_info.tmu_date.tm_year - 1900; /* the minizip conventions differs from the Unix one */
    nModifiedTime = CPLYMDHMSToUnixTime(&brokendowntime);

    cpl_unzGetFilePos(unzF, &this->file_pos);
}

/************************************************************************/
/*                           GotoNextFile()                             */
/************************************************************************/

int VSIZipReader::GotoNextFile()
{
    if (cpl_unzGoToNextFile(unzF) != UNZ_OK)
        return FALSE;

    SetInfo();

    return TRUE;
}

/************************************************************************/
/*                          GotoFirstFile()                             */
/************************************************************************/

int VSIZipReader::GotoFirstFile()
{
    if (cpl_unzGoToFirstFile(unzF) != UNZ_OK)
        return FALSE;

    SetInfo();

    return TRUE;
}

/************************************************************************/
/*                         GotoFileOffset()                             */
/************************************************************************/

int VSIZipReader::GotoFileOffset(VSIArchiveEntryFileOffset* pOffset)
{
    VSIZipEntryFileOffset* pZipEntryOffset = (VSIZipEntryFileOffset*)pOffset;
    if( cpl_unzGoToFilePos(unzF, &(pZipEntryOffset->file_pos)) != UNZ_OK )
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

class VSIZipFilesystemHandler : public VSIArchiveFilesystemHandler 
{
    std::map<CPLString, VSIZipWriteHandle*> oMapZipWriteHandles;
    VSIVirtualHandle *OpenForWrite_unlocked( const char *pszFilename,
                                            const char *pszAccess );

public:
    virtual ~VSIZipFilesystemHandler();
    
    virtual const char* GetPrefix() { return "/vsizip"; }
    virtual std::vector<CPLString> GetExtensions();
    virtual VSIArchiveReader* CreateReader(const char* pszZipFileName);

    virtual VSIVirtualHandle *Open( const char *pszFilename, 
                                    const char *pszAccess);

    virtual VSIVirtualHandle *OpenForWrite( const char *pszFilename,
                                            const char *pszAccess );

    virtual int      Mkdir( const char *pszDirname, long nMode );
    virtual char   **ReadDir( const char *pszDirname );
    virtual int      Stat( const char *pszFilename, VSIStatBufL *pStatBuf, int nFlags );

    void RemoveFromMap(VSIZipWriteHandle* poHandle);
};

/************************************************************************/
/* ==================================================================== */
/*                       VSIZipWriteHandle                              */
/* ==================================================================== */
/************************************************************************/

class VSIZipWriteHandle : public VSIVirtualHandle
{
   VSIZipFilesystemHandler *poFS;
   void                    *hZIP;
   VSIZipWriteHandle       *poChildInWriting;
   VSIZipWriteHandle       *poParent;
   int                      bAutoDeleteParent;
   vsi_l_offset             nCurOffset;

  public:

    VSIZipWriteHandle(VSIZipFilesystemHandler* poFS,
                      void *hZIP,
                      VSIZipWriteHandle* poParent);

    ~VSIZipWriteHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();

    void  StartNewFile(VSIZipWriteHandle* poSubFile);
    void  StopCurrentFile();
    void* GetHandle() { return hZIP; }
    VSIZipWriteHandle* GetChildInWriting() { return poChildInWriting; };
    void SetAutoDeleteParent() { bAutoDeleteParent = TRUE; }
};

/************************************************************************/
/*                      ~VSIZipFilesystemHandler()                      */
/************************************************************************/

VSIZipFilesystemHandler::~VSIZipFilesystemHandler()
{
    std::map<CPLString,VSIZipWriteHandle*>::const_iterator iter;

    for( iter = oMapZipWriteHandles.begin(); iter != oMapZipWriteHandles.end(); ++iter )
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

    /* Add to zip FS handler extensions array additional extensions */
    /* listed in CPL_VSIL_ZIP_ALLOWED_EXTENSIONS config option. */
    /* The extensions divided by comma */
    const char* pszAllowedExtensions =
        CPLGetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", NULL);
    if (pszAllowedExtensions)
    {
        char** papszExtensions = CSLTokenizeString2(pszAllowedExtensions, ", ", 0);
        for (int i = 0; papszExtensions[i] != NULL; i++)
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

VSIArchiveReader* VSIZipFilesystemHandler::CreateReader(const char* pszZipFileName)
{
    VSIZipReader* poReader = new VSIZipReader(pszZipFileName);

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

VSIVirtualHandle* VSIZipFilesystemHandler::Open( const char *pszFilename, 
                                                 const char *pszAccess)
{
    char* zipFilename;
    CPLString osZipInFileName;

    if (strchr(pszAccess, 'w') != NULL)
    {
        return OpenForWrite(pszFilename, pszAccess);
    }

    if (strchr(pszAccess, '+') != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Random access not supported for /vsizip");
        return NULL;
    }

    zipFilename = SplitFilename(pszFilename, osZipInFileName, TRUE);
    if (zipFilename == NULL)
        return NULL;

    {
        CPLMutexHolder oHolder(&hMutex);
        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return NULL;
        }
    }

    VSIArchiveReader* poReader = OpenArchiveFile(zipFilename, osZipInFileName);
    if (poReader == NULL)
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
        delete poReader;
        return NULL;
    }

    unzFile unzF = ((VSIZipReader*)poReader)->GetUnzFileHandle();

    if( cpl_unzOpenCurrentFile(unzF) != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "cpl_unzOpenCurrentFile() failed");
        delete poReader;
        return NULL;
    }

    uLong64 pos = cpl_unzGetCurrentFileZStreamPos(unzF);

    unz_file_info file_info;
    if( cpl_unzGetCurrentFileInfo (unzF, &file_info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "cpl_unzGetCurrentFileInfo() failed");
        cpl_unzCloseCurrentFile(unzF);
        delete poReader;
        return NULL;
    }

    cpl_unzCloseCurrentFile(unzF);

    delete poReader;

    VSIGZipHandle* poGZIPHandle = new VSIGZipHandle(poVirtualHandle,
                             NULL,
                             pos,
                             file_info.compressed_size,
                             file_info.uncompressed_size,
                             file_info.crc,
                             file_info.compression_method == 0);
    /* Wrap the VSIGZipHandle inside a buffered reader that will */
    /* improve dramatically performance when doing small backward */
    /* seeks */
    return VSICreateBufferedReaderHandle(poGZIPHandle);
}

/************************************************************************/
/*                                Mkdir()                               */
/************************************************************************/

int VSIZipFilesystemHandler::Mkdir( const char *pszDirname, CPL_UNUSED long nMode )
{
    CPLString osDirname = pszDirname;
    if (osDirname.size() != 0 && osDirname[osDirname.size() - 1] != '/')
        osDirname += "/";
    VSIVirtualHandle* poZIPHandle = OpenForWrite(osDirname, "wb");
    if (poZIPHandle == NULL)
        return -1;
    delete poZIPHandle;
    return 0;
}

/************************************************************************/
/*                                ReadDir()                             */
/************************************************************************/

char **VSIZipFilesystemHandler::ReadDir( const char *pszDirname )
{
    CPLString osInArchiveSubDir;
    char* zipFilename = SplitFilename(pszDirname, osInArchiveSubDir, TRUE);
    if (zipFilename == NULL)
        return NULL;

    {
        CPLMutexHolder oHolder(&hMutex);

        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read a zip file being written");
            CPLFree(zipFilename);
            return NULL;
        }
    }
    CPLFree(zipFilename);

    return VSIArchiveFilesystemHandler::ReadDir(pszDirname);
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
    if (zipFilename == NULL)
        return -1;

    {
        CPLMutexHolder oHolder(&hMutex);

        if (oMapZipWriteHandles.find(zipFilename) != oMapZipWriteHandles.end() )
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

void VSIZipFilesystemHandler::RemoveFromMap(VSIZipWriteHandle* poHandle)
{
    CPLMutexHolder oHolder( &hMutex );
    std::map<CPLString,VSIZipWriteHandle*>::iterator iter;

    for( iter = oMapZipWriteHandles.begin();
         iter != oMapZipWriteHandles.end(); ++iter )
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

VSIVirtualHandle* VSIZipFilesystemHandler::OpenForWrite( const char *pszFilename,
                                                         const char *pszAccess)
{
    CPLMutexHolder oHolder( &hMutex );
    return OpenForWrite_unlocked(pszFilename, pszAccess);
}


VSIVirtualHandle* VSIZipFilesystemHandler::OpenForWrite_unlocked( const char *pszFilename,
                                                         const char *pszAccess)
{
    char* zipFilename;
    CPLString osZipInFileName;
    
    zipFilename = SplitFilename(pszFilename, osZipInFileName, FALSE);
    if (zipFilename == NULL)
        return NULL;
    CPLString osZipFilename = zipFilename;
    CPLFree(zipFilename);
    zipFilename = NULL;

    /* Invalidate cached file list */
    std::map<CPLString,VSIArchiveContent*>::iterator iter = oFileList.find(osZipFilename);
    if (iter != oFileList.end())
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

        oFileList.erase(iter);
    }

    VSIZipWriteHandle* poZIPHandle;

    if (oMapZipWriteHandles.find(osZipFilename) != oMapZipWriteHandles.end() )
    {
        if (strchr(pszAccess, '+') != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Random access not supported for writable file in /vsizip");
            return NULL;
        }

        poZIPHandle = oMapZipWriteHandles[osZipFilename];

        if (poZIPHandle->GetChildInWriting() != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create %s while another file is being written in the .zip",
                     osZipInFileName.c_str());
            return NULL;
        }

        poZIPHandle->StopCurrentFile();

        /* Re-add path separator when creating directories */
        char chLastChar = pszFilename[strlen(pszFilename) - 1];
        if (chLastChar == '/' || chLastChar == '\\')
            osZipInFileName += chLastChar;

        if (CPLCreateFileInZip(poZIPHandle->GetHandle(),
                               osZipInFileName, NULL) != CE_None)
            return NULL;

        VSIZipWriteHandle* poChildHandle =
            new VSIZipWriteHandle(this, NULL, poZIPHandle);

        poZIPHandle->StartNewFile(poChildHandle);

        return poChildHandle;
    }
    else
    {
        char** papszOptions = NULL;
        if ((strchr(pszAccess, '+') && osZipInFileName.size() == 0) ||
             osZipInFileName.size() != 0)
        {
            VSIStatBufL sBuf;
            if (VSIStatExL(osZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0)
                papszOptions = CSLAddNameValue(papszOptions, "APPEND", "TRUE");
        }

        void* hZIP = CPLCreateZip(osZipFilename, papszOptions);
        CSLDestroy(papszOptions);

        if (hZIP == NULL)
            return NULL;

        oMapZipWriteHandles[osZipFilename] =
            new VSIZipWriteHandle(this, hZIP, NULL);

        if (osZipInFileName.size() != 0)
        {
            VSIZipWriteHandle* poRes =
                (VSIZipWriteHandle*)OpenForWrite_unlocked(pszFilename, pszAccess);
            if (poRes == NULL)
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

VSIZipWriteHandle::VSIZipWriteHandle(VSIZipFilesystemHandler* poFS,
                                     void* hZIP,
                                     VSIZipWriteHandle* poParent)
{
    this->poFS = poFS;
    this->hZIP = hZIP;
    this->poParent = poParent;
    poChildInWriting = NULL;
    bAutoDeleteParent = FALSE;
    nCurOffset = 0;
}

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

size_t VSIZipWriteHandle::Read( CPL_UNUSED void *pBuffer,
                                CPL_UNUSED size_t nSize,
                                CPL_UNUSED size_t nMemb )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFReadL() is not supported on writable Zip files");
    return 0;
}

/************************************************************************/
/*                               Write()                                 */
/************************************************************************/

size_t    VSIZipWriteHandle::Write( const void *pBuffer, size_t nSize, size_t nMemb )
{
    if (poParent == NULL)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VSIFWriteL() is not supported on main Zip file or closed subfiles");
        return 0;
    }

    if (CPLWriteFileInZip( poParent->hZIP, pBuffer, (int)(nSize * nMemb) ) != CE_None)
        return 0;

    nCurOffset +=(int) (nSize * nMemb);

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
    if (poParent)
    {
        CPLCloseFileInZip(poParent->hZIP);
        poParent->poChildInWriting = NULL;
        if (bAutoDeleteParent)
            delete poParent;
        poParent = NULL;
    }
    if (poChildInWriting)
    {
        poChildInWriting->Close();
        poChildInWriting = NULL;
    }
    if (hZIP)
    {
        CPLCloseZip(hZIP);
        hZIP = NULL;

        poFS->RemoveFromMap(this);
    }

    return 0;
}

/************************************************************************/
/*                           StopCurrentFile()                          */
/************************************************************************/

void  VSIZipWriteHandle::StopCurrentFile()
{
    if (poChildInWriting)
        poChildInWriting->Close();
    poChildInWriting = NULL;
}

/************************************************************************/
/*                           StartNewFile()                             */
/************************************************************************/

void  VSIZipWriteHandle::StartNewFile(VSIZipWriteHandle* poSubFile)
{
    poChildInWriting = poSubFile;
}

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
 * The syntax to open a file inside a zip file is /vsizip/path/to/the/file.zip/path/inside/the/zip/file
 * were path/to/the/file.zip is relative or absolute and path/inside/the/zip/file
 * is the relative path to the file inside the archive.
 * 
 * If the path is absolute, it should begin with a / on a Unix-like OS (or C:\ on Windows),
 * so the line looks like /vsizip//home/gdal/...
 * For example gdalinfo /vsizip/myarchive.zip/subdir1/file1.tif
 *
 * Syntaxic sugar : if the .zip file contains only one file located at its root,
 * just mentionning "/vsizip/path/to/the/file.zip" will work
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
 * Additional documentation is to be found at http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
 *
 * @since GDAL 1.6.0
 */

void VSIInstallZipFileHandler(void)
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
                      CPL_UNUSED int nLevel,
                      void* outptr,
                      size_t nOutAvailableBytes,
                      size_t* pnOutBytes )
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK)
    {
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }

    size_t nTmpSize;
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

    strm.avail_in = nBytes;
    strm.next_in = (Bytef*) ptr;
    strm.avail_out = nTmpSize;
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
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = nBytes;
    strm.next_in = (Bytef*) ptr;
    int ret = inflateInit(&strm);
    if (ret != Z_OK)
    {
        if( pnOutBytes != NULL )
            *pnOutBytes = 0;
        return NULL;
    }

    size_t nTmpSize;
    char* pszTmp;
    if( outptr == NULL )
    {
        nTmpSize = 2 * nBytes;
        pszTmp = (char*) VSIMalloc(nTmpSize + 1);
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
        pszTmp = (char*) outptr;
        nTmpSize = nOutAvailableBytes;
    }

    strm.avail_out = nTmpSize;
    strm.next_out = (Bytef*) pszTmp;

    while(TRUE)
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
            char* pszTmpNew = (char*) VSIRealloc(pszTmp, nTmpSize + 1);
            if( pszTmpNew == NULL )
            {
                VSIFree(pszTmp);
                inflateEnd(&strm);
                if( pnOutBytes != NULL )
                    *pnOutBytes = 0;
                return NULL;
            }
            pszTmp = pszTmpNew;
            strm.avail_out = nTmpSize - nAlreadyWritten;
            strm.next_out = (Bytef*) (pszTmp + nAlreadyWritten);
        }
        else
            break;
    }

    if (ret == Z_OK || ret == Z_STREAM_END)
    {
        size_t nOutBytes = nTmpSize - strm.avail_out;
        /* Nul-terminate if possible */
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
