/******************************************************************************
 *
 * Project:  FileGDB
 * Purpose:  Import FileGDB SDK headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef FILEGDBSDK_HEADERS_H
#define FILEGDBSDK_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

// In C++11 mode, GCC no longer defines linux, as expected by the SDK
#if defined(__linux__) && !defined(linux)
#define linux
#endif

/* FGDB API headers */
#include "FileGDBAPI.h"

#endif
