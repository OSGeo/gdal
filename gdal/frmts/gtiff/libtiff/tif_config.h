/* Get the common system configuration switches from the main file. */
#include "cpl_port.h"

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
   images to mutiple strips of ~8Kb to reduce memory usage) */
#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP

#define CHUNKY_STRIP_READ_SUPPORT 1

/* Default size of the strip in bytes (when strip chopping enabled) */
#define STRIP_SIZE_DEFAULT 8192

/* Enable SubIFD tag (330) support */
#define SUBIFD_SUPPORT 1

/* Signed 16-bit type */
#define TIFF_INT16_T GInt16

/* Signed 32-bit type */
#define TIFF_INT32_T GInt32

/* Signed 64-bit type */
#define TIFF_INT64_T GIntBig

/* Signed 8-bit type */
#define TIFF_INT8_T signed char

/* Pointer difference type */
#define TIFF_PTRDIFF_T ptrdiff_t

/* Signed size type */
#ifdef _WIN64
#  define TIFF_SSIZE_T GIntBig
#  define TIFF_SSIZE_FORMAT CPL_FRMT_GIB
#else
#  define TIFF_SSIZE_T signed long
#  define TIFF_SSIZE_FORMAT "%ld"
#endif

/* Unsigned 16-bit type */
#define TIFF_UINT16_T GUInt16

/* Unsigned 32-bit type */
#define TIFF_UINT32_T GUInt32

/* Unsigned 64-bit type */
#define TIFF_UINT64_T GUIntBig

/* Unsigned 8-bit type */
#define TIFF_UINT8_T unsigned char

#define TIFF_UINT64_FORMAT CPL_FRMT_GUIB
#define TIFF_INT64_FORMAT CPL_FRMT_GIB

#ifdef JPEG_DUAL_MODE_8_12
#  define LIBJPEG_12_PATH "../../jpeg/libjpeg12/jpeglib.h"
#endif

/* GDAL specific to indicate that internal libtiff is patched */
/* with fix for GDAL ticket #3259. Can be removed as well as */
/* its reference in geotiff.cpp as soon as a libtiff 4.0.0beta6 */
/* will be released */
#define BUG_3259_FIXED  1
