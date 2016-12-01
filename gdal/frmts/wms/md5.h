/* See md5.c for explanation and copyright information.  */

#ifndef MD5_H
#define MD5_H

/* Unlike previous versions of this code, uint32 need not be exactly
32 bits, merely 32 bits or more.  Choosing a data type which is 32
bits instead of 64 is not important; speed is considerably more
important.  ANSI guarantees that "unsigned long" will be big enough,
and always using it seems to have few disadvantages.  */
#if defined(CPL_BASE_H_INCLUDED)
// Alias cvs_uint32 to GUInt32
#define cvs_uint32 GUInt32
#else
typedef unsigned long cvs_uint32;
#endif

struct cvs_MD5Context {
    cvs_uint32 buf[4];
    cvs_uint32 bits[2];
    unsigned char in[64];
};

void cvs_MD5Init(struct cvs_MD5Context *context);
void cvs_MD5Update(struct cvs_MD5Context *context, unsigned char const *buf, unsigned len);
void cvs_MD5Final(unsigned char digest[16], struct cvs_MD5Context *context);
void cvs_MD5Transform(cvs_uint32 buf[4], const unsigned char in[64]);

#endif /* !MD5_H */
