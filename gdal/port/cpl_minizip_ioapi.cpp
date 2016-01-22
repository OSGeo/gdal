/* Modified version by Even Rouault. :
      - change fill_fopen_filefunc to cpl_fill_fopen_filefunc
      - port to VSIL*L API
      - Remove old C style function prototypes
      - Add support for ZIP64

 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>

   Original licence available in port/LICENCE_minizip
*/


/* ioapi.c -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include "cpl_vsi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "cpl_minizip_ioapi.h"


static
voidpf ZCALLBACK fopen_file_func OF((
   voidpf opaque,
   const char* filename,
   int mode));

static
uLong ZCALLBACK fread_file_func OF((
   voidpf opaque,
   voidpf stream,
   void* buf,
   uLong size));

static
uLong ZCALLBACK fwrite_file_func OF((
   voidpf opaque,
   voidpf stream,
   const void* buf,
   uLong size));

static
uLong64 ZCALLBACK ftell_file_func OF((
   voidpf opaque,
   voidpf stream));

static
long ZCALLBACK fseek_file_func OF((
   voidpf opaque,
   voidpf stream,
   uLong64 offset,
   int origin));

static
int ZCALLBACK fclose_file_func OF((
   voidpf opaque,
   voidpf stream));

static
int ZCALLBACK ferror_file_func OF((
   voidpf opaque,
   voidpf stream));

static
voidpf ZCALLBACK fopen_file_func (CPL_UNUSED voidpf opaque, const char* filename, int mode)
{
    VSILFILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = VSIFOpenL(filename, mode_fopen);
    return file;
}

static
uLong ZCALLBACK fread_file_func (CPL_UNUSED voidpf opaque, voidpf stream, void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)VSIFReadL(buf, 1, (size_t)size, (VSILFILE *)stream);
    return ret;
}

static
uLong ZCALLBACK fwrite_file_func (CPL_UNUSED voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)VSIFWriteL(buf, 1, (size_t)size, (VSILFILE *)stream);
    return ret;
}

static
uLong64 ZCALLBACK ftell_file_func (CPL_UNUSED voidpf opaque, voidpf stream)
{
    uLong64 ret;
    ret = VSIFTellL((VSILFILE *)stream);
    return ret;
}

static
long ZCALLBACK fseek_file_func (CPL_UNUSED voidpf  opaque, voidpf stream, uLong64 offset, int origin)
{
    int fseek_origin=0;
    long ret;
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
    ret = 0;
    VSIFSeekL((VSILFILE *)stream, offset, fseek_origin);
    return ret;
}

static
int ZCALLBACK fclose_file_func (CPL_UNUSED voidpf opaque, voidpf stream)
{
    int ret;
    ret = VSIFCloseL((VSILFILE *)stream);
    return ret;
}

static
int ZCALLBACK ferror_file_func (CPL_UNUSED voidpf opaque, CPL_UNUSED voidpf stream)
{
    int ret;
    ret = 0; // FIXME
    //ret = ferror((FILE *)stream);
    return ret;
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
    pzlib_filefunc_def->opaque = NULL;
}
