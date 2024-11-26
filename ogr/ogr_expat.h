/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Convenience function for parsing with Expat library
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_EXPATH_INCLUDED
#define OGR_EXPATH_INCLUDED

#ifdef HAVE_EXPAT

#include "cpl_port.h"
#include <expat.h>

#include <memory>

/* Compatibility stuff for expat >= 1.95.0 and < 1.95.7 */
#ifndef XMLCALL
#define XMLCALL
#endif
#ifndef XML_STATUS_OK
#define XML_STATUS_OK 1
#define XML_STATUS_ERROR 0
#endif

/* XML_StopParser only available for expat >= 1.95.8 */
#if !defined(XML_MAJOR_VERSION) ||                                             \
    (XML_MAJOR_VERSION * 10000 + XML_MINOR_VERSION * 100 +                     \
     XML_MICRO_VERSION) < 19508
#define XML_StopParser(parser, resumable)
#warning                                                                       \
    "Expat version is too old and does not have XML_StopParser. Corrupted files could hang OGR"
#endif

/* Only for internal use ! */
XML_Parser CPL_DLL OGRCreateExpatXMLParser(void);

//
//! @cond Doxygen_Suppress
struct CPL_DLL OGRExpatUniquePtrDeleter
{
    void operator()(XML_Parser oParser) const
    {
        XML_ParserFree(oParser);
    }
};

//! @endcond

/** Unique pointer type for XML_Parser.
 * @since GDAL 3.2
 */
using OGRExpatUniquePtr =
    std::unique_ptr<XML_ParserStruct, OGRExpatUniquePtrDeleter>;

#endif /* HAVE_EXPAT */

#endif /* OGR_EXPATH_INCLUDED */
