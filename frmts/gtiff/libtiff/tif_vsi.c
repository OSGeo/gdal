/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implement system hook functions for libtiff on top of CPL/VSI,
 *           including > 2GB support.  Based on tif_unix.c from libtiff
 *           distribution.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam, warmerdam@pobox.com
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

/*
 * TIFF Library UNIX-specific Routines.
 */
#include "tiffiop.h"
#include "cpl_vsi.h"

CPL_INLINE static void CPL_IGNORE_RET_VAL_INT(CPL_UNUSED int unused) {}

static tsize_t
_tiffReadProc(thandle_t fd, tdata_t buf, tsize_t size)
{
    return VSIFReadL( buf, 1, size, (VSILFILE *) fd );
}

static tsize_t
_tiffWriteProc(thandle_t fd, tdata_t buf, tsize_t size)
{
    return VSIFWriteL( buf, 1, size, (VSILFILE *) fd );
}

static toff_t
_tiffSeekProc(thandle_t fd, toff_t off, int whence)
{
    if( VSIFSeekL( (VSILFILE *) fd, off, whence ) == 0 )
        return (toff_t) VSIFTellL( (VSILFILE *) fd );
    else
        return (toff_t) -1;
}

static int
_tiffCloseProc(thandle_t fd)
{
    return VSIFCloseL( (VSILFILE *) fd );
}

static toff_t
_tiffSizeProc(thandle_t fd)
{
    vsi_l_offset  old_off;
    toff_t        file_size;

    old_off = VSIFTellL( (VSILFILE *) fd );
    CPL_IGNORE_RET_VAL_INT(VSIFSeekL( (VSILFILE *) fd, 0, SEEK_END ));

    file_size = (toff_t) VSIFTellL( (VSILFILE *) fd );
    CPL_IGNORE_RET_VAL_INT(VSIFSeekL( (VSILFILE *) fd, old_off, SEEK_SET ));

    return file_size;
}

static int
_tiffMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize)
{
	(void) fd; (void) pbase; (void) psize;
	return (0);
}

static void
_tiffUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
	(void) fd; (void) base; (void) size;
}

/*
 * Open a TIFF file descriptor for read/writing.
 */
TIFF*
TIFFFdOpen(CPL_UNUSED int fd, CPL_UNUSED const char* name, CPL_UNUSED const char* mode)
{
	return NULL;
}

/*
 * Open a TIFF file for read/writing.
 */
TIFF*
TIFFOpen(const char* name, const char* mode)
{
	static const char module[] = "TIFFOpen";
	int           i, a_out;
        char          szAccess[32];
        VSILFILE          *fp;
        TIFF          *tif;
        char         *pszAccess = szAccess;

        a_out = 0;
        pszAccess[0] = '\0';
        for( i = 0; mode[i] != '\0'; i++ )
        {
            if( mode[i] == 'r'
                || mode[i] == 'w'
                || mode[i] == '+'
                || mode[i] == 'a' )
            {
                szAccess[a_out++] = mode[i];
                szAccess[a_out] = '\0';
            }
        }

        strcat( szAccess, "b" );

        fp = VSIFOpenL( name, szAccess );
	if (fp == NULL) {
            if( errno >= 0 )
                TIFFError(module,"%s: %s", name, VSIStrerror( errno ) );
            else
		TIFFError(module, "%s: Cannot open", name);
            return ((TIFF *)0);
	}

	tif = TIFFClientOpen(name, mode,
	    (thandle_t) fp,
	    _tiffReadProc, _tiffWriteProc,
	    _tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
	    _tiffMapProc, _tiffUnmapProc);

        if( tif != NULL )
            tif->tif_fd = 0;
        else
            CPL_IGNORE_RET_VAL_INT(VSIFCloseL( fp ));
        
	return tif;
}

void*
_TIFFmalloc(tsize_t s)
{
    return VSIMalloc((size_t) s);
}

void* _TIFFcalloc(tmsize_t nmemb, tmsize_t siz)
{
    if( nmemb == 0 || siz == 0 )
        return ((void *) NULL);

    return VSICalloc((size_t) nmemb, (size_t)siz);
}

void
_TIFFfree(tdata_t p)
{
    VSIFree( p );
}

void*
_TIFFrealloc(tdata_t p, tsize_t s)
{
    return VSIRealloc( p, s );
}

void
_TIFFmemset(void* p, int v, tmsize_t c)
{
	memset(p, v, (size_t) c);
}

void
_TIFFmemcpy(void* d, const void* s, tmsize_t c)
{
	memcpy(d, s, (size_t) c);
}

int
_TIFFmemcmp(const void* p1, const void* p2, tmsize_t c)
{
	return (memcmp(p1, p2, (size_t) c));
}

static void
unixWarningHandler(const char* module, const char* fmt, va_list ap)
{
	if (module != NULL)
		fprintf(stderr, "%s: ", module);
	fprintf(stderr, "Warning, ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ".\n");
}
TIFFErrorHandler _TIFFwarningHandler = unixWarningHandler;

static void
unixErrorHandler(const char* module, const char* fmt, va_list ap)
{
	if (module != NULL)
		fprintf(stderr, "%s: ", module);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ".\n");
}
TIFFErrorHandler _TIFFerrorHandler = unixErrorHandler;
