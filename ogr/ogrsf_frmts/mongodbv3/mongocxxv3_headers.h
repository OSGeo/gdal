/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes MongoDB C++ v3 SDK headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef MONGOCXXV3_HEADERS_H
#define MONGOCXXV3_HEADERS_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#include <bsoncxx/config/version.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>

#if BSONCXX_VERSION_MAJOR < 4
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/types/value.hpp>
#endif

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/logger.hpp>
#include <mongocxx/options/client.hpp>
#include <mongocxx/options/ssl.hpp>

#endif
