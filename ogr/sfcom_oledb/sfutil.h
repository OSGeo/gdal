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
 *
 *
 */

#include <stdio.h>
#include "ogrsf_frmts.h"
#include <oledb.h>


OGRDataSource	*SFGetOGRDataSource(IUnknown *pUnk);
void			SFSetOGRDataSource(IUnknown *pUnk, OGRDataSource *pOGR);
void			SFClearOGRDataSource(IUnknown *pUnk);
void			SFGetFilenames(const char *,char **,char **);
char			*SFGetInitDataSource(IUnknown *pIUnknownIn);
HRESULT	     	SFReportError(HRESULT passed_hr, IID iid, DWORD providerCode,char *pszText);

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );
