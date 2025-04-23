/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes Xerces-C headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGR_XERCES_HEADERS_H
#define OGR_XERCES_HEADERS_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#include <xercesc/framework/MemoryManager.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/sax/Locator.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLNetAccessor.hpp>
#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/util/XMLURL.hpp>

using namespace XERCES_CPP_NAMESPACE;

#endif /* OGR_XERCES_HEADERS_H */
