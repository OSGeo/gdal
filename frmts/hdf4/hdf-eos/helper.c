#include <stdint.h>

#include "mfhdf.h"
#include "HdfEosDef.h"
#include "hfile.h"

int32_t VgetnameSafe(int32_t vkey, char *vgname, size_t vgnamesize)
{
#if LIBVER_MAJOR == 4 && LIBVER_MINOR >= 4
    size_t len = 0;
    if( Vgetname(vkey, NULL, &len) != SUCCEED ||
        len >= vgnamesize ||
        Vgetname(vkey, vgname, &vgnamesize) != SUCCEED)
    {
         vgname[0] = 0;
         return DFE_BADLEN;
    }
    return SUCCEED;
#else
    uint16_t name_len = 0;
    if (Vgetnamelen(vkey, &name_len) != SUCCEED)
    {
         vgname[0] = 0;
         return DFE_BADLEN;
    }
    if (name_len >= vgnamesize )
    {
         vgname[0] = 0;
         return DFE_BADLEN;
    }
    return Vgetname(vkey, vgname);
#endif
}
