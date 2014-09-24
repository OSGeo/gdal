#if defined(JPEG_DUAL_MODE_8_12)
#  define LIBJPEG_12_PATH "../jpeg/libjpeg12/jpeglib.h"
#  define NITFWriteJPEGBlock NITFWriteJPEGBlock_12
#  include "nitfwritejpeg.cpp"
#endif /* defined(JPEG_DUAL_MODE_8_12) */
