/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow virtual file system using VSI
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault, <even.rouault at spatialys.com>
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
 ****************************************************************************/

#include "cpl_error.h"

#include "../arrow_common/ogr_include_arrow.h"
#include "../arrow_common/vsiarrowfilesystem.hpp"

// Only available since Arrow 17.0
#ifndef ARROW_REGISTER_FILESYSTEM
namespace arrow::fs
{
#define ARROW_REGISTER_FILESYSTEM(scheme, factory_function, finalizer)         \
    ::arrow::fs::FileSystemRegistrar                                           \
    {                                                                          \
        scheme, ::arrow::fs::FileSystemFactory{factory_function}, finalizer    \
    }
}  // namespace arrow::fs
#endif

auto kVSIFileSystemModule = ARROW_REGISTER_FILESYSTEM(
    []()
    {
        CPLDebugOnly("ARROW", "Register VSI Arrow file system");
        return "vsi";
    }(),
    [](const arrow::fs::Uri &uri, const arrow::io::IOContext & /* io_context */,
       std::string *out_path)
        -> arrow::Result<std::shared_ptr<arrow::fs::FileSystem>>
    {
        constexpr std::string_view kScheme = "vsi://";
        if (out_path)
            *out_path = uri.ToString().substr(kScheme.size());
        return std::make_shared<VSIArrowFileSystem>("ARROW", std::string());
    },
    {});
