/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Import GTA headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef GTA_HEADERS_H
#define GTA_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <gta/gta.hpp>

#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

#endif
