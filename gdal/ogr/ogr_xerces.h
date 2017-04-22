/******************************************************************************
 * Project:  OGR
 * Purpose:  Convenience functions for parsing with Xerces-C library
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGR_XERCES_INCLUDED
#define OGR_XERCES_INCLUDED

// Must be first for DEBUG_BOOL case
#ifdef HAVE_XERCES
#include "ogr_xerces_headers.h"
#endif

#include "cpl_port.h"
#include "cpl_string.h"

#ifdef HAVE_XERCES

/* All those functions are for in-tree drivers use only ! */

/* Thread-safe initialization/de-initialization. Calls should be paired */
bool CPL_DLL OGRInitializeXerces(void);
void CPL_DLL OGRDeinitializeXerces(void);

namespace OGR
{
CPLString CPL_DLL transcode( const XMLCh *panXMLString, int nLimitingChars = -1 );
CPLString CPL_DLL &transcode( const XMLCh *panXMLString, CPLString& osRet,
                              int nLimitingChars = -1 );
}

#ifndef OGR_USING
using OGR::transcode;
#endif

#endif /* HAVE_XERCES */

void OGRCleanupXercesMutex(void);

#endif /* OGR_XERCES_INCLUDED */
