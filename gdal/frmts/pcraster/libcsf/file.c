#include "csf.h" 
#include "csfimpl.h" 


int csf_fseek(
    FILE* file,
    CSF_FADDR offset,
    int origin)
{
    return
#ifdef _WIN32
        _fseeki64
#else
        fseek
#endif
            (file, offset, origin);
}


CSF_FADDR csf_ftell(
    FILE* file)
{
    return
#ifdef _WIN32
        _ftelli64
#else
        ftell
#endif
            (file);
}
