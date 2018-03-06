/* See md5.cpp for explanation and copyright information.  */

#ifndef CPL_MD5_H
#define CPL_MD5_H

#include "cpl_port.h"

CPL_C_START
const char CPL_DLL *CPLMD5String( const char *pszText );
CPL_C_END

#ifndef DOXYGEN_SKIP

struct CPLMD5Context {
    GUInt32 buf[4];
    GUInt32 bits[2];
    unsigned char in[64];
};

void CPLMD5Init( struct CPLMD5Context *context );
void CPLMD5Update( struct CPLMD5Context *context, unsigned char const *buf,
                   unsigned len );
void CPLMD5Final( unsigned char digest[16], struct CPLMD5Context *context );
void CPLMD5Transform( GUInt32 buf[4], const unsigned char inraw[64] );

#endif // #ifndef DOXYGEN_SKIP

#endif /* !CPL_MD5_H */
