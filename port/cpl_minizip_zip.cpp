/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Adjusted minizip "zip.c" source code for zip services.
 *
 * Modified version by Even Rouault. :
 *   - Decoration of symbol names unz* -> cpl_unz*
 *   - Undef EXPORT so that we are sure the symbols are not exported
 *   - Remove old C style function prototypes
 *   - Added CPL* simplified API at bottom.
 *
 *   Original license available in port/LICENCE_minizip
 *
 *****************************************************************************/

/* zip.c -- IO on .zip files using zlib
   Version 1.1, February 14h, 2010
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html
   )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) (
   http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

         Changes
   Oct-2009 - Mathias Svensson - Remove old C style function prototypes
   Oct-2009 - Mathias Svensson - Added Zip64 Support when creating new file
   archives Oct-2009 - Mathias Svensson - Did some code cleanup and refactoring
   to get better overview of some functions. Oct-2009 - Mathias Svensson - Added
   zipRemoveExtraInfoBlock to strip extra field data from its ZIP64 data It is
   used when recreting zip archive with RAW when deleting items from a zip.
                                 ZIP64 data is automatically added to items that
   needs it, and existing ZIP64 data need to be removed. Oct-2009 - Mathias
   Svensson - Added support for BZIP2 as compression mode (bzip2 lib is
   required) Jan-2010 - back to unzip and minizip 1.0 name scheme, with
   compatibility layer

   Copyright (c) 2010-2018, Even Rouault <even dot rouault at spatialys.com>

*/

#include "cpl_port.h"
#include "cpl_minizip_zip.h"

#include <algorithm>
#include <limits>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <time.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minizip_unzip.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi_virtual.h"

#ifdef NO_ERRNO_H
extern int errno;
#else
#include <errno.h>
#endif

#ifndef VERSIONMADEBY
#define VERSIONMADEBY (0x0) /* platform dependent */
#endif

#ifndef Z_BUFSIZE
#define Z_BUFSIZE (16384)
#endif

#ifndef ALLOC
#define ALLOC(size) (malloc(size))
#endif
#ifndef TRYFREE
#define TRYFREE(p)                                                             \
    {                                                                          \
        if (p)                                                                 \
            free(p);                                                           \
    }
#endif

/*
#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)
*/

/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined... */

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef DEF_MEM_LEVEL
#if MAX_MEM_LEVEL >= 8
#define DEF_MEM_LEVEL 8
#else
#define DEF_MEM_LEVEL MAX_MEM_LEVEL
#endif
#endif

CPL_UNUSED static const char zip_copyright[] =
    " zip 1.01 Copyright 1998-2004 Gilles Vollant - "
    "http://www.winimage.com/zLibDll";

#define SIZEDATA_INDATABLOCK (4096 - (4 * 4))

#define LOCALHEADERMAGIC (0x04034b50)
#define CENTRALHEADERMAGIC (0x02014b50)
#define ENDHEADERMAGIC (0x06054b50)
#define ZIP64ENDHEADERMAGIC (0x6064b50)
#define ZIP64ENDLOCHEADERMAGIC (0x7064b50)

#define FLAG_LOCALHEADER_OFFSET (0x06)
#define CRC_LOCALHEADER_OFFSET (0x0e)

#define SIZECENTRALHEADER (0x2e) /* 46 */

typedef struct linkedlist_datablock_internal_s
{
    struct linkedlist_datablock_internal_s *next_datablock;
    uLong avail_in_this_block;
    uLong filled_in_this_block;
    uLong unused;  // For future use and alignment.
    unsigned char data[SIZEDATA_INDATABLOCK];
} linkedlist_datablock_internal;

typedef struct linkedlist_data_s
{
    linkedlist_datablock_internal *first_block;
    linkedlist_datablock_internal *last_block;
} linkedlist_data;

typedef struct
{
    z_stream stream;           /* zLib stream structure for inflate */
    int stream_initialised;    /* 1 is stream is initialized */
    uInt pos_in_buffered_data; /* last written byte in buffered_data */

    ZPOS64_T pos_local_header; /* offset of the local header of the file
                                 currently writing */
    char *local_header;
    uInt size_local_header;
    uInt size_local_header_extrafield;

    char *central_header; /* central header data for the current file */
    uLong size_centralExtra;
    uLong size_centralheader;    /* size of the central header for cur file */
    uLong size_centralExtraFree; /* Extra bytes allocated to the centralheader
                                    but that are not used */
    uLong flag;                  /* flag of the file currently writing */

    // TODO: What is "wr"?  "to"?
    int method;                    /* compression method of file currently wr.*/
    int raw;                       /* 1 for directly writing raw data */
    Byte buffered_data[Z_BUFSIZE]; /* buffer contain compressed data to be
                                        written. */
    uLong dosDate;
    uLong crc32;
    int encrypt;
    ZPOS64_T pos_zip64extrainfo;
    ZPOS64_T totalCompressedData;
    ZPOS64_T totalUncompressedData;
#ifndef NOCRYPT
    unsigned long keys[3]; /* keys defining the pseudo-random sequence */
    const unsigned long *pcrc_32_tab;
    int crypt_header_size;
#endif
} curfile64_info;

typedef struct
{
    zlib_filefunc_def z_filefunc;
    voidpf filestream;           /* IO structure of the zipfile */
    linkedlist_data central_dir; /* datablock with central dir in construction*/
    int in_opened_file_inzip;    /* 1 if a file in the zip is currently writ.*/
    curfile64_info ci;           /* info on the file currently writing */

    ZPOS64_T begin_pos; /* position of the beginning of the zipfile */
    ZPOS64_T add_position_when_writing_offset;
    ZPOS64_T number_entry;
#ifndef NO_ADDFILEINEXISTINGZIP
    char *globalcomment;
#endif
    int use_cpl_io;
    vsi_l_offset vsi_raw_length_before;
    VSIVirtualHandle *vsi_deflate_handle;
    size_t nChunkSize;
    int nThreads;
    size_t nOffsetSize;
    std::vector<uint8_t> *sozip_index;
} zip64_internal;

#ifndef NOCRYPT
#define INCLUDECRYPTINGCODE_IFCRYPTALLOWED
#include "crypt.h"
#endif

static linkedlist_datablock_internal *allocate_new_datablock()
{
    linkedlist_datablock_internal *ldi;
    ldi = static_cast<linkedlist_datablock_internal *>(
        ALLOC(sizeof(linkedlist_datablock_internal)));
    if (ldi != nullptr)
    {
        ldi->next_datablock = nullptr;
        ldi->filled_in_this_block = 0;
        ldi->avail_in_this_block = SIZEDATA_INDATABLOCK;
    }
    return ldi;
}

static void free_datablock(linkedlist_datablock_internal *ldi)
{
    while (ldi != nullptr)
    {
        linkedlist_datablock_internal *ldinext = ldi->next_datablock;
        TRYFREE(ldi);
        ldi = ldinext;
    }
}

static void init_linkedlist(linkedlist_data *ll)
{
    ll->first_block = ll->last_block = nullptr;
}

static void free_linkedlist(linkedlist_data *ll)
{
    free_datablock(ll->first_block);
    ll->first_block = ll->last_block = nullptr;
}

static int add_data_in_datablock(linkedlist_data *ll, const void *buf,
                                 uLong len)
{
    linkedlist_datablock_internal *ldi;
    const unsigned char *from_copy;

    if (ll == nullptr)
        return ZIP_INTERNALERROR;

    if (ll->last_block == nullptr)
    {
        ll->first_block = ll->last_block = allocate_new_datablock();
        if (ll->first_block == nullptr)
            return ZIP_INTERNALERROR;
    }

    ldi = ll->last_block;
    from_copy = reinterpret_cast<const unsigned char *>(buf);

    while (len > 0)
    {
        uInt copy_this;
        uInt i;
        unsigned char *to_copy;

        if (ldi->avail_in_this_block == 0)
        {
            ldi->next_datablock = allocate_new_datablock();
            if (ldi->next_datablock == nullptr)
                return ZIP_INTERNALERROR;
            ldi = ldi->next_datablock;
            ll->last_block = ldi;
        }

        if (ldi->avail_in_this_block < len)
            copy_this = static_cast<uInt>(ldi->avail_in_this_block);
        else
            copy_this = static_cast<uInt>(len);

        to_copy = &(ldi->data[ldi->filled_in_this_block]);

        for (i = 0; i < copy_this; i++)
            *(to_copy + i) = *(from_copy + i);

        ldi->filled_in_this_block += copy_this;
        ldi->avail_in_this_block -= copy_this;
        from_copy += copy_this;
        len -= copy_this;
    }
    return ZIP_OK;
}

/****************************************************************************/

#ifndef NO_ADDFILEINEXISTINGZIP
/* ===========================================================================
   Inputs a long in LSB order to the given file
   nbByte == 1, 2 ,4 or 8 (byte, short or long, ZPOS64_T)
*/

static int zip64local_putValue(const zlib_filefunc_def *pzlib_filefunc_def,
                               voidpf filestream, ZPOS64_T x, int nbByte)
{
    unsigned char buf[8];
    for (int n = 0; n < nbByte; n++)
    {
        buf[n] = static_cast<unsigned char>(x & 0xff);
        x >>= 8;
    }
    if (x != 0)
    { /* data overflow - hack for ZIP64 (X Roche) */
        for (int n = 0; n < nbByte; n++)
        {
            buf[n] = 0xff;
        }
    }

    if (ZWRITE64(*pzlib_filefunc_def, filestream, buf, nbByte) !=
        static_cast<uLong>(nbByte))
        return ZIP_ERRNO;
    else
        return ZIP_OK;
}

static void zip64local_putValue_inmemory(void *dest, ZPOS64_T x, int nbByte)
{
    unsigned char *buf = reinterpret_cast<unsigned char *>(dest);
    for (int n = 0; n < nbByte; n++)
    {
        buf[n] = static_cast<unsigned char>(x & 0xff);
        x >>= 8;
    }

    if (x != 0)
    { /* data overflow - hack for ZIP64 */
        for (int n = 0; n < nbByte; n++)
        {
            buf[n] = 0xff;
        }
    }
}

/****************************************************************************/

static uLong zip64local_TmzDateToDosDate(const tm_zip *ptm)
{
    uLong year = static_cast<uLong>(ptm->tm_year);
    if (year > 1980)
        year -= 1980;
    else if (year > 80)
        year -= 80;
    return static_cast<uLong>(
               ((ptm->tm_mday) + (32 * (ptm->tm_mon + 1)) + (512 * year))
               << 16) |
           ((ptm->tm_sec / 2) + (32 * ptm->tm_min) +
            (2048 * static_cast<uLong>(ptm->tm_hour)));
}

/****************************************************************************/

