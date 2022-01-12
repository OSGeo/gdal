
/**
 * @file
 * @brief Do not use, json-c internal, may be changed or removed at any time.
 */
#ifndef _json_inttypes_h_
#define _json_inttypes_h_

#include "json_config.h"

#ifdef JSON_C_HAVE_INTTYPES_H
#include <inttypes.h>
#if defined(__MSVCRT__) && !defined(__MINGW64__)
#  undef PRId64
#  define PRId64 "I64d"
#  undef SCNd64
#  define SCNd64 "I64d"
#  undef PRIu64
#  define PRIU64 "I64u"
#endif
#endif
/* inttypes.h includes stdint.h */

#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
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
