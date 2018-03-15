#include "csf.h"
#include "csfimpl.h"


int csf_fseek(
    FILE* file,
    CSF_FADDR offset,
    int origin)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    return _fseeki64(file, offset, origin);
#else
    return fseek(file, offset, origin);
#endif
}


CSF_FADDR csf_ftell(
    FILE* file)
{
#if defined(_WIN32) && !defined(__MINGW32__)
    return _ftelli64(file);
#else
    return ftell(file);
#endif
}