static int zip64local_getByte(const zlib_filefunc_def *pzlib_filefunc_def,
                              voidpf filestream, int *pi)
{
    unsigned char c = 0;
    const int err =
        static_cast<int>(ZREAD64(*pzlib_filefunc_def, filestream, &c, 1));
    if (err == 1)
    {
        *pi = static_cast<int>(c);
        return ZIP_OK;
    }
    else
    {
        if (ZERROR64(*pzlib_filefunc_def, filestream))
            return ZIP_ERRNO;
        else
            return ZIP_EOF;
    }
}

/* ===========================================================================
   Reads a long in LSB order from the given gz_stream. Sets
*/
static int zip64local_getShort(const zlib_filefunc_def *pzlib_filefunc_def,
                               voidpf filestream, uLong *pX)
{
    int i = 0;
    int err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    uLong x = static_cast<uLong>(i);

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<uLong>(i) << 8;

    if (err == ZIP_OK)
        *pX = x;
    else
        *pX = 0;
    return err;
}

static int zip64local_getLong(const zlib_filefunc_def *pzlib_filefunc_def,
                              voidpf filestream, uLong *pX)
{
    int i = 0;
    int err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    uLong x = static_cast<uLong>(i);

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<uLong>(i) << 8;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<uLong>(i) << 16;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<uLong>(i) << 24;

    if (err == ZIP_OK)
        *pX = x;
    else
        *pX = 0;
    return err;
}

static int zip64local_getLong64(const zlib_filefunc_def *pzlib_filefunc_def,
                                voidpf filestream, ZPOS64_T *pX)
{
    ZPOS64_T x;
    int i = 0;
    int err;

    err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x = static_cast<ZPOS64_T>(i);

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 8;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 16;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 24;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 32;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 40;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 48;

    if (err == ZIP_OK)
        err = zip64local_getByte(pzlib_filefunc_def, filestream, &i);
    x += static_cast<ZPOS64_T>(i) << 56;

    if (err == ZIP_OK)
        *pX = x;
    else
        *pX = 0;

    return err;
}

#ifndef BUFREADCOMMENT
#define BUFREADCOMMENT (0x400)
#endif
/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
static ZPOS64_T
zip64local_SearchCentralDir(const zlib_filefunc_def *pzlib_filefunc_def,
                            voidpf filestream)
{
    ZPOS64_T uMaxBack = 0xffff; /* maximum size of global comment */
    ZPOS64_T uPosFound = 0;

    if (ZSEEK64(*pzlib_filefunc_def, filestream, 0, ZLIB_FILEFUNC_SEEK_END) !=
        0)
        return 0;

    ZPOS64_T uSizeFile = ZTELL64(*pzlib_filefunc_def, filestream);

    if (uMaxBack > uSizeFile)
        uMaxBack = uSizeFile;

    unsigned char *buf =
        static_cast<unsigned char *>(ALLOC(BUFREADCOMMENT + 4));
    if (buf == nullptr)
        return 0;

    ZPOS64_T uBackRead = 4;
    while (uBackRead < uMaxBack)
    {
        if (uBackRead + BUFREADCOMMENT > uMaxBack)
            uBackRead = uMaxBack;
        else
            uBackRead += BUFREADCOMMENT;
        ZPOS64_T uReadPos = uSizeFile - uBackRead;

        uLong uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos))
                              ? (BUFREADCOMMENT + 4)
                              : static_cast<uLong>(uSizeFile - uReadPos);
        if (ZSEEK64(*pzlib_filefunc_def, filestream, uReadPos,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            break;

        if (ZREAD64(*pzlib_filefunc_def, filestream, buf, uReadSize) !=
            uReadSize)
            break;

        for (int i = static_cast<int>(uReadSize) - 3; (i--) > 0;)
            if (((*(buf + i)) == 0x50) && ((*(buf + i + 1)) == 0x4b) &&
                ((*(buf + i + 2)) == 0x05) && ((*(buf + i + 3)) == 0x06))
            {
                uPosFound = uReadPos + i;
                break;
            }

        if (uPosFound != 0)
            break;
    }
    TRYFREE(buf);
    return uPosFound;
}

/*
Locate the End of Zip64 Central directory locator and from there find the CD of
a zipfile (at the end, just before the global comment)
*/
static ZPOS64_T
zip64local_SearchCentralDir64(const zlib_filefunc_def *pzlib_filefunc_def,
                              voidpf filestream)
{
    unsigned char *buf;
    ZPOS64_T uSizeFile;
    ZPOS64_T uBackRead;
    ZPOS64_T uMaxBack = 0xffff; /* maximum size of global comment */
    ZPOS64_T uPosFound = 0;
    uLong uL;
    ZPOS64_T relativeOffset;

    if (ZSEEK64(*pzlib_filefunc_def, filestream, 0, ZLIB_FILEFUNC_SEEK_END) !=
        0)
        return 0;

    uSizeFile = ZTELL64(*pzlib_filefunc_def, filestream);

    if (uMaxBack > uSizeFile)
        uMaxBack = uSizeFile;

    buf = static_cast<unsigned char *>(ALLOC(BUFREADCOMMENT + 4));
    if (buf == nullptr)
        return 0;

    uBackRead = 4;
    while (uBackRead < uMaxBack)
    {
        uLong uReadSize;
        ZPOS64_T uReadPos;
        int i;
        if (uBackRead + BUFREADCOMMENT > uMaxBack)
            uBackRead = uMaxBack;
        else
            uBackRead += BUFREADCOMMENT;
        uReadPos = uSizeFile - uBackRead;

        uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos))
                        ? (BUFREADCOMMENT + 4)
                        : static_cast<uLong>(uSizeFile - uReadPos);
        if (ZSEEK64(*pzlib_filefunc_def, filestream, uReadPos,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            break;

        if (ZREAD64(*pzlib_filefunc_def, filestream, buf, uReadSize) !=
            uReadSize)
            break;

        for (i = static_cast<int>(uReadSize) - 3; (i--) > 0;)
        {
            // Signature "0x07064b50" Zip64 end of central directory locater
            if (((*(buf + i)) == 0x50) && ((*(buf + i + 1)) == 0x4b) &&
                ((*(buf + i + 2)) == 0x06) && ((*(buf + i + 3)) == 0x07))
            {
                uPosFound = uReadPos + i;
                break;
            }
        }

        if (uPosFound != 0)
            break;
    }

    TRYFREE(buf);
    if (uPosFound == 0)
        return 0;

    /* Zip64 end of central directory locator */
    if (ZSEEK64(*pzlib_filefunc_def, filestream, uPosFound,
                ZLIB_FILEFUNC_SEEK_SET) != 0)
        return 0;

    /* the signature, already checked */
    if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
        return 0;

    /* number of the disk with the start of the zip64 end of  central directory
     */
    if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
        return 0;
    if (uL != 0)
        return 0;

    /* relative offset of the zip64 end of central directory record */
    if (zip64local_getLong64(pzlib_filefunc_def, filestream, &relativeOffset) !=
        ZIP_OK)
        return 0;

    /* total number of disks */
    if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
        return 0;
    /* Some .zip declare 0 disks, such as in
     * http://trac.osgeo.org/gdal/ticket/5615 */
    if (uL != 0 && uL != 1)
        return 0;

    /* Goto Zip64 end of central directory record */
    if (ZSEEK64(*pzlib_filefunc_def, filestream, relativeOffset,
                ZLIB_FILEFUNC_SEEK_SET) != 0)
        return 0;

    /* the signature */
    if (zip64local_getLong(pzlib_filefunc_def, filestream, &uL) != ZIP_OK)
        return 0;

    if (uL != 0x06064b50)  // signature of 'Zip64 end of central directory'
        return 0;

    return relativeOffset;
}

