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
#define HAVE_MMAP 0
    
typedef double dblparam_t;
    
#define	INLINE
    
#define GLOBALDATA(TYPE,NAME)	extern TYPE NAME

#include <sys/types.h>
#include <fcntl.h>

#ifdef __cplusplus
}
#endif
#endif
