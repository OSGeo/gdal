/******************************************************************************
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

#ifndef CRUNCH_HEADERS_H
#define CRUNCH_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

/* stdio.h needed before including crnlib.h, since the later needs NULL to be
 * defined */
#include <stdio.h>
#include "crnlib.h"
#include "dds_defs.h"

#endif  // CRUNCH_HEADERS_H
