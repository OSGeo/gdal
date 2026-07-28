#include "mfhdf.h"
#include "HdfEosDef.h"

int32 VgetnameSafe(int32 vkey, char *vgname, size_t vgnamesize)
{
    uint16 name_len = 0;
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
}
