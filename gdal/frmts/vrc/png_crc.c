/*
 * $Id: png_crc.c,v 1.9 2021/06/20 08:58:41 werdna Exp $
 *
 * http://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
 *
 */

#include "png_crc.h"

// This should be C not C++, so this is not needed ?
// ... unless compiled with something like $(CC) ... -std=c++11
CPL_C_START

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
// #include <unistd.h>

// #include <stdio.h>
#include <errno.h>
#include <string.h>

const unsigned long nBitsPerBytes=8;

/* Table of CRCs of all 8-bit messages. */
#define ncrc_table_size 256
// static const
unsigned long crc_table[ ncrc_table_size ]
#if 1
   =
    {
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
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
     0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
    }
#endif
    ;

/* Flag: has the table been computed? Initially false. */
// static
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void make_crc_table(void)
{
    // unsigned int n, k;

    unsigned long const crc_magic = 0xedb88320L;

    for (unsigned int n = 0; n < ncrc_table_size; n++) {
        unsigned long c = (unsigned long) n;
        for (unsigned int k = 0; k < nBitsPerBytes; k++) {
            if (c & 1U) {
                c = crc_magic ^ (c >> 1U);
            } else {
                c = c >> 1U;
            }
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
   
static unsigned long update_crc(const unsigned long crc,
                                const unsigned char *buf,
                                const unsigned int len)
{
    unsigned long c = crc;
    
    if (!crc_table_computed) {
        make_crc_table();
    }
    const unsigned char fullbyte = 0xff;
    for (unsigned int n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & fullbyte] ^ (c >> nBitsPerBytes);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
extern unsigned long pngcrc_for_VRC(const unsigned char *buf,
                                    const unsigned int len)
{
    const unsigned long full32bits = 0xffffffffL;
    return update_crc(full32bits, buf, len) ^ full32bits;
}

CPL_C_END

