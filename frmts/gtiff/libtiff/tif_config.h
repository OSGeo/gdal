/* Get the common system configuration switches from the main file. */
#include "cpl_port.h"
#include <inttypes.h>

/* Libtiff specific switches. */

/* Support CCITT Group 3 & 4 algorithms */
#define CCITT_SUPPORT 1

/* Support LogLuv high dynamic range encoding */
#define LOGLUV_SUPPORT 1

/* Support LZW algorithm */
#define LZW_SUPPORT 1

/* Support NeXT 2-bit RLE algorithm */
#define NEXT_SUPPORT 1

/* Support Macintosh PackBits algorithm */
#define PACKBITS_SUPPORT 1

/* Support ThunderScan 4-bit RLE algorithm */
#define THUNDER_SUPPORT 1

/* Pick up YCbCr subsampling info from the JPEG data stream to support files
   lacking the tag (default enabled). */
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1

/* Treat extra sample as alpha (default enabled). The RGBA interface will
   treat a fourth sample with no EXTRASAMPLE_ value as being ASSOCALPHA. Many
   packages produce RGBA files but don't mark the alpha properly. */
#define DEFAULT_EXTRASAMPLE_AS_ALPHA 1

/* Support strip chopping (whether or not to convert single-strip uncompressed
   images to multiple strips of ~8Kb to reduce memory usage) */
#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP

#define CHUNKY_STRIP_READ_SUPPORT 1

/* Default size of the strip in bytes (when strip chopping enabled) */
#define STRIP_SIZE_DEFAULT 8192

/* Enable SubIFD tag (330) support */
#define SUBIFD_SUPPORT 1

/* Signed 16-bit type */
#define TIFF_INT16_T int16_t

/* Signed 32-bit type */
#define TIFF_INT32_T int32_t

/* Signed 64-bit type */
#define TIFF_INT64_T int64_t

/* Signed 8-bit type */
#define TIFF_INT8_T signed char

/* Pointer difference type */
#define TIFF_PTRDIFF_T ptrdiff_t

/* Signed size type */
#ifdef _WIN64
#  define TIFF_SSIZE_T GIntBig
#  define TIFF_SSIZE_FORMAT CPL_FRMT_GB_WITHOUT_PREFIX "d"
#  define TIFF_SIZE_FORMAT CPL_FRMT_GB_WITHOUT_PREFIX "u"
#else
#  define TIFF_SSIZE_T signed long
#  define TIFF_SSIZE_FORMAT "ld"
#  if SIZEOF_VOIDP == 8
#    define TIFF_SIZE_FORMAT "lu"
#  else
#    define TIFF_SIZE_FORMAT "u"
#  endif
#endif

/* Unsigned 16-bit type */
#define TIFF_UINT16_T uint16_t

/* Unsigned 32-bit type */
#define TIFF_UINT32_T uint32_t

/* Unsigned 64-bit type */
#define TIFF_UINT64_T uint64_t

/* Unsigned 8-bit type */
#define TIFF_UINT8_T uint8_t

#define TIFF_UINT64_FORMAT PRIu64
#define TIFF_INT64_FORMAT PRId64

#ifdef JPEG_DUAL_MODE_8_12
#  define LIBJPEG_12_PATH "../../jpeg/libjpeg12/jpeglib.h"
#endif

#ifndef SIZEOF_SIZE_T
#define SIZEOF_SIZE_T SIZEOF_VOIDP
#endif

#define HAVE_ASSERT_H

/* we may lie, but there are so many other places where GDAL assumes IEEE fp */
#define HAVE_IEEEFP 1

#ifdef RENAME_INTERNAL_LIBTIFF_SYMBOLS
#include "gdal_libtiff_symbol_rename.h"
#endif
