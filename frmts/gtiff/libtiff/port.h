/*
 * Warning, this file was automatically created by the TIFF configure script
 * VERSION:	 v3.5.5
 * DATE:	 Tue Mar 28 23:28:13 EST 2000
 * TARGET:	 i586-unknown-linux
 * CCOMPILER:	 /usr/bin/gcc-2.95.2 20000220 (Debian GNU/Linux)
 */
#ifndef _PORT_
#define _PORT_ 1
#include "cpl_port.h"
    
#ifdef __cplusplus
extern "C" {
#endif

#ifdef CPL_LSB
#define HOST_BIGENDIAN 0
#else
#define HOST_BIGENDIAN 1
#endif
    
#define HOST_FILLORDER FILLORDER_LSB2MSB
#define HOST_BIGENDIAN	0
#define HAVE_MMAP 0
    
typedef double dblparam_t;
    
#define	INLINE
    
#define GLOBALDATA(TYPE,NAME)	extern TYPE NAME

#ifdef __cplusplus
}
#endif
#endif
