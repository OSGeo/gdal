/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Adjusted minizip "zip.h" include file for zip services.
 *
 * Modified version by Even Rouault. :
 *   - Decoration of symbol names unz* -> cpl_unz*
 *   - Undef EXPORT so that we are sure the symbols are not exported
 *   - Remove old C style function prototypes
 *   - Added CPL* simplified API at bottom.
 *
 *   Original licence available in port/LICENCE_minizip
 *
 *****************************************************************************/

/* zip.h -- IO for compress .zip files using zlib
   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant

   This unzip package allow creates .ZIP file, compatible with PKZip 2.04g
     WinZip, InfoZip tools and compatible.
   Multi volume ZipFile (span) are not supported.
   Encryption compatible with pkzip 2.04g only supported
   Old compressions used by old PKZip 1.x are not supported

  For uncompress .zip file, look at unzip.h

   I WAIT FEEDBACK at mail info@winimage.com
   Visit also http://www.winimage.com/zLibDll/unzip.html for evolution

   Condition of use and distribution are the same than zlib :

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
*/

/* for more info about .ZIP format, see
      http://www.info-zip.org/pub/infozip/doc/appnote-981119-iz.zip
      http://www.info-zip.org/pub/infozip/doc/
   PkWare has also a specification at :
      ftp://ftp.pkware.com/probdesc.zip
*/

#ifndef CPL_MINIZIP_ZIP_H_INCLUDED
#define CPL_MINIZIP_ZIP_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_vsi.h"
#define uLong64 vsi_l_offset
typedef vsi_l_offset ZPOS64_T;

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _ZLIB_H
#include "cpl_zlib_header.h"  // to avoid warnings when including zlib.h
#endif

#ifndef CPL_MINIZIP_IOAPI_H_INCLUDED
#include "cpl_minizip_ioapi.h"
#endif

#define NOCRYPT
#undef ZEXPORT
#define ZEXPORT

#if defined(STRICTZIP) || defined(STRICTZIPUNZIP)
    /* like the STRICT of WIN32, we define a pointer that cannot be converted
        from (void*) without cast */
    typedef struct TagzipFile__
    {
        int unused;
    } zipFile__;
    typedef zipFile__ *zipFile;
#else
typedef voidp zipFile;
#endif

#define ZIP_OK (0)
#define ZIP_EOF (0)
#define ZIP_ERRNO (Z_ERRNO)
#define ZIP_PARAMERROR (-102)
#define ZIP_BADZIPFILE (-103)
#define ZIP_INTERNALERROR (-104)

#ifndef DEF_MEM_LEVEL
#if MAX_MEM_LEVEL >= 8
#define DEF_MEM_LEVEL 8
#else
#define DEF_MEM_LEVEL MAX_MEM_LEVEL
#endif
#endif
    /* default memLevel */

    /* tm_zip contain date/time info */
    typedef struct tm_zip_s
    {
        uInt tm_sec;  /* seconds after the minute - [0,59] */
        uInt tm_min;  /* minutes after the hour - [0,59] */
        uInt tm_hour; /* hours since midnight - [0,23] */
        uInt tm_mday; /* day of the month - [1,31] */
        uInt tm_mon;  /* months since January - [0,11] */
        uInt tm_year; /* years - [1980..2044] */
    } tm_zip;

    typedef struct
    {
        tm_zip tmz_date; /* date in understandable format           */
        uLong dosDate;   /* if dos_date == 0, tmu_date is used      */
        /*    uLong       flag;        */ /* general purpose bit flag        2
                                             bytes */

        uLong internal_fa; /* internal file attributes        2 bytes */
        uLong external_fa; /* external file attributes        4 bytes */
    } zip_fileinfo;

    typedef const char *zipcharpc;

