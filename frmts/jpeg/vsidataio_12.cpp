/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement JPEG read/write io indirection through VSI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Code partially derived from libjpeg jdatasrc.c and jdatadst.c.
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#if defined(JPEG_DUAL_MODE_8_12)
#if !defined(HAVE_JPEGTURBO_DUAL_MODE_8_12)
#define LIBJPEG_12_PATH "libjpeg12/jpeglib.h"
#endif

#define jpeg_vsiio_src jpeg_vsiio_src_12
#define jpeg_vsiio_dest jpeg_vsiio_dest_12
#define my_source_mgr my_source_mgr_12
#define my_src_ptr my_src_ptr_12
#define my_destination_mgr my_destination_mgr_12
#define my_dest_ptr my_dest_ptr_12

#include "vsidataio.cpp"

#endif
