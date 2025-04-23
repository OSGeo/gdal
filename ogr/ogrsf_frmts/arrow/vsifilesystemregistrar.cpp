/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow virtual file system using VSI
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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
        return "gdalvsi";
    }(),
    [](const arrow::fs::Uri &uri, const arrow::io::IOContext & /* io_context */,
       std::string *out_path)
        -> arrow::Result<std::shared_ptr<arrow::fs::FileSystem>>
    {
        constexpr std::string_view kScheme = "gdalvsi://";
        if (out_path)
            *out_path = uri.ToString().substr(kScheme.size());
        return std::make_shared<VSIArrowFileSystem>("ARROW", std::string());
    },
    {});