#define APPEND_STATUS_CREATE (0)
#define APPEND_STATUS_CREATEAFTER (1)
#define APPEND_STATUS_ADDINZIP (2)

    extern zipFile ZEXPORT cpl_zipOpen(const char *pathname, int append);
    /*
      Create a zipfile.
         pathname contain on Windows XP a filename like "c:\\zlib\\zlib113.zip"
      or on an Unix computer "zlib/zlib113.zip". if the file pathname exist and
      append==APPEND_STATUS_CREATEAFTER, the zip will be created at the end of
      the file. (useful if the file contain a self extractor code) if the file
      pathname exist and append==APPEND_STATUS_ADDINZIP, we will add files in
      existing zip (be sure you don't add file that doesn't exist) If the
      zipfile cannot be opened, the return value is NULL. Else, the return value
      is a zipFile Handle, usable with other function of this zip package.
    */

    /* Note : there is no delete function for a zipfile.
       If you want delete file in a zipfile, you must open a zipfile, and create
       another. Of course, you can use RAW reading and writing to copy the file
       you did not want delete.
    */

    extern zipFile ZEXPORT cpl_zipOpen2(const char *pathname, int append,
                                        zipcharpc *globalcomment,
                                        zlib_filefunc_def *pzlib_filefunc_def);

    extern int ZEXPORT cpl_zipOpenNewFileInZip(
        zipFile file, const char *filename, const zip_fileinfo *zipfi,
        const void *extrafield_local, uInt size_extrafield_local,
        const void *extrafield_global, uInt size_extrafield_global,
        const char *comment, int method, int level);
    /*
      Open a file in the ZIP for writing.
      filename : the filename in zip (if NULL, '-' without quote will be used
      *zipfi contain supplemental information
      if extrafield_local!=NULL and size_extrafield_local>0, extrafield_local
        contains the extrafield data the local header
      if extrafield_global!=NULL and size_extrafield_global>0, extrafield_global
        contains the extrafield data the local header
      if comment != NULL, comment contain the comment string
      method contain the compression method (0 for store, Z_DEFLATED for
      deflate) level contain the level of compression (can be
      Z_DEFAULT_COMPRESSION)
    */

    extern int ZEXPORT cpl_zipOpenNewFileInZip2(
        zipFile file, const char *filename, const zip_fileinfo *zipfi,
        const void *extrafield_local, uInt size_extrafield_local,
        const void *extrafield_global, uInt size_extrafield_global,
        const char *comment, int method, int level, int raw);

    /*
      Same than zipOpenNewFileInZip, except if raw=1, we write raw file
     */

    extern int ZEXPORT cpl_zipOpenNewFileInZip3(
        zipFile file, const char *filename, const zip_fileinfo *zipfi,
        const void *extrafield_local, uInt size_extrafield_local,
        const void *extrafield_global, uInt size_extrafield_global,
        const char *comment, int method, int level, int raw, int windowBits,
        int memLevel, int strategy, const char *password, uLong crcForCtypting);

    /*
      Same than zipOpenNewFileInZip2, except
        windowBits,memLevel,,strategy : see parameter strategy in deflateInit2
        password : crypting password (NULL for no crypting)
        crcForCtypting : crc of file to compress (needed for crypting)
     */

    extern int ZEXPORT cpl_zipWriteInFileInZip(zipFile file, const void *buf,
                                               unsigned len);
    /*
      Write data in the zipfile
    */

    extern int ZEXPORT cpl_zipCloseFileInZip(zipFile file);
    /*
      Close the current file in the zipfile
    */

    extern int ZEXPORT cpl_zipCloseFileInZipRaw(zipFile file,
                                                ZPOS64_T uncompressed_size,
                                                uLong crc32);
    /*
      Close the current file in the zipfile, for file opened with
        parameter raw=1 in zipOpenNewFileInZip2
      uncompressed_size and crc32 are value for the uncompressed size
    */

    extern int ZEXPORT cpl_zipClose(zipFile file, const char *global_comment);
    /*
      Close the zipfile
    */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* _zip_H */
