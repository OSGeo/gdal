/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_INCLUDE_ARROW_H
#define OGR_INCLUDE_ARROW_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
// warning 4244: 'initializing': conversion from 'int32_t' to 'int16_t',
// possible loss of data
#pragma warning(disable : 4244)
// warning C4324: 'arrow::internal::MemoryPoolStats': structure was padded
// due to alignment specifier
#pragma warning(disable : 4324)
// warning 4458: declaration of 'type_id' hides class member
#pragma warning(disable : 4458)
#endif

#include "arrow/array.h"
#include "arrow/builder.h"
#include "arrow/array/array_dict.h"
#include "arrow/c/bridge.h"
#include "arrow/filesystem/filesystem.h"
#if ARROW_VERSION_MAJOR >= 16
#include "arrow/filesystem/filesystem_library.h"
#endif
#include "arrow/ipc/reader.h"
#include "arrow/ipc/writer.h"
#include "arrow/io/file.h"
#include "arrow/memory_pool.h"
#include "arrow/record_batch.h"
#include "arrow/util/decimal.h"
#include "arrow/util/key_value_metadata.h"
#include "arrow/util/uri.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
