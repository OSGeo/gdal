
#ifndef json_inttypes_h_
#define json_inttypes_h_

#include "json_config.h"

#if defined(_MSC_VER) && _MSC_VER <= 1700

/* Anything less than Visual Studio C++ 10 is missing stdint.h and inttypes.h */
typedef __int32 int32_t;
#define INT32_MIN    ((int32_t)_I32_MIN)
#define INT32_MAX    ((int32_t)_I32_MAX)
typedef __int64 int64_t;
#define INT64_MIN    ((int64_t)_I64_MIN)
#define INT64_MAX    ((int64_t)_I64_MAX)
#define PRId64 "I64d"
#define SCNd64 "I64d"

#else

#ifdef JSON_C_HAVE_INTTYPES_H
#include <inttypes.h>
#if defined(__MSVCRT__)
#  undef PRId64
#  define PRId64 "I64d"
#endif
#endif
/* inttypes.h includes stdint.h */

#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef SCNd64
#define SCNd64 "lld"
#endif
#ifndef INT32_MIN
#define INT32_MIN (-2147483647-1)
#endif
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL-1)
#endif

#endif

#endif
