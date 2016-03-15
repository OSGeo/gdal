/******************************************************************************
 * $Id$
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement JPEG read/write io indirection through VSI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Code partially derived from libjpeg jdatasrc.c and jdatadst.c.
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#if defined(JPEG_DUAL_MODE_8_12)
#define LIBJPEG_12_PATH   "libjpeg12/jpeglib.h"

#define jpeg_vsiio_src    jpeg_vsiio_src_12
#define jpeg_vsiio_dest   jpeg_vsiio_dest_12
#define my_source_mgr     my_source_mgr_12
#define my_src_ptr        my_src_ptr_12
#define my_destination_mgr my_destination_mgr_12
#define my_dest_ptr      my_dest_ptr_12

#include "vsidataio.cpp"

#endif
