/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Convenience function for parsing with Expat library
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

#ifndef OGR_EXPATH_INCLUDED
#define OGR_EXPATH_INCLUDED

#ifdef HAVE_EXPAT

#include "cpl_port.h"
#include <expat.h>

/* Compatibility stuff for expat >= 1.95.0 and < 1.95.7 */
#ifndef XMLCALL
#define XMLCALL
#endif
#ifndef XML_STATUS_OK
#define XML_STATUS_OK    1
#define XML_STATUS_ERROR 0
#endif

/* XML_StopParser only available for expat >= 1.95.8 */
#if !defined(XML_MAJOR_VERSION) || (XML_MAJOR_VERSION * 10000 + XML_MINOR_VERSION * 100 + XML_MICRO_VERSION) < 19508
#define XML_StopParser(parser, resumable)
#warning "Expat version is too old and does not have XML_StopParser. Corrupted files could hang OGR"
#endif

/* Only for internal use ! */
XML_Parser CPL_DLL OGRCreateExpatXMLParser(void);

#endif /* HAVE_EXPAT */

#endif /* OGR_EXPATH_INCLUDED */
