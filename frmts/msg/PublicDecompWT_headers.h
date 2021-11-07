#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244 ) /* conversion from 'int' to 'WORD', possible loss of data */
#pragma warning( disable : 4267 ) /* '=': conversion from 'size_t' to 'int', possible loss of data */
#endif

#include "PublicDecompWT/COMP/WT/Inc/CWTDecoder.h"
#include "PublicDecompWT/DISE/CDataField.h" // Util namespace

#ifdef _MSC_VER
#pragma warning( pop )
#endif
