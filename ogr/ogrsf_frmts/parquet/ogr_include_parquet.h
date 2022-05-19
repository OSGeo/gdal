/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef OGR_INCLUDE_PARQUET_H
#define OGR_INCLUDE_PARQUET_H

#if defined(__GNUC__) && !defined(_MSC_VER)
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244 )  /*  warning 4244: 'initializing': conversion from 'int32_t' to 'int16_t', possible loss of data */
#pragma warning( disable : 4458 )  /*  warning 4458: declaration of 'type_id' hides class member */
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
#include "parquet/file_writer.h"
#include "parquet/schema.h"
#include "parquet/statistics.h"
#include "parquet/arrow/reader.h"
#include "parquet/arrow/writer.h"
#include "parquet/arrow/schema.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
