/*
 * $Id: png_crc.h,v 1.7 2021/06/15 10:05:06 werdna Exp $
 */

#pragma once

#ifndef PNG_CRC_H_INCLUDED
#define PNG_CRC_H_INCLUDED

#include <cpl_port.h>

CPL_C_START
      
#ifdef INTERNAL_PNG
#include "../png/libpng/png.h"
#else
#include <png.h>
#endif

/* Return the PNG CRC of the bytes buf[0..len-1]. */
extern unsigned long pngcrc_for_VRC(
                                    const unsigned char *buf,
                                    const unsigned int len);

CPL_C_END

#endif // PNG_CRC_H_INCLUDED
