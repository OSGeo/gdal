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

#ifndef XERCESC_HEADERS_H
#define XERCESC_HEADERS_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#include <util/PlatformUtils.hpp>
#include <sax2/DefaultHandler.hpp>
#include <sax2/ContentHandler.hpp>
#include <sax2/SAX2XMLReader.hpp>
#include <sax2/XMLReaderFactory.hpp>
#include <dom/DOM.hpp>
#include <util/XMLString.hpp>
#include <sax2/Attributes.hpp>

using namespace XERCES_CPP_NAMESPACE;

#endif /* XERCESC_HEADERS_H */
