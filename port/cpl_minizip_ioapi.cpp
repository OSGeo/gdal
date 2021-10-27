/* Modified version by Even Rouault. :
      - change fill_fopen_filefunc to cpl_fill_fopen_filefunc
      - port to VSIL*L API
      - Remove old C style function prototypes
      - Add support for ZIP64

 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>

   Original licence available in port/LICENCE_minizip
*/

/* ioapi.c -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include "cpl_port.h"
#include "cpl_minizip_ioapi.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "cpl_minizip_ioapi.h"
#include "cpl_vsi.h"
#include "zconf.h"
#include "zlib.h"

CPL_CVSID("$Id$")

static
voidpf ZCALLBACK fopen_file_func ( voidpf /* opaque */ ,
                                   const char* filename, int mode )
{
    VSILFILE* file = nullptr;
    const char* mode_fopen = nullptr;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
    {
        mode_fopen = "wb";
        if( filename != nullptr )
            return VSIFOpenExL(filename, mode_fopen, true);
    }

    if ((filename!=nullptr) && (mode_fopen != nullptr))
        file = VSIFOpenL(filename, mode_fopen);
    return file;
}

static
uLong ZCALLBACK fread_file_func ( voidpf /* opaque */, voidpf stream,
                                  void* buf, uLong size )
{
    uLong ret =
        static_cast<uLong>(VSIFReadL(buf, 1, static_cast<size_t>(size),
                                     static_cast<VSILFILE *>(stream)));
    return ret;
}

static
uLong ZCALLBACK fwrite_file_func ( voidpf /* opaque */, voidpf stream,
                                   const void* buf, uLong size )
{
    uLong ret =
        static_cast<uLong>(VSIFWriteL(buf, 1, static_cast<size_t>(size),
                                      static_cast<VSILFILE *>(stream)));
    return ret;
}

static
uLong64 ZCALLBACK ftell_file_func ( voidpf /* opaque */, voidpf stream )
{
    uLong64 ret;
    ret = VSIFTellL(reinterpret_cast<VSILFILE*>(stream));
    return ret;
}

static
long ZCALLBACK fseek_file_func ( voidpf /* opaque */, voidpf stream,
                                 uLong64 offset, int origin )
{
    int fseek_origin=0;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    return VSIFSeekL(reinterpret_cast<VSILFILE*>(stream), offset, fseek_origin);
}

static
int ZCALLBACK fclose_file_func ( voidpf /* opaque */, voidpf stream )
{
    return VSIFCloseL(reinterpret_cast<VSILFILE*>(stream));
}

static
int ZCALLBACK ferror_file_func ( voidpf /* opaque */, voidpf /* stream */ )
{
    // ret = ferror((FILE *)stream);
    return 0;
}

void cpl_fill_fopen_filefunc (zlib_filefunc_def*  pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen_file = fopen_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell_file = ftell_file_func;
    pzlib_filefunc_def->zseek_file = fseek_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = nullptr;
}
