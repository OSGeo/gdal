/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef LIBKML_HEADERS_H
#define LIBKML_HEADERS_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(                                                               \
    disable : 4512) /* assignment operator could not be generated  \
                                 */
#endif

#include <kml/engine.h>
#include <kml/dom.h>
#include <kml/base/color32.h>
#include <kml/base/file.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