static int LoadCentralDirectoryRecord(zip64_internal *pziinit)
{
    int err = ZIP_OK;
    ZPOS64_T byte_before_the_zipfile; /* byte before the zipfile, (>0 for sfx)*/

    ZPOS64_T size_central_dir;   /* size of the central directory  */
    ZPOS64_T offset_central_dir; /* offset of start of central directory */
    ZPOS64_T central_pos;
    uLong uL;

    uLong number_disk;         /* number of the current dist, used for
                               spanning ZIP, unsupported, always 0*/
    uLong number_disk_with_CD; /* number the disk with central dir, used
                               for spanning ZIP, unsupported, always 0*/
    ZPOS64_T number_entry;
    ZPOS64_T number_entry_CD; /* total number of entries in
                             the central dir
                             (same than number_entry on nospan) */
    uLong VersionMadeBy;
    uLong VersionNeeded;
    uLong size_comment;

    int hasZIP64Record = 0;

    // check first if we find a ZIP64 record
    central_pos = zip64local_SearchCentralDir64(&pziinit->z_filefunc,
                                                pziinit->filestream);
    if (central_pos > 0)
    {
        hasZIP64Record = 1;
    }
    else /* if (central_pos == 0) */
    {
        central_pos = zip64local_SearchCentralDir(&pziinit->z_filefunc,
                                                  pziinit->filestream);
    }

    /* disable to allow appending to empty ZIP archive
            if (central_pos==0)
                err=ZIP_ERRNO;
    */

    if (hasZIP64Record)
    {
        ZPOS64_T sizeEndOfCentralDirectory;
        if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, central_pos,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        /* the signature, already checked */
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &uL) != ZIP_OK)
            err = ZIP_ERRNO;

        /* size of zip64 end of central directory record */
        if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream,
                                 &sizeEndOfCentralDirectory) != ZIP_OK)
            err = ZIP_ERRNO;

        /* version made by */
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &VersionMadeBy) != ZIP_OK)
            err = ZIP_ERRNO;

        /* version needed to extract */
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &VersionNeeded) != ZIP_OK)
            err = ZIP_ERRNO;

        /* number of this disk */
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &number_disk) != ZIP_OK)
            err = ZIP_ERRNO;

        /* number of the disk with the start of the central directory */
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &number_disk_with_CD) != ZIP_OK)
            err = ZIP_ERRNO;

        /* total number of entries in the central directory on this disk */
        if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream,
                                 &number_entry) != ZIP_OK)
            err = ZIP_ERRNO;

        /* total number of entries in the central directory */
        if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream,
                                 &number_entry_CD) != ZIP_OK)
            err = ZIP_ERRNO;

        if ((number_entry_CD != number_entry) || (number_disk_with_CD != 0) ||
            (number_disk != 0))
            err = ZIP_BADZIPFILE;

        /* size of the central directory */
        if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream,
                                 &size_central_dir) != ZIP_OK)
            err = ZIP_ERRNO;

        /* offset of start of central directory with respect to the
        starting disk number */
        if (zip64local_getLong64(&pziinit->z_filefunc, pziinit->filestream,
                                 &offset_central_dir) != ZIP_OK)
            err = ZIP_ERRNO;

        // TODO..
        // read the comment from the standard central header.
        size_comment = 0;
    }
    else
    {
        // Read End of central Directory info
        if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream, central_pos,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        /* the signature, already checked */
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &uL) != ZIP_OK)
            err = ZIP_ERRNO;

        /* number of this disk */
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &number_disk) != ZIP_OK)
            err = ZIP_ERRNO;

        /* number of the disk with the start of the central directory */
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &number_disk_with_CD) != ZIP_OK)
            err = ZIP_ERRNO;

        /* total number of entries in the central dir on this disk */
        number_entry = 0;
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &uL) != ZIP_OK)
            err = ZIP_ERRNO;
        else
            number_entry = uL;

        /* total number of entries in the central dir */
        number_entry_CD = 0;
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &uL) != ZIP_OK)
            err = ZIP_ERRNO;
        else
            number_entry_CD = uL;

        if ((number_entry_CD != number_entry) || (number_disk_with_CD != 0) ||
            (number_disk != 0))
            err = ZIP_BADZIPFILE;

        /* size of the central directory */
        size_central_dir = 0;
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &uL) != ZIP_OK)
            err = ZIP_ERRNO;
        else
            size_central_dir = uL;

        /* offset of start of central directory with respect to the starting
         * disk number */
        offset_central_dir = 0;
        if (zip64local_getLong(&pziinit->z_filefunc, pziinit->filestream,
                               &uL) != ZIP_OK)
            err = ZIP_ERRNO;
        else
            offset_central_dir = uL;

        /* zipfile global comment length */
        if (zip64local_getShort(&pziinit->z_filefunc, pziinit->filestream,
                                &size_comment) != ZIP_OK)
            err = ZIP_ERRNO;
    }

    if ((central_pos < offset_central_dir + size_central_dir) &&
        (err == ZIP_OK))
        err = ZIP_BADZIPFILE;

    if (err != ZIP_OK)
    {
        ZCLOSE64(pziinit->z_filefunc, pziinit->filestream);
        return ZIP_ERRNO;
    }

    if (size_comment > 0)
    {
        pziinit->globalcomment = static_cast<char *>(ALLOC(size_comment + 1));
        if (pziinit->globalcomment)
        {
            size_comment = ZREAD64(pziinit->z_filefunc, pziinit->filestream,
                                   pziinit->globalcomment, size_comment);
            pziinit->globalcomment[size_comment] = 0;
        }
    }

    byte_before_the_zipfile =
        central_pos - (offset_central_dir + size_central_dir);
    pziinit->add_position_when_writing_offset = byte_before_the_zipfile;

    {
        ZPOS64_T size_central_dir_to_read = size_central_dir;
        size_t buf_size = SIZEDATA_INDATABLOCK;
        void *buf_read = ALLOC(buf_size);
        if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream,
                    offset_central_dir + byte_before_the_zipfile,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        while ((size_central_dir_to_read > 0) && (err == ZIP_OK))
        {
            ZPOS64_T read_this = SIZEDATA_INDATABLOCK;
            if (read_this > size_central_dir_to_read)
                read_this = size_central_dir_to_read;

            if (ZREAD64(pziinit->z_filefunc, pziinit->filestream, buf_read,
                        static_cast<uLong>(read_this)) != read_this)
                err = ZIP_ERRNO;

            if (err == ZIP_OK)
                err = add_data_in_datablock(&pziinit->central_dir, buf_read,
                                            static_cast<uLong>(read_this));

            size_central_dir_to_read -= read_this;
        }
        TRYFREE(buf_read);
    }
    pziinit->begin_pos = byte_before_the_zipfile;
    pziinit->number_entry = number_entry_CD;

    if (ZSEEK64(pziinit->z_filefunc, pziinit->filestream,
                offset_central_dir + byte_before_the_zipfile,
                ZLIB_FILEFUNC_SEEK_SET) != 0)
        err = ZIP_ERRNO;

    return err;
}

#endif /* !NO_ADDFILEINEXISTINGZIP*/

/************************************************************/
extern zipFile ZEXPORT cpl_zipOpen2(const char *pathname, int append,
                                    zipcharpc *globalcomment,
                                    zlib_filefunc_def *pzlib_filefunc_def)
{
    zip64_internal ziinit;
    memset(&ziinit, 0, sizeof(ziinit));

    if (pzlib_filefunc_def == nullptr)
        cpl_fill_fopen_filefunc(&ziinit.z_filefunc);
    else
        ziinit.z_filefunc = *pzlib_filefunc_def;

    ziinit.filestream = (*(ziinit.z_filefunc.zopen_file))(
        ziinit.z_filefunc.opaque, pathname,
        (append == APPEND_STATUS_CREATE)
            ? (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE |
               ZLIB_FILEFUNC_MODE_CREATE)
            : (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE |
               ZLIB_FILEFUNC_MODE_EXISTING));

    if (ziinit.filestream == nullptr)
        return nullptr;

    if (append == APPEND_STATUS_CREATEAFTER)
        ZSEEK64(ziinit.z_filefunc, ziinit.filestream, 0, SEEK_END);

    ziinit.begin_pos = ZTELL64(ziinit.z_filefunc, ziinit.filestream);
    ziinit.in_opened_file_inzip = 0;
    ziinit.ci.stream_initialised = 0;
    ziinit.number_entry = 0;
    ziinit.add_position_when_writing_offset = 0;
    ziinit.use_cpl_io = (pzlib_filefunc_def == nullptr) ? 1 : 0;
    ziinit.vsi_raw_length_before = 0;
    ziinit.vsi_deflate_handle = nullptr;
    ziinit.nChunkSize = 0;
    ziinit.nThreads = 0;
    ziinit.nOffsetSize = 0;
    ziinit.sozip_index = nullptr;
    init_linkedlist(&(ziinit.central_dir));

    zip64_internal *zi =
        static_cast<zip64_internal *>(ALLOC(sizeof(zip64_internal)));
    if (zi == nullptr)
    {
        ZCLOSE64(ziinit.z_filefunc, ziinit.filestream);
        return nullptr;
    }

    /* now we add file in a zipfile */
#ifndef NO_ADDFILEINEXISTINGZIP
    ziinit.globalcomment = nullptr;

    int err = ZIP_OK;
    if (append == APPEND_STATUS_ADDINZIP)
    {
        // Read and Cache Central Directory Records
        err = LoadCentralDirectoryRecord(&ziinit);
    }

    if (globalcomment)
    {
        *globalcomment = ziinit.globalcomment;
    }
#endif /* !NO_ADDFILEINEXISTINGZIP*/

    if (err != ZIP_OK)
    {
#ifndef NO_ADDFILEINEXISTINGZIP
        TRYFREE(ziinit.globalcomment);
#endif /* !NO_ADDFILEINEXISTINGZIP*/
        TRYFREE(zi);
        return nullptr;
    }
    else
    {
        *zi = ziinit;
        return static_cast<zipFile>(zi);
    }
}

extern zipFile ZEXPORT cpl_zipOpen(const char *pathname, int append)
{
    return cpl_zipOpen2(pathname, append, nullptr, nullptr);
}

static void zip64local_putValue_inmemory_update(char **dest, ZPOS64_T x,
                                                int nbByte)
{
    zip64local_putValue_inmemory(*dest, x, nbByte);
    *dest += nbByte;
}

static int Write_LocalFileHeader(zip64_internal *zi, const char *filename,
                                 uInt size_extrafield_local,
                                 const void *extrafield_local, int zip64)
{
    /* write the local header */
    int err = ZIP_OK;
    uInt size_filename = static_cast<uInt>(strlen(filename));
    uInt size_extrafield = size_extrafield_local;

    if (zip64)
    {
        size_extrafield += 20;
    }

    uInt size_local_header = 30 + size_filename + size_extrafield;
    char *local_header = static_cast<char *>(ALLOC(size_local_header));
    char *p = local_header;

    zip64local_putValue_inmemory_update(&p, LOCALHEADERMAGIC, 4);
    if (zip64)
        zip64local_putValue_inmemory_update(&p, 45,
                                            2); /* version needed to extract */
    else
        zip64local_putValue_inmemory_update(&p, 20,
                                            2); /* version needed to extract */

    zip64local_putValue_inmemory_update(&p, zi->ci.flag, 2);

    zip64local_putValue_inmemory_update(&p, zi->ci.method, 2);

    zip64local_putValue_inmemory_update(&p, zi->ci.dosDate, 4);

    // CRC / Compressed size / Uncompressed size will be filled in later and
    // rewritten later
    zip64local_putValue_inmemory_update(&p, 0, 4); /* crc 32, unknown */

    if (zip64)
        zip64local_putValue_inmemory_update(&p, 0xFFFFFFFFU,
                                            4); /* compressed size, unknown */
    else
        zip64local_putValue_inmemory_update(&p, 0,
                                            4); /* compressed size, unknown */

    if (zip64)
        zip64local_putValue_inmemory_update(&p, 0xFFFFFFFFU,
                                            4); /* uncompressed size, unknown */
    else
        zip64local_putValue_inmemory_update(&p, 0,
                                            4); /* uncompressed size, unknown */

    zip64local_putValue_inmemory_update(&p, size_filename, 2);

    zi->ci.size_local_header_extrafield = size_extrafield;

    zip64local_putValue_inmemory_update(&p, size_extrafield, 2);

    if (size_filename > 0)
    {
        memcpy(p, filename, size_filename);
        p += size_filename;
    }

    if (size_extrafield_local > 0)
    {
        memcpy(p, extrafield_local, size_extrafield_local);
        p += size_extrafield_local;
    }

    if (zip64)
    {
        // write the Zip64 extended info
        short HeaderID = 1;
        short DataSize = 16;
        ZPOS64_T CompressedSize = 0;
        ZPOS64_T UncompressedSize = 0;

        // Remember position of Zip64 extended info for the local file header.
        // (needed when we update size after done with file)
        zi->ci.pos_zip64extrainfo =
            ZTELL64(zi->z_filefunc, zi->filestream) + p - local_header;

        zip64local_putValue_inmemory_update(&p, HeaderID, 2);
        zip64local_putValue_inmemory_update(&p, DataSize, 2);

        zip64local_putValue_inmemory_update(&p, UncompressedSize, 8);
        zip64local_putValue_inmemory_update(&p, CompressedSize, 8);
    }
    assert(p == local_header + size_local_header);

    if (ZWRITE64(zi->z_filefunc, zi->filestream, local_header,
                 size_local_header) != size_local_header)
        err = ZIP_ERRNO;

    zi->ci.local_header = local_header;
    zi->ci.size_local_header = size_local_header;

    return err;
}

extern int ZEXPORT cpl_zipOpenNewFileInZip3(
    zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uInt size_extrafield_local,
    const void *extrafield_global, uInt size_extrafield_global,
    const char *comment, int method, int level, int raw, int windowBits,
    int memLevel, int strategy, const char *password,
#ifdef NOCRYPT
    uLong /* crcForCrypting */
#else
    uLong crcForCrypting
#endif
    ,
    bool bZip64, bool bIncludeInCentralDirectory)
{
    zip64_internal *zi;
    uInt size_filename;
    uInt size_comment;
    uInt i;
    int err = ZIP_OK;
    uLong flagBase = 0;

#ifdef NOCRYPT
    if (password != nullptr)
        return ZIP_PARAMERROR;
#endif

    if (file == nullptr)
        return ZIP_PARAMERROR;
    if ((method != 0) && (method != Z_DEFLATED))
        return ZIP_PARAMERROR;

    zi = reinterpret_cast<zip64_internal *>(file);

    if (zi->in_opened_file_inzip == 1)
    {
        err = cpl_zipCloseFileInZip(file);
        if (err != ZIP_OK)
            return err;
    }

    if (filename == nullptr)
        filename = "-";

    // The filename and comment length must fit in 16 bits.
    if ((filename != nullptr) && (strlen(filename) > 0xffff))
        return ZIP_PARAMERROR;
    if ((comment != nullptr) && (strlen(comment) > 0xffff))
        return ZIP_PARAMERROR;
    // The extra field length must fit in 16 bits. If the member also requires
    // a Zip64 extra block, that will also need to fit within that 16-bit
    // length, but that will be checked for later.
    if ((size_extrafield_local > 0xffff) || (size_extrafield_global > 0xffff))
        return ZIP_PARAMERROR;

    if (comment == nullptr)
        size_comment = 0;
    else
        size_comment = static_cast<uInt>(strlen(comment));

    size_filename = static_cast<uInt>(strlen(filename));

    if (zipfi == nullptr)
        zi->ci.dosDate = 0;
    else
    {
        if (zipfi->dosDate != 0)
            zi->ci.dosDate = zipfi->dosDate;
        else
            zi->ci.dosDate = zip64local_TmzDateToDosDate(&zipfi->tmz_date);
    }

    zi->ci.flag = flagBase;
    if ((level == 8) || (level == 9))
        zi->ci.flag |= 2;
    if (level == 2)
        zi->ci.flag |= 4;
    if (level == 1)
        zi->ci.flag |= 6;
#ifndef NOCRYPT
    if (password != nullptr)
        zi->ci.flag |= 1;
#endif

    zi->ci.crc32 = 0;
    zi->ci.method = method;
    zi->ci.encrypt = 0;
    zi->ci.stream_initialised = 0;
    zi->ci.pos_in_buffered_data = 0;
    zi->ci.raw = raw;
    zi->ci.pos_local_header = ZTELL64(zi->z_filefunc, zi->filestream);

    if (bIncludeInCentralDirectory)
    {
        zi->ci.size_centralheader = SIZECENTRALHEADER + size_filename +
                                    size_extrafield_global + size_comment;
        zi->ci.size_centralExtraFree =
            32;  // Extra space we have reserved in case we need to add ZIP64
                 // extra info data

        zi->ci.central_header = static_cast<char *>(ALLOC(static_cast<uInt>(
            zi->ci.size_centralheader + zi->ci.size_centralExtraFree)));

        zi->ci.size_centralExtra = size_extrafield_global;
        zip64local_putValue_inmemory(zi->ci.central_header, CENTRALHEADERMAGIC,
                                     4);
        /* version info */
        zip64local_putValue_inmemory(zi->ci.central_header + 4, VERSIONMADEBY,
                                     2);
        zip64local_putValue_inmemory(zi->ci.central_header + 6, 20, 2);
        zip64local_putValue_inmemory(zi->ci.central_header + 8,
                                     static_cast<uLong>(zi->ci.flag), 2);
        zip64local_putValue_inmemory(zi->ci.central_header + 10,
                                     static_cast<uLong>(zi->ci.method), 2);
        zip64local_putValue_inmemory(zi->ci.central_header + 12,
                                     static_cast<uLong>(zi->ci.dosDate), 4);
        zip64local_putValue_inmemory(zi->ci.central_header + 16, 0, 4); /*crc*/
        zip64local_putValue_inmemory(zi->ci.central_header + 20, 0,
                                     4); /*compr size*/
        zip64local_putValue_inmemory(zi->ci.central_header + 24, 0,
                                     4); /*uncompr size*/
        zip64local_putValue_inmemory(zi->ci.central_header + 28,
                                     static_cast<uLong>(size_filename), 2);
        zip64local_putValue_inmemory(zi->ci.central_header + 30,
                                     static_cast<uLong>(size_extrafield_global),
                                     2);
        zip64local_putValue_inmemory(zi->ci.central_header + 32,
                                     static_cast<uLong>(size_comment), 2);
        zip64local_putValue_inmemory(zi->ci.central_header + 34, 0,
                                     2); /*disk nm start*/

        if (zipfi == nullptr)
            zip64local_putValue_inmemory(zi->ci.central_header + 36, 0, 2);
        else
            zip64local_putValue_inmemory(zi->ci.central_header + 36,
                                         static_cast<uLong>(zipfi->internal_fa),
                                         2);

        if (zipfi == nullptr)
            zip64local_putValue_inmemory(zi->ci.central_header + 38, 0, 4);
        else
            zip64local_putValue_inmemory(zi->ci.central_header + 38,
                                         static_cast<uLong>(zipfi->external_fa),
                                         4);

        if (zi->ci.pos_local_header >= 0xffffffff)
            zip64local_putValue_inmemory(zi->ci.central_header + 42,
                                         static_cast<uLong>(0xffffffff), 4);
        else
            zip64local_putValue_inmemory(
                zi->ci.central_header + 42,
                static_cast<uLong>(zi->ci.pos_local_header) -
                    zi->add_position_when_writing_offset,
                4);

        for (i = 0; i < size_filename; i++)
            *(zi->ci.central_header + SIZECENTRALHEADER + i) = *(filename + i);

        for (i = 0; i < size_extrafield_global; i++)
            *(zi->ci.central_header + SIZECENTRALHEADER + size_filename + i) =
                *((reinterpret_cast<const char *>(extrafield_global)) + i);

        for (i = 0; i < size_comment; i++)
            *(zi->ci.central_header + SIZECENTRALHEADER + size_filename +
              size_extrafield_global + i) = *(comment + i);
        if (zi->ci.central_header == nullptr)
            return ZIP_INTERNALERROR;
    }
    else
    {
        zi->ci.central_header = nullptr;
    }

    zi->ci.totalCompressedData = 0;
    zi->ci.totalUncompressedData = 0;
    zi->ci.pos_zip64extrainfo = 0;

    // For now default is to generate zip64 extra fields
    err = Write_LocalFileHeader(zi, filename, size_extrafield_local,
                                extrafield_local, bZip64 ? 1 : 0);

    zi->ci.stream.avail_in = 0;
    zi->ci.stream.avail_out = Z_BUFSIZE;
    zi->ci.stream.next_out = zi->ci.buffered_data;
    zi->ci.stream.total_in = 0;
    zi->ci.stream.total_out = 0;
    zi->ci.stream.data_type = Z_UNKNOWN;

    if ((err == ZIP_OK) && (zi->ci.method == Z_DEFLATED) && (!zi->ci.raw))
    {
        zi->ci.stream.zalloc = nullptr;
        zi->ci.stream.zfree = nullptr;
        zi->ci.stream.opaque = nullptr;

        if (windowBits > 0)
            windowBits = -windowBits;

        if (zi->use_cpl_io)
        {
            auto fpRaw = reinterpret_cast<VSIVirtualHandle *>(zi->filestream);
            zi->vsi_raw_length_before = fpRaw->Tell();
            zi->vsi_deflate_handle = VSICreateGZipWritable(
                fpRaw, CPL_DEFLATE_TYPE_RAW_DEFLATE, false, zi->nThreads,
                zi->nChunkSize, zi->nOffsetSize, zi->sozip_index);
            err = Z_OK;
        }
        else
        {
            err = deflateInit2(&zi->ci.stream, level, Z_DEFLATED, windowBits,
                               memLevel, strategy);
        }

        if (err == Z_OK)
            zi->ci.stream_initialised = 1;
    }
#ifndef NOCRYPT
    zi->ci.crypt_header_size = 0;
    if ((err == Z_OK) && (password != nullptr))
    {
        unsigned char bufHead[RAND_HEAD_LEN];
        unsigned int sizeHead = 0;
        zi->ci.encrypt = 1;
        zi->ci.pcrc_32_tab = get_crc_table();
        /*init_keys(password,zi->ci.keys,zi->ci.pcrc_32_tab);*/

        sizeHead = crypthead(password, bufHead, RAND_HEAD_LEN, zi->ci.keys,
                             zi->ci.pcrc_32_tab, crcForCrypting);
        zi->ci.crypt_header_size = sizeHead;

        if (ZWRITE64(zi->z_filefunc, zi->filestream, bufHead, sizeHead) !=
            sizeHead)
            err = ZIP_ERRNO;
    }
#endif

    if (err == Z_OK)
        zi->in_opened_file_inzip = 1;
    else
    {
        free(zi->ci.central_header);
        zi->ci.central_header = nullptr;
        free(zi->ci.local_header);
        zi->ci.local_header = nullptr;
    }

    return err;
}

extern int ZEXPORT cpl_zipOpenNewFileInZip2(
    zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uInt size_extrafield_local,
    const void *extrafield_global, uInt size_extrafield_global,
    const char *comment, int method, int level, int raw)
{
    return cpl_zipOpenNewFileInZip3(
        file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, method, level, raw,
        -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, nullptr, 0, true, true);
}

extern int ZEXPORT cpl_zipOpenNewFileInZip(
    zipFile file, const char *filename, const zip_fileinfo *zipfi,
    const void *extrafield_local, uInt size_extrafield_local,
    const void *extrafield_global, uInt size_extrafield_global,
    const char *comment, int method, int level)
{
    return cpl_zipOpenNewFileInZip2(
        file, filename, zipfi, extrafield_local, size_extrafield_local,
        extrafield_global, size_extrafield_global, comment, method, level, 0);
}

static int zip64FlushWriteBuffer(zip64_internal *zi)
{
    int err = ZIP_OK;

    if (zi->ci.encrypt != 0)
    {
#ifndef NOCRYPT
        int t = 0;
        for (uInt i = 0; i < zi->ci.pos_in_buffered_data; i++)
            zi->ci.buffered_data[i] = zencode(zi->ci.keys, zi->ci.pcrc_32_tab,
                                              zi->ci.buffered_data[i], t);
#endif
    }
    if (ZWRITE64(zi->z_filefunc, zi->filestream, zi->ci.buffered_data,
                 zi->ci.pos_in_buffered_data) != zi->ci.pos_in_buffered_data)
        err = ZIP_ERRNO;

    zi->ci.totalCompressedData += zi->ci.pos_in_buffered_data;
    zi->ci.totalUncompressedData += zi->ci.stream.total_in;
    zi->ci.stream.total_in = 0;

    zi->ci.pos_in_buffered_data = 0;
    return err;
}

extern int ZEXPORT cpl_zipWriteInFileInZip(zipFile file, const void *buf,
                                           unsigned len)
{
    if (file == nullptr)
        return ZIP_PARAMERROR;

    zip64_internal *zi = reinterpret_cast<zip64_internal *>(file);

    if (zi->in_opened_file_inzip == 0)
        return ZIP_PARAMERROR;

    zi->ci.stream.next_in = reinterpret_cast<Bytef *>(const_cast<void *>(buf));
    zi->ci.stream.avail_in = len;
    zi->ci.crc32 =
        crc32(zi->ci.crc32, reinterpret_cast<const Bytef *>(buf), len);

    int err = ZIP_OK;
    while ((err == ZIP_OK) && (zi->ci.stream.avail_in > 0))
    {
        if (zi->ci.stream.avail_out == 0)
        {
            if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
                err = ZIP_ERRNO;
            zi->ci.stream.avail_out = Z_BUFSIZE;
            zi->ci.stream.next_out = zi->ci.buffered_data;
        }

        if (err != ZIP_OK)
            break;

        if ((zi->ci.method == Z_DEFLATED) && (!zi->ci.raw))
        {
            if (zi->vsi_deflate_handle)
            {
                zi->ci.totalUncompressedData += len;
                if (zi->vsi_deflate_handle->Write(buf, 1, len) < len)
                    err = ZIP_INTERNALERROR;
                zi->ci.stream.avail_in = 0;
            }
            else
            {
                uLong uTotalOutBefore = zi->ci.stream.total_out;
                err = deflate(&zi->ci.stream, Z_NO_FLUSH);
                zi->ci.pos_in_buffered_data += static_cast<uInt>(
                    zi->ci.stream.total_out - uTotalOutBefore);
            }
        }
        else
        {
            uInt copy_this;
            if (zi->ci.stream.avail_in < zi->ci.stream.avail_out)
                copy_this = zi->ci.stream.avail_in;
            else
                copy_this = zi->ci.stream.avail_out;
            for (uInt i = 0; i < copy_this; i++)
                *((reinterpret_cast<char *>(zi->ci.stream.next_out)) + i) =
                    *((reinterpret_cast<const char *>(zi->ci.stream.next_in)) +
                      i);
            {
                zi->ci.stream.avail_in -= copy_this;
                zi->ci.stream.avail_out -= copy_this;
                zi->ci.stream.next_in += copy_this;
                zi->ci.stream.next_out += copy_this;
                zi->ci.stream.total_in += copy_this;
                zi->ci.stream.total_out += copy_this;
                zi->ci.pos_in_buffered_data += copy_this;
            }
        }
    }

    return err;
}

extern int ZEXPORT cpl_zipCloseFileInZipRaw(zipFile file,
                                            ZPOS64_T uncompressed_size,
                                            uLong crc32)
{
    if (file == nullptr)
        return ZIP_PARAMERROR;

    zip64_internal *zi = reinterpret_cast<zip64_internal *>(file);

    if (zi->in_opened_file_inzip == 0)
        return ZIP_PARAMERROR;
    zi->ci.stream.avail_in = 0;

    int err = ZIP_OK;
    if ((zi->ci.method == Z_DEFLATED) && (!zi->ci.raw))
    {
        if (zi->vsi_deflate_handle)
        {
            auto fpRaw = reinterpret_cast<VSIVirtualHandle *>(zi->filestream);
            delete zi->vsi_deflate_handle;
            zi->vsi_deflate_handle = nullptr;
            zi->ci.totalCompressedData =
                fpRaw->Tell() - zi->vsi_raw_length_before;

            if (zi->sozip_index)
            {
                uint64_t nVal =
                    static_cast<uint64_t>(zi->ci.totalCompressedData);
                CPL_LSBPTR64(&nVal);
                memcpy(zi->sozip_index->data() + 24, &nVal, sizeof(uint64_t));
            }
        }
        else
        {
            while (err == ZIP_OK)
            {
                if (zi->ci.stream.avail_out == 0)
                {
                    if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
                    {
                        err = ZIP_ERRNO;
                        break;
                    }
                    zi->ci.stream.avail_out = Z_BUFSIZE;
                    zi->ci.stream.next_out = zi->ci.buffered_data;
                }
                uLong uTotalOutBefore = zi->ci.stream.total_out;
                err = deflate(&zi->ci.stream, Z_FINISH);
                zi->ci.pos_in_buffered_data += static_cast<uInt>(
                    zi->ci.stream.total_out - uTotalOutBefore);
            }
        }
    }

    if (err == Z_STREAM_END)
        err = ZIP_OK; /* this is normal */

    if ((zi->ci.pos_in_buffered_data > 0) && (err == ZIP_OK))
        if (zip64FlushWriteBuffer(zi) == ZIP_ERRNO)
            err = ZIP_ERRNO;

    if (!zi->use_cpl_io && (zi->ci.method == Z_DEFLATED) && (!zi->ci.raw))
    {
        err = deflateEnd(&zi->ci.stream);
        zi->ci.stream_initialised = 0;
    }

    if (!zi->ci.raw)
    {
        crc32 = static_cast<uLong>(zi->ci.crc32);
        uncompressed_size = zi->ci.totalUncompressedData;
    }
    ZPOS64_T compressed_size = zi->ci.totalCompressedData;
#ifndef NOCRYPT
    compressed_size += zi->ci.crypt_header_size;
#endif

#ifdef disabled
    // Code finally disabled since it causes compatibility issues with
    // libreoffice for .xlsx or .ods files
    if (zi->ci.pos_zip64extrainfo && uncompressed_size < 0xffffffff &&
        compressed_size < 0xffffffff)
    {
        // Update the LocalFileHeader to be a regular one and not a ZIP64 one
        // by removing its trailing 20 bytes, and moving it in the file
        // 20 bytes further of its original position.

        ZPOS64_T cur_pos_inzip = ZTELL64(zi->z_filefunc, zi->filestream);

        if (ZSEEK64(zi->z_filefunc, zi->filestream, zi->ci.pos_local_header,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        // Insert leading padding
        constexpr uInt nZIP64ExtraBytes = 20;
        char padding[nZIP64ExtraBytes];
        memset(padding, 0, sizeof(padding));
        if (ZWRITE64(zi->z_filefunc, zi->filestream, padding,
                     nZIP64ExtraBytes) != nZIP64ExtraBytes)
            err = ZIP_ERRNO;

        // Correct version needed to extract
        zip64local_putValue_inmemory(zi->ci.local_header + 4, 20, 2);

        // Correct extra field length
        zi->ci.size_local_header_extrafield -= nZIP64ExtraBytes;
        zip64local_putValue_inmemory(zi->ci.local_header + 28,
                                     zi->ci.size_local_header_extrafield, 2);

        zi->ci.size_local_header -= nZIP64ExtraBytes;

        // Rewrite local header
        if (ZWRITE64(zi->z_filefunc, zi->filestream, zi->ci.local_header,
                     zi->ci.size_local_header) != zi->ci.size_local_header)
            err = ZIP_ERRNO;

        if (ZSEEK64(zi->z_filefunc, zi->filestream, cur_pos_inzip,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        zi->ci.pos_zip64extrainfo = 0;

        // Correct central header offset to local header
        zi->ci.pos_local_header += nZIP64ExtraBytes;
        if (zi->ci.central_header)
        {
            if (zi->ci.pos_local_header >= 0xffffffff)
                zip64local_putValue_inmemory(zi->ci.central_header + 42,
                                             static_cast<uLong>(0xffffffff), 4);
            else
                zip64local_putValue_inmemory(
                    zi->ci.central_header + 42,
                    static_cast<uLong>(zi->ci.pos_local_header) -
                        zi->add_position_when_writing_offset,
                    4);
        }
    }
#endif

    const bool bInCentralHeader = zi->ci.central_header != nullptr;
    if (zi->ci.central_header)
    {
        // update Current Item crc and sizes,
        if (zi->ci.pos_zip64extrainfo || compressed_size >= 0xffffffff ||
            uncompressed_size >= 0xffffffff ||
            zi->ci.pos_local_header >= 0xffffffff)
        {
            /*version Made by*/
            zip64local_putValue_inmemory(zi->ci.central_header + 4, 45, 2);
            /*version needed*/
            zip64local_putValue_inmemory(zi->ci.central_header + 6, 45, 2);
        }

        zip64local_putValue_inmemory(zi->ci.central_header + 16, crc32,
                                     4); /*crc*/

        const uLong invalidValue = 0xffffffff;
        if (compressed_size >= 0xffffffff)
            zip64local_putValue_inmemory(zi->ci.central_header + 20,
                                         invalidValue, 4); /*compr size*/
        else
            zip64local_putValue_inmemory(zi->ci.central_header + 20,
                                         compressed_size, 4); /*compr size*/

        /// set internal file attributes field
        if (zi->ci.stream.data_type == Z_ASCII)
            zip64local_putValue_inmemory(zi->ci.central_header + 36, Z_ASCII,
                                         2);

        if (uncompressed_size >= 0xffffffff)
            zip64local_putValue_inmemory(zi->ci.central_header + 24,
                                         invalidValue, 4); /*uncompr size*/
        else
            zip64local_putValue_inmemory(zi->ci.central_header + 24,
                                         uncompressed_size, 4); /*uncompr size*/

        short datasize = 0;
        // Add ZIP64 extra info field for uncompressed size
        if (uncompressed_size >= 0xffffffff)
            datasize += 8;

        // Add ZIP64 extra info field for compressed size
        if (compressed_size >= 0xffffffff)
            datasize += 8;

        // Add ZIP64 extra info field for relative offset to local file header
        // of current file
        if (zi->ci.pos_local_header >= 0xffffffff)
            datasize += 8;

        if (datasize > 0)
        {
            char *p = nullptr;

            if (static_cast<uLong>(datasize + 4) > zi->ci.size_centralExtraFree)
            {
                // we can not write more data to the buffer that we have room
                // for.
                return ZIP_BADZIPFILE;
            }

            p = zi->ci.central_header + zi->ci.size_centralheader;

            // Add Extra Information Header for 'ZIP64 information'
            zip64local_putValue_inmemory(p, 0x0001, 2);  // HeaderID
            p += 2;
            zip64local_putValue_inmemory(p, datasize, 2);  // DataSize
            p += 2;

            if (uncompressed_size >= 0xffffffff)
            {
                zip64local_putValue_inmemory(p, uncompressed_size, 8);
                p += 8;
            }

            if (compressed_size >= 0xffffffff)
            {
                zip64local_putValue_inmemory(p, compressed_size, 8);
                p += 8;
            }

            if (zi->ci.pos_local_header >= 0xffffffff)
            {
                zip64local_putValue_inmemory(p, zi->ci.pos_local_header, 8);
                // p += 8;
            }

            // Update how much extra free space we got in the memory buffer
            // and increase the centralheader size so the new ZIP64 fields are
            // included ( 4 below is the size of HeaderID and DataSize field )
            zi->ci.size_centralExtraFree -= datasize + 4;
            zi->ci.size_centralheader += datasize + 4;

            // Update the extra info size field
            zi->ci.size_centralExtra += datasize + 4;
            zip64local_putValue_inmemory(
                zi->ci.central_header + 30,
                static_cast<uLong>(zi->ci.size_centralExtra), 2);
        }

        if (err == ZIP_OK)
            err = add_data_in_datablock(
                &zi->central_dir, zi->ci.central_header,
                static_cast<uLong>(zi->ci.size_centralheader));
        free(zi->ci.central_header);
        zi->ci.central_header = nullptr;
    }

    free(zi->ci.local_header);
    zi->ci.local_header = nullptr;

    if (err == ZIP_OK)
    {
        // Update the LocalFileHeader with the new values.

        ZPOS64_T cur_pos_inzip = ZTELL64(zi->z_filefunc, zi->filestream);

        if (ZSEEK64(zi->z_filefunc, zi->filestream,
                    zi->ci.pos_local_header + 14, ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;

        if (err == ZIP_OK)
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream, crc32,
                                      4); /* crc 32, unknown */

        if (uncompressed_size >= 0xffffffff || compressed_size >= 0xffffffff)
        {
            if (zi->ci.pos_zip64extrainfo > 0)
            {
                // Update the size in the ZIP64 extended field.
                if (ZSEEK64(zi->z_filefunc, zi->filestream,
                            zi->ci.pos_zip64extrainfo + 4,
                            ZLIB_FILEFUNC_SEEK_SET) != 0)
                    err = ZIP_ERRNO;

                if (err == ZIP_OK) /* compressed size, unknown */
                    err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                              uncompressed_size, 8);

                if (err == ZIP_OK) /* uncompressed size, unknown */
                    err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                              compressed_size, 8);
            }
            else
                err = ZIP_BADZIPFILE;  // Caller passed zip64 = 0, so no room
                                       // for zip64 info -> fatal
        }
        else
        {
            if (err == ZIP_OK) /* compressed size, unknown */
                err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                          compressed_size, 4);

            if (err == ZIP_OK) /* uncompressed size, unknown */
                err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                          uncompressed_size, 4);
        }
        if (ZSEEK64(zi->z_filefunc, zi->filestream, cur_pos_inzip,
                    ZLIB_FILEFUNC_SEEK_SET) != 0)
            err = ZIP_ERRNO;
    }

    if (bInCentralHeader)
        zi->number_entry++;
    zi->in_opened_file_inzip = 0;

    return err;
}

extern int ZEXPORT cpl_zipCloseFileInZip(zipFile file)
{
    return cpl_zipCloseFileInZipRaw(file, 0, 0);
}

static int Write_Zip64EndOfCentralDirectoryLocator(zip64_internal *zi,
                                                   ZPOS64_T zip64eocd_pos_inzip)
{
    int err = ZIP_OK;
    ZPOS64_T pos = zip64eocd_pos_inzip - zi->add_position_when_writing_offset;

    err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                              ZIP64ENDLOCHEADERMAGIC, 4);

    /*num disks*/
    if (err ==
        ZIP_OK) /* number of the disk with the start of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0, 4);

    /*relative offset*/
    if (err == ZIP_OK) /* Relative offset to the Zip64EndOfCentralDirectory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, pos, 8);

    /*total disks*/ /* Do not support spawning of disk so always say 1 here*/
    if (err ==
        ZIP_OK) /* number of the disk with the start of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 1, 4);

    return err;
}

static int Write_Zip64EndOfCentralDirectoryRecord(zip64_internal *zi,
                                                  uLong size_centraldir,
                                                  ZPOS64_T centraldir_pos_inzip)
{
    int err = ZIP_OK;

    uLong Zip64DataSize = 44;

    err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                              ZIP64ENDHEADERMAGIC, 4);

    if (err == ZIP_OK) /* size of this 'zip64 end of central directory' */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                  Zip64DataSize, 8);  // why ZPOS64_T of this ?

    if (err == ZIP_OK) /* version made by */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 45, 2);

    if (err == ZIP_OK) /* version needed */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 45, 2);

    if (err == ZIP_OK) /* number of this disk */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0, 4);

    if (err ==
        ZIP_OK) /* number of the disk with the start of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0, 4);

    if (err ==
        ZIP_OK) /* total number of entries in the central dir on this disk */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                  zi->number_entry, 8);

    if (err == ZIP_OK) /* total number of entries in the central dir */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                  zi->number_entry, 8);

    if (err == ZIP_OK) /* size of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                  size_centraldir, 8);

    if (err == ZIP_OK) /* offset of start of central directory with respect to
                          the starting disk number */
    {
        ZPOS64_T pos =
            centraldir_pos_inzip - zi->add_position_when_writing_offset;
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, pos, 8);
    }
    return err;
}

static int Write_EndOfCentralDirectoryRecord(zip64_internal *zi,
                                             uLong size_centraldir,
                                             ZPOS64_T centraldir_pos_inzip)
{
    int err = ZIP_OK;

    /*signature*/
    err =
        zip64local_putValue(&zi->z_filefunc, zi->filestream, ENDHEADERMAGIC, 4);

    if (err == ZIP_OK) /* number of this disk */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0, 2);

    if (err ==
        ZIP_OK) /* number of the disk with the start of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0, 2);

    if (err ==
        ZIP_OK) /* total number of entries in the central dir on this disk */
    {
        {
            if (zi->number_entry >= 0xFFFF)
                err =
                    zip64local_putValue(&zi->z_filefunc, zi->filestream, 0xffff,
                                        2);  // use value in ZIP64 record
            else
                err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                          zi->number_entry, 2);
        }
    }

    if (err == ZIP_OK) /* total number of entries in the central dir */
    {
        if (zi->number_entry >= 0xFFFF)
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream, 0xffff,
                                      2);  // use value in ZIP64 record
        else
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                      zi->number_entry, 2);
    }

    if (err == ZIP_OK) /* size of the central directory */
        err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                  size_centraldir, 4);

    if (err == ZIP_OK) /* offset of start of central directory with respect to
                          the starting disk number */
    {
        ZPOS64_T pos =
            centraldir_pos_inzip - zi->add_position_when_writing_offset;
        if (pos >= 0xffffffff)
        {
            err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                                      0xffffffff, 4);
        }
        else
            err = zip64local_putValue(
                &zi->z_filefunc, zi->filestream,
                (centraldir_pos_inzip - zi->add_position_when_writing_offset),
                4);
    }

    return err;
}

static int Write_GlobalComment(zip64_internal *zi, const char *global_comment)
{
    int err = ZIP_OK;
    uInt size_global_comment = 0;

    if (global_comment != nullptr)
        size_global_comment = static_cast<uInt>(strlen(global_comment));

    err = zip64local_putValue(&zi->z_filefunc, zi->filestream,
                              size_global_comment, 2);

    if (err == ZIP_OK && size_global_comment > 0)
    {
        if (ZWRITE64(zi->z_filefunc, zi->filestream, global_comment,
                     size_global_comment) != size_global_comment)
            err = ZIP_ERRNO;
    }
    return err;
}

extern int ZEXPORT cpl_zipClose(zipFile file, const char *global_comment)
{
    int err = 0;
    uLong size_centraldir = 0;
    ZPOS64_T centraldir_pos_inzip;
    ZPOS64_T pos;

    if (file == nullptr)
        return ZIP_PARAMERROR;

    zip64_internal *zi = reinterpret_cast<zip64_internal *>(file);

    if (zi->in_opened_file_inzip == 1)
    {
        err = cpl_zipCloseFileInZip(file);
    }

#ifndef NO_ADDFILEINEXISTINGZIP
    if (global_comment == nullptr)
        global_comment = zi->globalcomment;
#endif

    centraldir_pos_inzip = ZTELL64(zi->z_filefunc, zi->filestream);
    if (err == ZIP_OK)
    {
        linkedlist_datablock_internal *ldi = zi->central_dir.first_block;
        while (ldi != nullptr)
        {
            if ((err == ZIP_OK) && (ldi->filled_in_this_block > 0))
                if (ZWRITE64(zi->z_filefunc, zi->filestream, ldi->data,
                             ldi->filled_in_this_block) !=
                    ldi->filled_in_this_block)
                    err = ZIP_ERRNO;

            size_centraldir += ldi->filled_in_this_block;
            ldi = ldi->next_datablock;
        }
    }
    free_linkedlist(&(zi->central_dir));

    pos = centraldir_pos_inzip - zi->add_position_when_writing_offset;
    if (pos >= 0xffffffff || zi->number_entry > 0xFFFF)
    {
        ZPOS64_T Zip64EOCDpos = ZTELL64(zi->z_filefunc, zi->filestream);
        Write_Zip64EndOfCentralDirectoryRecord(zi, size_centraldir,
                                               centraldir_pos_inzip);

        Write_Zip64EndOfCentralDirectoryLocator(zi, Zip64EOCDpos);
    }

    if (err == ZIP_OK)
        err = Write_EndOfCentralDirectoryRecord(zi, size_centraldir,
                                                centraldir_pos_inzip);

    if (err == ZIP_OK)
        err = Write_GlobalComment(zi, global_comment);

    if (ZCLOSE64(zi->z_filefunc, zi->filestream) != 0)
        if (err == ZIP_OK)
            err = ZIP_ERRNO;

#ifndef NO_ADDFILEINEXISTINGZIP
    TRYFREE(zi->globalcomment);
#endif
    TRYFREE(zi);

    return err;
}

/************************************************************************/
/* ==================================================================== */
/*   The following is a simplified CPL API for creating ZIP files       */
/*   exported from cpl_conv.h.                                          */
/* ==================================================================== */
/************************************************************************/

#include "cpl_minizip_unzip.h"

typedef struct
{
    zipFile hZip;
    char **papszFilenames;
} CPLZip;

/************************************************************************/
/*                            CPLCreateZip()                            */
/************************************************************************/

/** Create ZIP file */
void *CPLCreateZip(const char *pszZipFilename, char **papszOptions)

{
    const bool bAppend =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "APPEND", "FALSE"));
    char **papszFilenames = nullptr;

    if (bAppend)
    {
        zipFile unzF = cpl_unzOpen(pszZipFilename);
        if (unzF != nullptr)
        {
            if (cpl_unzGoToFirstFile(unzF) == UNZ_OK)
            {
                do
                {
                    char fileName[8193];
                    unz_file_info file_info;
                    cpl_unzGetCurrentFileInfo(unzF, &file_info, fileName,
                                              sizeof(fileName) - 1, nullptr, 0,
                                              nullptr, 0);
                    fileName[sizeof(fileName) - 1] = '\0';
                    papszFilenames = CSLAddString(papszFilenames, fileName);
                } while (cpl_unzGoToNextFile(unzF) == UNZ_OK);
            }
            cpl_unzClose(unzF);
        }
    }

    zipFile hZip = cpl_zipOpen(pszZipFilename, bAppend ? APPEND_STATUS_ADDINZIP
                                                       : APPEND_STATUS_CREATE);
    if (hZip == nullptr)
    {
        CSLDestroy(papszFilenames);
        return nullptr;
    }

    CPLZip *psZip = static_cast<CPLZip *>(CPLMalloc(sizeof(CPLZip)));
    psZip->hZip = hZip;
    psZip->papszFilenames = papszFilenames;
    return psZip;
}

/************************************************************************/
/*                         CPLCreateFileInZip()                         */
/************************************************************************/

/** Create a file in a ZIP file */
CPLErr CPLCreateFileInZip(void *hZip, const char *pszFilename,
                          char **papszOptions)

{
    if (hZip == nullptr)
        return CE_Failure;

    CPLZip *psZip = static_cast<CPLZip *>(hZip);

    if (CSLFindString(psZip->papszFilenames, pszFilename) >= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s already exists in ZIP file",
                 pszFilename);
        return CE_Failure;
    }

    const bool bCompressed =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "COMPRESSED", "TRUE"));

    char *pszCPFilename = nullptr;
    std::vector<GByte> abyExtra;
    // If the filename is not ASCII only, we need an extended field
    if (!CPLIsASCII(pszFilename, strlen(pszFilename)))
    {
        const char *pszDestEncoding = CPLGetConfigOption("CPL_ZIP_ENCODING",
#if defined(_WIN32) && !defined(HAVE_ICONV)
                                                         "CP_OEMCP"
#else
                                                         "CP437"
#endif
        );

        pszCPFilename = CPLRecode(pszFilename, CPL_ENC_UTF8, pszDestEncoding);

        /* Create a Info-ZIP Unicode Path Extra Field (0x7075) */
        const size_t nDataLength =
            sizeof(GByte) + sizeof(uint32_t) + strlen(pszFilename);
        if (abyExtra.size() + 2 * sizeof(uint16_t) + nDataLength >
            std::numeric_limits<uint16_t>::max())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Too much content to fit in ZIP ExtraField");
        }
        else
        {
            const uint16_t nHeaderIdLE = CPL_LSBWORD16(0x7075);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(&nHeaderIdLE),
                            reinterpret_cast<const GByte *>(&nHeaderIdLE) + 2);
            const uint16_t nDataLengthLE =
                CPL_LSBWORD16(static_cast<uint16_t>(nDataLength));
            abyExtra.insert(
                abyExtra.end(), reinterpret_cast<const GByte *>(&nDataLengthLE),
                reinterpret_cast<const GByte *>(&nDataLengthLE) + 2);
            const GByte nVersion = 1;
            abyExtra.push_back(nVersion);
            const uint32_t nNameCRC32 = static_cast<uint32_t>(
                crc32(0, reinterpret_cast<const Bytef *>(pszCPFilename),
                      static_cast<uInt>(strlen(pszCPFilename))));
            const uint32_t nNameCRC32LE = CPL_LSBWORD32(nNameCRC32);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(&nNameCRC32LE),
                            reinterpret_cast<const GByte *>(&nNameCRC32LE) + 4);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(pszFilename),
                            reinterpret_cast<const GByte *>(pszFilename) +
                                strlen(pszFilename));
        }
    }
    else
    {
        pszCPFilename = CPLStrdup(pszFilename);
    }

    const char *pszContentType =
        CSLFetchNameValue(papszOptions, "CONTENT_TYPE");
    if (pszContentType)
    {
        const size_t nDataLength = strlen("KeyValuePairs") + sizeof(GByte) +
                                   sizeof(uint16_t) + strlen("Content-Type") +
                                   sizeof(uint16_t) + strlen(pszContentType);
        if (abyExtra.size() + 2 * sizeof(uint16_t) + nDataLength >
            std::numeric_limits<uint16_t>::max())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Too much content to fit in ZIP ExtraField");
        }
        else
        {
            abyExtra.push_back(GByte('K'));
            abyExtra.push_back(GByte('V'));
            const uint16_t nDataLengthLE =
                CPL_LSBWORD16(static_cast<uint16_t>(nDataLength));
            abyExtra.insert(
                abyExtra.end(), reinterpret_cast<const GByte *>(&nDataLengthLE),
                reinterpret_cast<const GByte *>(&nDataLengthLE) + 2);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>("KeyValuePairs"),
                            reinterpret_cast<const GByte *>("KeyValuePairs") +
                                strlen("KeyValuePairs"));
            abyExtra.push_back(1);  // number of key/value pairs
            const uint16_t nKeyLen =
                CPL_LSBWORD16(static_cast<uint16_t>(strlen("Content-Type")));
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(&nKeyLen),
                            reinterpret_cast<const GByte *>(&nKeyLen) + 2);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>("Content-Type"),
                            reinterpret_cast<const GByte *>("Content-Type") +
                                strlen("Content-Type"));
            const uint16_t nValLen =
                CPL_LSBWORD16(static_cast<uint16_t>(strlen(pszContentType)));
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(&nValLen),
                            reinterpret_cast<const GByte *>(&nValLen) + 2);
            abyExtra.insert(abyExtra.end(),
                            reinterpret_cast<const GByte *>(pszContentType),
                            reinterpret_cast<const GByte *>(
                                pszContentType + strlen(pszContentType)));
        }
    }

    const bool bIncludeInCentralDirectory = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "INCLUDE_IN_CENTRAL_DIRECTORY", "YES"));
    const bool bZip64 = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "ZIP64", CPLGetConfigOption("CPL_CREATE_ZIP64", "ON")));

    // Set datetime to write
    zip_fileinfo fileinfo;
    memset(&fileinfo, 0, sizeof(fileinfo));
    const char *pszTimeStamp =
        CSLFetchNameValueDef(papszOptions, "TIMESTAMP", "NOW");
    GIntBig unixTime =
        EQUAL(pszTimeStamp, "NOW")
            ? time(nullptr)
            : static_cast<GIntBig>(std::strtoll(pszTimeStamp, nullptr, 10));
    struct tm brokenDown;
    CPLUnixTimeToYMDHMS(unixTime, &brokenDown);
    fileinfo.tmz_date.tm_year = brokenDown.tm_year;
    fileinfo.tmz_date.tm_mon = brokenDown.tm_mon;
    fileinfo.tmz_date.tm_mday = brokenDown.tm_mday;
    fileinfo.tmz_date.tm_hour = brokenDown.tm_hour;
    fileinfo.tmz_date.tm_min = brokenDown.tm_min;
    fileinfo.tmz_date.tm_sec = brokenDown.tm_sec;

    const int nErr = cpl_zipOpenNewFileInZip3(
        psZip->hZip, pszCPFilename, &fileinfo,
        abyExtra.empty() ? nullptr : abyExtra.data(),
        static_cast<uInt>(abyExtra.size()),
        abyExtra.empty() ? nullptr : abyExtra.data(),
        static_cast<uInt>(abyExtra.size()), "", bCompressed ? Z_DEFLATED : 0,
        bCompressed ? Z_DEFAULT_COMPRESSION : 0,
        /* raw = */ 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
        /* password = */ nullptr,
        /* crcForCtypting = */ 0, bZip64, bIncludeInCentralDirectory);

    CPLFree(pszCPFilename);

    if (nErr != ZIP_OK)
        return CE_Failure;

    if (bIncludeInCentralDirectory)
        psZip->papszFilenames =
            CSLAddString(psZip->papszFilenames, pszFilename);

    return CE_None;
}

/************************************************************************/
/*                         CPLWriteFileInZip()                          */
/************************************************************************/

/** Write in current file inside a ZIP file */
CPLErr CPLWriteFileInZip(void *hZip, const void *pBuffer, int nBufferSize)

{
    if (hZip == nullptr)
        return CE_Failure;

    CPLZip *psZip = static_cast<CPLZip *>(hZip);

    int nErr = cpl_zipWriteInFileInZip(psZip->hZip, pBuffer,
                                       static_cast<unsigned int>(nBufferSize));

    if (nErr != ZIP_OK)
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                         CPLCloseFileInZip()                          */
/************************************************************************/

/** Close current file inside ZIP file */
CPLErr CPLCloseFileInZip(void *hZip)

{
    if (hZip == nullptr)
        return CE_Failure;

    CPLZip *psZip = static_cast<CPLZip *>(hZip);

    int nErr = cpl_zipCloseFileInZip(psZip->hZip);

    if (nErr != ZIP_OK)
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                         CPLAddFileInZip()                            */
/************************************************************************/

/** Add a file inside a ZIP file opened/created with CPLCreateZip().
 *
 * This combines calls sto CPLCreateFileInZip(), CPLWriteFileInZip(),
 * and CPLCloseFileInZip() in a more convenient and powerful way.
 *
 * In particular, this enables to add a compressed file using the seek
 * optimization extension.
 *
 * Supported options are:
 * <ul>
 * <li>SOZIP_ENABLED=AUTO/YES/NO: whether to generate a SOZip index for the
 * file. The default can be changed with the CPL_SOZIP_ENABLED configuration
 * option.</li>
 * <li>SOZIP_CHUNK_SIZE: chunk size to use for SOZip generation. Defaults to
 * 32768.
 * </li>
 * <li>SOZIP_MIN_FILE_SIZE: minimum file size to consider to enable SOZip index
 * generation in SOZIP_ENABLED=AUTO mode. Defaults to 1 MB.
 * </li>
 * <li>NUM_THREADS: number of threads used for SOZip generation. Defaults to
 * ALL_CPUS.</li>
 * <li>TIMESTAMP=AUTO/NOW/timestamp_as_epoch_since_jan_1_1970: in AUTO mode,
 * the timestamp of pszInputFilename will be used (if available), otherwise
 * it will fallback to NOW.</li>
 * <li>CONTENT_TYPE=string: Content-Type value for the file. This is stored as
 * a key-value pair in the extra field extension 'KV' (0x564b) dedicated to
 * storing key-value pair metadata.</li>
 * </ul>
 *
 * @param hZip ZIP file handle
 * @param pszArchiveFilename Filename (in UTF-8) stored in the archive.
 * @param pszInputFilename Filename of the file to add. If NULL, fpInput must
 * not be NULL
 * @param fpInput File handle opened on the file to add. May be NULL if
 * pszInputFilename is provided.
 * @param papszOptions Options.
 * @param pProgressFunc Progress callback, or NULL.
 * @param pProgressData User data of progress callback, or NULL.
 * @return CE_None in case of success.
 *
 * @since GDAL 3.7
 */
CPLErr CPLAddFileInZip(void *hZip, const char *pszArchiveFilename,
                       const char *pszInputFilename, VSILFILE *fpInput,
                       CSLConstList papszOptions,
                       GDALProgressFunc pProgressFunc, void *pProgressData)
{
    if (!hZip || !pszArchiveFilename || (!pszInputFilename && !fpInput))
        return CE_Failure;

    CPLZip *psZip = static_cast<CPLZip *>(hZip);
    zip64_internal *zi = reinterpret_cast<zip64_internal *>(psZip->hZip);

    VSIVirtualHandleUniquePtr poFileHandleAutoClose;
    if (!fpInput)
    {
        fpInput = VSIFOpenL(pszInputFilename, "rb");
        if (!fpInput)
            return CE_Failure;
        poFileHandleAutoClose.reset(fpInput);
    }

    VSIFSeekL(fpInput, 0, SEEK_END);
    const auto nUncompressedSize = VSIFTellL(fpInput);
    VSIFSeekL(fpInput, 0, SEEK_SET);

    CPLStringList aosNewsOptions(papszOptions);
    bool bSeekOptimized = false;
    const char *pszSOZIP =
        CSLFetchNameValueDef(papszOptions, "SOZIP_ENABLED",
                             CPLGetConfigOption("CPL_SOZIP_ENABLED", "AUTO"));

    const char *pszChunkSize = CSLFetchNameValueDef(
        papszOptions, "SOZIP_CHUNK_SIZE",
        CPLGetConfigOption("CPL_VSIL_DEFLATE_CHUNK_SIZE", nullptr));
    const bool bChunkSizeSpecified = pszChunkSize != nullptr;
    if (!pszChunkSize)
        pszChunkSize = "1024K";
    unsigned nChunkSize = static_cast<unsigned>(atoi(pszChunkSize));
    if (strchr(pszChunkSize, 'K'))
        nChunkSize *= 1024;
    else if (strchr(pszChunkSize, 'M'))
        nChunkSize *= 1024 * 1024;
    nChunkSize =
        std::max(static_cast<unsigned>(1),
                 std::min(static_cast<unsigned>(UINT_MAX), nChunkSize));

    const char *pszMinFileSize = CSLFetchNameValueDef(
        papszOptions, "SOZIP_MIN_FILE_SIZE",
        CPLGetConfigOption("CPL_SOZIP_MIN_FILE_SIZE", "1M"));
    uint64_t nSOZipMinFileSize = std::strtoull(pszMinFileSize, nullptr, 10);
    if (strchr(pszMinFileSize, 'K'))
        nSOZipMinFileSize *= 1024;
    else if (strchr(pszMinFileSize, 'M'))
        nSOZipMinFileSize *= 1024 * 1024;
    else if (strchr(pszMinFileSize, 'G'))
        nSOZipMinFileSize *= 1024 * 1024 * 1024;

    std::vector<uint8_t> sozip_index;
    uint64_t nExpectedIndexSize = 0;
    constexpr unsigned nDefaultSOZipChunkSize = 32 * 1024;
    constexpr size_t nOffsetSize = 8;
    if (((EQUAL(pszSOZIP, "AUTO") && nUncompressedSize > nSOZipMinFileSize) ||
         (!EQUAL(pszSOZIP, "AUTO") && CPLTestBool(pszSOZIP))) &&
        ((bChunkSizeSpecified &&
          nUncompressedSize > static_cast<unsigned>(nChunkSize)) ||
         (!bChunkSizeSpecified && nUncompressedSize > nDefaultSOZipChunkSize)))
    {
        if (!bChunkSizeSpecified)
            nChunkSize = nDefaultSOZipChunkSize;

        bSeekOptimized = true;

        aosNewsOptions.SetNameValue(
            "UNCOMPRESSED_SIZE", CPLSPrintf(CPL_FRMT_GUIB, nUncompressedSize));

        zi->nOffsetSize = nOffsetSize;
        nExpectedIndexSize =
            32 + ((nUncompressedSize - 1) / nChunkSize) * nOffsetSize;
        if (nExpectedIndexSize >
            static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too big file w.r.t CHUNK_SIZE");
            return CE_Failure;
        }
        try
        {
            sozip_index.reserve(static_cast<size_t>(nExpectedIndexSize));
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate memory for SOZip index");
            return CE_Failure;
        }
        sozip_index.resize(32);
        uint32_t nVal32;
        // Version
        nVal32 = CPL_LSBWORD32(1);
        memcpy(sozip_index.data(), &nVal32, sizeof(nVal32));
        // Extra reserved space after 32 bytes of header
        nVal32 = CPL_LSBWORD32(0);
        memcpy(sozip_index.data() + 4, &nVal32, sizeof(nVal32));
        // Chunksize
        nVal32 = CPL_LSBWORD32(nChunkSize);
        memcpy(sozip_index.data() + 8, &nVal32, sizeof(nVal32));
        // SOZIPIndexEltSize
        nVal32 = CPL_LSBWORD32(static_cast<uint32_t>(nOffsetSize));
        memcpy(sozip_index.data() + 12, &nVal32, sizeof(nVal32));
        // Uncompressed size
        uint64_t nVal64 = nUncompressedSize;
        CPL_LSBPTR64(&nVal64);
        memcpy(sozip_index.data() + 16, &nVal64, sizeof(nVal64));
        zi->sozip_index = &sozip_index;

        zi->nChunkSize = nChunkSize;

        const char *pszThreads = CSLFetchNameValue(papszOptions, "NUM_THREADS");
        if (pszThreads == nullptr || EQUAL(pszThreads, "ALL_CPUS"))
            zi->nThreads = CPLGetNumCPUs();
        else
            zi->nThreads = atoi(pszThreads);
        zi->nThreads = std::max(1, std::min(128, zi->nThreads));
    }

    aosNewsOptions.SetNameValue("ZIP64",
                                nUncompressedSize > 0xFFFFFFFFU ? "YES" : "NO");

    if (pszInputFilename != nullptr &&
        aosNewsOptions.FetchNameValue("TIMESTAMP") == nullptr)
    {
        VSIStatBufL sStat;
        if (VSIStatL(pszInputFilename, &sStat) == 0 && sStat.st_mtime != 0)
        {
            aosNewsOptions.SetNameValue(
                "TIMESTAMP",
                CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(sStat.st_mtime)));
        }
    }

    if (CPLCreateFileInZip(hZip, pszArchiveFilename, aosNewsOptions.List()) !=
        CE_None)
    {
        zi->sozip_index = nullptr;
        zi->nChunkSize = 0;
        zi->nThreads = 0;
        return CE_Failure;
    }
    zi->nChunkSize = 0;
    zi->nThreads = 0;

    constexpr int CHUNK_READ_MAX_SIZE = 1024 * 1024;
    std::vector<GByte> abyChunk(CHUNK_READ_MAX_SIZE);
    vsi_l_offset nOffset = 0;
    while (true)
    {
        const int nRead = static_cast<int>(
            VSIFReadL(abyChunk.data(), 1, abyChunk.size(), fpInput));
        if (nRead > 0 &&
            CPLWriteFileInZip(hZip, abyChunk.data(), nRead) != CE_None)
        {
            CPLCloseFileInZip(hZip);
            zi->sozip_index = nullptr;
            return CE_Failure;
        }
        nOffset += nRead;
        if (pProgressFunc &&
            !pProgressFunc(nUncompressedSize == 0
                               ? 1.0
                               : double(nOffset) / nUncompressedSize,
                           nullptr, pProgressData))
        {
            CPLCloseFileInZip(hZip);
            zi->sozip_index = nullptr;
            return CE_Failure;
        }
        if (nRead < CHUNK_READ_MAX_SIZE)
            break;
    }

    if (CPLCloseFileInZip(hZip) != CE_None)
    {
        zi->sozip_index = nullptr;
        return CE_Failure;
    }

    if (bSeekOptimized && sozip_index.size() != nExpectedIndexSize)
    {
        // shouldn't happen
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sozip_index.size() (=%u) != nExpectedIndexSize (=%u)",
                 static_cast<unsigned>(sozip_index.size()),
                 static_cast<unsigned>(nExpectedIndexSize));
    }
    else if (bSeekOptimized)
    {
        std::string osIdxName;
        const char *pszLastSlash = strchr(pszArchiveFilename, '/');
        if (pszLastSlash)
        {
            osIdxName.assign(pszArchiveFilename,
                             pszLastSlash - pszArchiveFilename + 1);
            osIdxName += '.';
            osIdxName += pszLastSlash + 1;
        }
        else
        {
            osIdxName = '.';
            osIdxName += pszArchiveFilename;
        }
        osIdxName += ".sozip.idx";

        CPLStringList aosIndexOptions;
        aosIndexOptions.SetNameValue("COMPRESSED", "NO");
        aosIndexOptions.SetNameValue("ZIP64", "NO");
        aosIndexOptions.SetNameValue("INCLUDE_IN_CENTRAL_DIRECTORY", "NO");
        aosIndexOptions.SetNameValue(
            "TIMESTAMP", aosNewsOptions.FetchNameValue("TIMESTAMP"));
        if (CPLCreateFileInZip(hZip, osIdxName.c_str(),
                               aosIndexOptions.List()) != CE_None)
        {
            zi->sozip_index = nullptr;
            return CE_Failure;
        }

        if (CPLWriteFileInZip(hZip, sozip_index.data(),
                              static_cast<int>(sozip_index.size())) != CE_None)
        {
            zi->sozip_index = nullptr;
            CPLCloseFileInZip(hZip);
            return CE_Failure;
        }

        zi->sozip_index = nullptr;
        if (CPLCloseFileInZip(hZip) != CE_None)
        {
            return CE_Failure;
        }
    }

    zi->sozip_index = nullptr;

    return CE_None;
}

/************************************************************************/
/*                            CPLCloseZip()                             */
/************************************************************************/

/** Close ZIP file */
CPLErr CPLCloseZip(void *hZip)
{
    if (hZip == nullptr)
        return CE_Failure;

    CPLZip *psZip = static_cast<CPLZip *>(hZip);

    int nErr = cpl_zipClose(psZip->hZip, nullptr);

    psZip->hZip = nullptr;
    CSLDestroy(psZip->papszFilenames);
    psZip->papszFilenames = nullptr;
    CPLFree(psZip);

    if (nErr != ZIP_OK)
        return CE_Failure;

    return CE_None;
}
