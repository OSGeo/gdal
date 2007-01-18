/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions.
 * Author:   Ken Shih, kshih@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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

#include <stdio.h>
#include "ogrsf_frmts.h"
#include <oledb.h>

#ifdef SUPPORT_ATL_NET
#  define SF_SIMPLE_ARRAY CAtlArray
#  define GET_SIZE_MACRO GetCount
#else
#  define SF_SIMPLE_ARRAY CSimpleArray
#  define GET_SIZE_MACRO GetSize
#endif

class CSFSource;

OGRDataSource  *SFGetOGRDataSource(IUnknown *pUnk);
CSFSource      *SFGetCSFSource(IUnknown *pUnk);
char	       *SFGetInitDataSource(IUnknown *pIUnknownIn);
char          **SFGetProviderOptions( IUnknown *);
char           *SFGetLayerWKT( OGRLayer *, IUnknown * );
int             SFGetSRSIDFromWKT( const char *, IUnknown * );
HRESULT	     	SFReportError(HRESULT passed_hr, IID iid, DWORD providerCode,
                              char *pszText, ...);
void		SFRegisterOGRFormats();
int             SFWkbGeomTypeToDBGEOM( OGRwkbGeometryType );
OGRDataSource *SFDSCacheOpenDataSource( const char *pszDataSourceName );
void           SFDSCacheReleaseDataSource( OGRDataSource * );
void           SFDSCacheCleanup();


void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );
