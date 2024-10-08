/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes JP2KAK SDK headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef JP2KAK_HEADERS_H
#define JP2KAK_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4458)
#endif

#include "jp2_local.h"

// Kakadu core includes
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"
#include "kdu_stripe_compressor.h"
#include "kdu_stripe_decompressor.h"
#include "kdu_arch.h"

// Application level includes
#include "kdu_file_io.h"
#include "jp2.h"

// ROI related.
#include "kdu_roi_processing.h"
#include "kdu_image.h"
#include "roi_sources.h"

// From rouault:
// I don't think JPIP support currently works due to changes in
// classes like kdu_window ... some fixing required if someone wants it.

// #define USE_JPIP

#ifdef USE_JPIP
#include "kdu_client.h"
#else
#define kdu_client void
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Before v7.5 Kakadu does not advertise its version well
// After v7.5 Kakadu has KDU_{MAJOR,MINOR,PATCH}_VERSION defines so it is easier
// For older releases compile with them manually specified
#ifndef KDU_MAJOR_VERSION
#error Compile with eg. -DKDU_MAJOR_VERSION=7 -DKDU_MINOR_VERSION=3 -DKDU_PATCH_VERSION=2 to specify Kakadu library version
#endif

#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 8)
// Before Kakadu 7.8, kdu_roi_rect was missing from libkdu_aXY
#define KDU_HAS_ROI_RECT
#endif

#endif  // JP2KAK_HEADERS_H
