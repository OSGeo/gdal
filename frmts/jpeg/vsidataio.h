/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement JPEG read/write io indirection through VSI.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VSIDATAIO_H_INCLUDED
#define VSIDATAIO_H_INCLUDED

#include "cpl_vsi.h"

CPL_C_START
#ifdef LIBJPEG_12_PATH
#include LIBJPEG_12_PATH
#else
#include "jpeglib.h"
#endif
CPL_C_END

void jpeg_vsiio_src(j_decompress_ptr cinfo, VSILFILE *infile);
void jpeg_vsiio_dest(j_compress_ptr cinfo, VSILFILE *outfile);

#endif  // VSIDATAIO_H_INCLUDED
