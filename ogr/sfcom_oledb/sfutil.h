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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2002/08/29 19:01:43  warmerda
 * cleaned some cruft, added SFGetCSFSource, SFGetSRSIDFromWKT
 *
 * Revision 1.10  2002/08/09 21:33:52  warmerda
 * prepare some .net compatibility macros
 *
 * Revision 1.9  2002/05/08 20:27:48  warmerda
 * added support for caching OGRDataSources
 *
 * Revision 1.8  2002/05/06 15:12:39  warmerda
 * improve IErrorInfo support
 *
 * Revision 1.7  2001/11/09 20:48:58  warmerda
 * added functions for processing WKT and getting provider options
 *
 * Revision 1.6  2001/05/28 19:39:29  warmerda
 * added SFWkbGeomTypeToDBGEOM
 *
 * Revision 1.5  1999/07/23 19:20:27  kshih
 * Modifications for errors etc...
 *
 * Revision 1.4  1999/07/20 17:11:11  kshih
 * Use OGR code
 *
 * Revision 1.3  1999/06/25 18:17:25  kshih
 * Changes to get datasource from session/rowset/command
 *
 * Revision 1.2  1999/06/22 16:17:11  warmerda
 * added ogrcomdebug
 *
 * Revision 1.1  1999/06/22 15:53:54  kshih
 * Utility functions.
 */

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
