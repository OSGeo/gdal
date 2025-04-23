/******************************************************************************
 * Project:  OGR
 * Purpose:  Convenience functions for parsing with Xerces-C library
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_XERCES_INCLUDED
#define OGR_XERCES_INCLUDED

#ifdef HAVE_XERCES
#include "ogr_xerces_headers.h"
#endif

#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#ifdef HAVE_XERCES

/* All those functions are for in-tree drivers use only ! */

/* Thread-safe initialization/de-initialization. Calls should be paired */
bool CPL_DLL OGRInitializeXerces(void);
void CPL_DLL OGRDeinitializeXerces(void);

InputSource CPL_DLL *OGRCreateXercesInputSource(VSILFILE *fp);
void CPL_DLL OGRDestroyXercesInputSource(InputSource *is);

void CPL_DLL OGRStartXercesLimitsForThisThread(size_t nMaxMemAlloc,
                                               const char *pszMsgMaxMemAlloc,
                                               double dfTimeoutSecond,
                                               const char *pszMsgTimeout);
void CPL_DLL OGRStopXercesLimitsForThisThread();

namespace OGR
{
CPLString CPL_DLL transcode(const XMLCh *panXMLString, int nLimitingChars = -1);
CPLString CPL_DLL &transcode(const XMLCh *panXMLString, CPLString &osRet,
                             int nLimitingChars = -1);
}  // namespace OGR

#ifndef OGR_USING
using OGR::transcode;
#endif

void OGRCleanupXercesMutex(void);

#endif /* HAVE_XERCES */

#endif /* OGR_XERCES_INCLUDED */
