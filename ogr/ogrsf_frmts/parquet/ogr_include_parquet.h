/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_INCLUDE_PARQUET_H
#define OGR_INCLUDE_PARQUET_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
// warning 4244: 'initializing': conversion from 'int32_t' to 'int16_t',
// possible loss of data
#pragma warning(disable : 4244)
// warning 4458: declaration of 'type_id' hides class member
#pragma warning(disable : 4458)
#endif

#include "arrow/builder.h"
#include "arrow/memory_pool.h"
#include "arrow/array/array_dict.h"
#include "arrow/io/file.h"
#include "arrow/ipc/writer.h"
#include "arrow/util/base64.h"
#include "arrow/util/compression.h"
#include "arrow/util/decimal.h"
#include "arrow/util/key_value_metadata.h"
#include "arrow/util/config.h"  // for ARROW_VERSION_MAJOR
#include "parquet/file_writer.h"
#include "parquet/schema.h"
#include "parquet/statistics.h"
#include "parquet/arrow/reader.h"
#include "parquet/arrow/writer.h"
#include "parquet/arrow/schema.h"

#ifdef GDAL_USE_ARROWDATASET
#include "arrow/filesystem/filesystem.h"
#include "arrow/compute/api_scalar.h"
#include "arrow/compute/cast.h"
#include "arrow/compute/registry.h"
#include "arrow/dataset/dataset.h"
#include "arrow/dataset/discovery.h"
#include "arrow/dataset/file_parquet.h"
#include "arrow/dataset/scanner.h"
#endif

#ifdef GDAL_USE_ARROWCOMPUTE
#include <arrow/compute/api.h>
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
