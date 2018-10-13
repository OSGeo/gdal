/*****************************************************************************
 * tendian.h
 *
 * DESCRIPTION
 *    This file contains all the utility functions that the Driver uses to
 * solve endian'ness related issues.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef MEMENDIAN_H
#define MEMENDIAN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "type.h"
#include "cpl_port.h"

/*
 * MadeOnIntel    ==> LittleEndian
 * NotMadeOnIntel ==> BigEndian
 */
#undef BIG_ENDIAN
#undef LITTLE_ENDIAN

#ifdef WORDS_BIGENDIAN
  #define BIG_ENDIAN
#else
  #define LITTLE_ENDIAN
#endif

/* The following #defines are used to make the code easier to read. */
#ifdef BIG_ENDIAN
  #define MEMCPY_BIG memcpy
  #define MEMCPY_LIT revmemcpy
#else
  #define MEMCPY_BIG revmemcpy
  #define MEMCPY_LIT memcpy
#endif

void memswp (void *Data, const size_t elem_size, const size_t num_elem);

void *revmemcpy (void *Dst, void *Src, const size_t len);
void *revmemcpyRay (void *Dst, void *Src, const size_t elem_size,
                    const size_t num_elem);

char memBitRead (void *Dst, size_t dstLen, void *Src, size_t numBits,
                 uChar * bufLoc, size_t *numUsed);
char memBitWrite (void *Src, size_t srcLen, void *Dst, size_t numBits,
                  uChar * bufLoc, size_t *numUsed);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#ifdef __cplusplus

#include "cpl_vsi.h"

/* The following #defines are used to make the code easier to read. */
#ifdef BIG_ENDIAN
  #define FREAD_BIG norfread
  #define FREAD_LIT revfread
  #define FWRITE_BIG fwrite
  #define FWRITE_LIT revfwrite
#else
  #define FREAD_BIG revfread
  #define FREAD_LIT norfread
  #define FWRITE_BIG revfwrite
  #define FWRITE_LIT fwrite
#endif

size_t norfread (void *Dst, size_t elem_size, size_t num_elem, VSILFILE *fp);
size_t revfread (void *Dst, size_t elem_size, size_t num_elem, VSILFILE *fp);
size_t revfwrite (void *Src, size_t elem_size, size_t num_elem, FILE *fp);

size_t FREAD_ODDINT_BIG (sInt4 * dst, uChar len, VSILFILE *fp);
size_t FREAD_ODDINT_LIT (sInt4 * dst, uChar len, VSILFILE *fp);
size_t FWRITE_ODDINT_BIG (sInt4 * src, uChar len, FILE *fp);
size_t FWRITE_ODDINT_LIT (sInt4 * src, uChar len, FILE *fp);

int fileBitRead (void *Dst, size_t dstLen, uShort2 num_bits, FILE *fp,
                 uChar * gbuf, sChar * gbufLoc);
char fileBitWrite (void *Src, size_t srcLen, uShort2 numBits, FILE *fp,
                   uChar * pbuf, sChar * pbufLoc);

#endif

#endif /* MEMENDIAN_H */
