/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2017, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

typedef enum
{
    TIFF_NOTYPE = 0,     /* placeholder */
    TIFF_BYTE = 1,       /* 8-bit unsigned integer */
    TIFF_ASCII = 2,      /* 8-bit bytes w/ last byte null */
    TIFF_SHORT = 3,      /* 16-bit unsigned integer */
    TIFF_LONG = 4,       /* 32-bit unsigned integer */
    TIFF_RATIONAL = 5,   /* 64-bit unsigned fraction */
    TIFF_SBYTE = 6,      /* !8-bit signed integer */
    TIFF_UNDEFINED = 7,  /* !8-bit untyped data */
    TIFF_SSHORT = 8,     /* !16-bit signed integer */
    TIFF_SLONG = 9,      /* !32-bit signed integer */
    TIFF_SRATIONAL = 10, /* !64-bit signed fraction */
    TIFF_FLOAT = 11,     /* !32-bit IEEE floating point */
    TIFF_DOUBLE = 12,    /* !64-bit IEEE floating point */
    TIFF_IFD = 13        /* %32-bit unsigned integer (offset) */
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
typedef struct
{
    GUInt16 tdir_tag;    /* see below */
    GUInt16 tdir_type;   /* data type; see below */
    GUInt32 tdir_count;  /* number of items; length in spec */
    GUInt32 tdir_offset; /* byte offset to field data */
} GDALEXIFTIFFDirEntry;

GByte CPL_DLL *EXIFCreate(char **papszEXIFMetadata, GByte *pabyThumbnail,
                          GUInt32 nThumbnailSize, GUInt32 nThumbnailWidth,
                          GUInt32 nThumbnailHeight, GUInt32 *pnOutBufferSize);

//! @endcond
