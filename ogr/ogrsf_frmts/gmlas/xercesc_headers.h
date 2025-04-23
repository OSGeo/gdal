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

#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/framework/psvi/XSAnnotation.hpp>
#include <xercesc/framework/psvi/XSAttributeDeclaration.hpp>
#include <xercesc/framework/psvi/XSAttributeGroupDefinition.hpp>
#include <xercesc/framework/psvi/XSAttributeUse.hpp>
#include <xercesc/framework/psvi/XSComplexTypeDefinition.hpp>
#include <xercesc/framework/psvi/XSElementDeclaration.hpp>
#include <xercesc/framework/psvi/XSFacet.hpp>
#include <xercesc/framework/psvi/XSModel.hpp>
#include <xercesc/framework/psvi/XSModelGroup.hpp>
#include <xercesc/framework/psvi/XSModelGroupDefinition.hpp>
#include <xercesc/framework/psvi/XSNamespaceItem.hpp>
#include <xercesc/framework/psvi/XSParticle.hpp>
#include <xercesc/framework/psvi/XSSimpleTypeDefinition.hpp>
#include <xercesc/framework/psvi/XSTypeDefinition.hpp>
#include <xercesc/framework/psvi/XSWildcard.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/sax/SAXException.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/sax2/ContentHandler.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/PSVIUni.hpp>
#include <xercesc/util/TranscodingException.hpp>
#include <xercesc/util/XMLException.hpp>
#include <xercesc/dom/DOMException.hpp>

using namespace XERCES_CPP_NAMESPACE;

#endif /* XERCESC_HEADERS_H */
