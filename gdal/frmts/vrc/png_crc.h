/*
 * $Id: png_crc.h,v 1.5 2021/04/24 10:46:54 werdna Exp werdna $
 */

#pragma once

#ifndef PNG_CRC_H_INCLUDED
#define PNG_CRC_H_INCLUDED

// This should be C not C++, so this is not needed ?
#ifdef __cplusplus
  extern "C" {     // CPL_C_START
#endif
      
/* Return the PNG CRC of the bytes buf[0..len-1]. */
extern unsigned long pngcrc_for_VRC(const unsigned char *buf, const unsigned int len);

#ifdef __cplusplus
  } // extern "C" // CPL_C_END
#endif

#endif // PNG_CRC_H_INCLUDED
