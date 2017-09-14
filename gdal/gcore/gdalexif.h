/******************************************************************************
 * $Id$
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2017, Even Rouault
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

typedef enum {
        TIFF_NOTYPE     = 0,    /* placeholder */
        TIFF_BYTE       = 1,    /* 8-bit unsigned integer */
        TIFF_ASCII      = 2,    /* 8-bit bytes w/ last byte null */
        TIFF_SHORT      = 3,    /* 16-bit unsigned integer */
        TIFF_LONG       = 4,    /* 32-bit unsigned integer */
        TIFF_RATIONAL   = 5,    /* 64-bit unsigned fraction */
        TIFF_SBYTE      = 6,    /* !8-bit signed integer */
        TIFF_UNDEFINED  = 7,    /* !8-bit untyped data */
        TIFF_SSHORT     = 8,    /* !16-bit signed integer */
        TIFF_SLONG      = 9,    /* !32-bit signed integer */
        TIFF_SRATIONAL  = 10,   /* !64-bit signed fraction */
        TIFF_FLOAT      = 11,   /* !32-bit IEEE floating point */
        TIFF_DOUBLE     = 12,   /* !64-bit IEEE floating point */
        TIFF_IFD        = 13    /* %32-bit unsigned integer (offset) */
} GDALEXIFTIFFDataType;

/*
 * TIFF Image File Directories are comprised of a table of field
 * descriptors of the form shown below.  The table is sorted in
 * ascending order by tag.  The values associated with each entry are
 * disjoint and may appear anywhere in the file (so long as they are
 * placed on a word boundary).
 *
 * If the value is 4 bytes or less, then it is placed in the offset
 * field to save space.  If the value is less than 4 bytes, it is
 * left-justified in the offset field.
 */
typedef struct {
        GUInt16          tdir_tag;       /* see below */
        GUInt16          tdir_type;      /* data type; see below */
        GUInt32          tdir_count;     /* number of items; length in spec */
        GUInt32          tdir_offset;    /* byte offset to field data */
} GDALEXIFTIFFDirEntry;

GByte* EXIFCreate(char**     papszEXIFMetadata,
                  GByte*     pabyThumbnail,
                  GUInt32    nThumbnailSize,
                  GUInt32    nThumbnailWidth,
                  GUInt32    nThumbnailHeight,
                  GUInt32   *pnOutBufferSize);


//! @endcond
