/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes json-c header
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#ifndef OGR_JSON_HEADER_H
#define OGR_JSON_HEADER_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER))
#pragma GCC system_header
#endif

#include <json.h>

#undef json_object_object_foreachC
#define json_object_object_foreachC(obj,iter) \
 for(iter.entry = json_object_get_object(obj)->head; (iter.entry ? (iter.key = (char*)iter.entry->k, iter.val = (struct json_object*)iter.entry->v, iter.entry) : nullptr) != nullptr; iter.entry = iter.entry->next)

/*! @cond Doxygen_Suppress */
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
#ifndef DOXYGEN_SKIP
#include <memory>
#endif

struct JsonObjectReleaser
{
    void operator()(json_object* poObj) const { json_object_put(poObj); }
};

using JsonObjectUniquePtr = std::unique_ptr<json_object, JsonObjectReleaser>;

} // extern "C++"

#endif /* def __cplusplus && !CPL_SUPRESS_CPLUSPLUS */
/*! @endcond */

#endif /* OGR_JSON_HEADER_H */
