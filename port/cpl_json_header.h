/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes json-c header
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGR_JSON_HEADER_H
#define OGR_JSON_HEADER_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
// json_inttypes.h(18,1): warning C4005: 'PRId64': macro redefinition.
#pragma warning(disable : 4005)
#endif

#include <json.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#undef json_object_object_foreachC
#define json_object_object_foreachC(obj, iter)                                 \
    for (iter.entry = json_object_get_object(obj)->head;                       \
         (iter.entry                                                           \
              ? (iter.key = (char *)iter.entry->k,                             \
                iter.val = (struct json_object *)iter.entry->v, iter.entry)    \
              : nullptr) != nullptr;                                           \
         iter.entry = iter.entry->next)

/*! @cond Doxygen_Suppress */
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
#ifndef DOXYGEN_SKIP
#include <memory>
#endif

    struct JsonObjectReleaser
    {
        void operator()(json_object *poObj) const
        {
            json_object_put(poObj);
        }
    };

    using JsonObjectUniquePtr =
        std::unique_ptr<json_object, JsonObjectReleaser>;

}  // extern "C++"

#endif /* def __cplusplus && !CPL_SUPRESS_CPLUSPLUS */
/*! @endcond */

#endif /* OGR_JSON_HEADER_H */
