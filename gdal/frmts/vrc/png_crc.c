/*
 * $Id: png_crc.c,v 1.5 2021/04/24 10:46:54 werdna Exp werdna $
 *
 * http://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
 *
 */

// This should be C not C++, so this is not needed
#ifdef __cplusplus
  extern "C" {     // CPL_C_START
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "png_crc.h"

/* Table of CRCs of all 8-bit messages. */
static unsigned long crc_table[256] =
    {
     0
     /* This doesn't stop cppcheck bughunt from reporting
[png_crc.c:77]: (error) Cannot determine that 'buf[n]' is initialized
[png_crc.c:85] -> [png_crc.c:77]: (error) Cannot determine that 'buf[n]' is initialized

      ,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
     */
    };
   
/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;
   
/* Make the table for a fast CRC. */
static void make_crc_table(void)
{
    int n, k;
   
    for (n = 0; n < 256; n++) {
        unsigned long c = (unsigned long) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}
   
/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
   
static unsigned long update_crc(unsigned long crc, const unsigned char *buf,
                         unsigned int len)
{
    unsigned long c = crc;
    unsigned int n;
    
    if (!crc_table_computed) {
        make_crc_table();
    }
    for (n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}
   
/* Return the CRC of the bytes buf[0..len-1]. */
extern unsigned long
pngcrc_for_VRC(const unsigned char *buf, const unsigned int len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

#ifdef __cplusplus
  } // extern "C" // CPL_C_END
#endif

