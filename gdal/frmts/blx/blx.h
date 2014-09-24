/* libblx - Magellan BLX topo reader/writer library
 *
 * Copyright (c) 2008, Henrik Johansson <henrik@johome.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _BLX_H_INCLUDED
#define _BLX_H_INCLUDED

#include <stdio.h>

/* Constants */
#define BLX_UNDEF -32768
#define BLX_OVERVIEWLEVELS 4

/* Possible values of ctx.endian */
#define BIGENDIAN 1
#define LITTLEENDIAN 0

/* Data types */

typedef short int blxdata;

struct cellindex_s {
    int offset;	
    unsigned int datasize;       /* Uncompressed size */
    unsigned int compdatasize;   /* Compressed data size */
};

struct blxcontext_s {
    int xsize, ysize;
    int cell_xsize, cell_ysize;
    int cell_cols, cell_rows;
    double lon,lat;
    double pixelsize_lon, pixelsize_lat;

    int zscale;
    int maxchunksize;
    int minval, maxval;

    int endian;

    struct cellindex_s *cellindex;
    
    int debug;

    int fillundef;     /* If non-zero, fillundefval will be used instead of -32768 for undefined values in non-empty cells when
			a cell is written */ 
    int fillundefval; 

    FILE *fh;
    int write;
    int open;
};

typedef struct blxcontext_s blxcontext_t;

struct component_s {
    int n;
    blxdata *lut;
    int dlen;
    blxdata *data;
};

/* Included all functions in the global name space when testing */
#ifdef TEST
#define STATIC
#else
#define STATIC static
#endif

/* Define memory allocation and I/O function macros */
#ifdef GDALDRIVER
#include "cpl_conv.h"
#define BLXfopen VSIFOpen
#define BLXfclose VSIFClose
#define BLXfread VSIFRead
#define BLXfwrite VSIFWrite
#define BLXfseek VSIFSeek
#define BLXftell VSIFTell
#define BLXmalloc VSIMalloc
#define BLXfree CPLFree
#define BLXdebug0(text)              CPLDebug("BLX", text)
#define BLXdebug1(text, arg1)        CPLDebug("BLX", text, arg1)
#define BLXdebug2(text, arg1, arg2)  CPLDebug("BLX", text, arg1, arg2)
#define BLXerror0(text)              CPLError(CE_Failure, CPLE_AppDefined, text)
#define BLXnotice1(text, arg1)       CPLDebug("BLX", text, arg1)
#define BLXnotice2(text, arg1, arg2) CPLDebug("BLX", text, arg1, arg2)
#else
#define BLXfopen fopen
#define BLXfclose fclose
#define BLXfread fread
#define BLXfwrite fwrite
#define BLXfseek fseek
#define BLXftell ftell
#define BLXmalloc malloc
#define BLXfree free
#define BLXdebug0 printf
#define BLXdebug1 printf
#define BLXdebug2 printf
#define BLXerror0 printf
#define BLXnotice1 printf
#define BLXnotice2 printf
#endif

/* Function prototypes */
#ifdef TEST
STATIC int compress_chunk(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outbuflen);
STATIC int uncompress_chunk(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outbuflen);
STATIC blxdata *reconstruct_horiz(blxdata *base, blxdata *diff, unsigned rows, unsigned cols, blxdata *out);
STATIC blxdata *reconstruct_vert(blxdata *base, blxdata *diff, unsigned rows, unsigned cols, blxdata *out);
STATIC void decimate_horiz(blxdata *in, unsigned rows, unsigned cols, blxdata *base, blxdata *diff);
STATIC void decimate_vert(blxdata *in, unsigned int rows, unsigned int cols, blxdata *base, blxdata *diff);
STATIC blxdata *decode_celldata(blxcontext_t *blxcontext, unsigned char *inbuf, int len, int *side, blxdata *outbuf, int outbufsize, int overviewlevel);
#endif

blxcontext_t *blx_create_context(void);
void blx_free_context(blxcontext_t *ctx);
int blxopen(blxcontext_t *ctx, const char *filename, const char *rw);
int blxclose(blxcontext_t *ctx);
void blxprintinfo(blxcontext_t *ctx);
short *blx_readcell(blxcontext_t *ctx, int row, int col, short *buffer, int bufsize, int overviewlevel);
int blx_encode_celldata(blxcontext_t *ctx, blxdata *indata, int side, unsigned char *outbuf, int outbufsize);
int blx_checkheader(char *header);
int blx_writecell(blxcontext_t *ctx, blxdata *cell, int cellrow, int cellcol);

#endif
