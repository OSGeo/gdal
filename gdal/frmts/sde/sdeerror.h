#ifndef SDEERROR_INCLUDED
#define SDEERROR_INCLUDED


#include "gdal_sde.h"

void IssueSDEError( int nErrorCode, 
                    const char *pszFunction );
void IssueSDEExtendedError ( int nErrorCode,
                           const char *pszFunction,
                           SE_CONNECTION* connection,
                           SE_STREAM* stream);
                           

#endif
