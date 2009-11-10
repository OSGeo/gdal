/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 convenience functions.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _GH5_CONVENIENCE_H_INCLUDED_
#define _GH5_CONVENIENCE_H_INCLUDED_

#define H5_USE_16_API

#include "hdf5.h"

#include "cpl_string.h"
#include "gdal.h"

/* release 1.6.3 or 1.6.4 changed the type of count in some api functions */

#if H5_VERS_MAJOR == 1 && H5_VERS_MINOR <= 6 \
       && (H5_VERS_MINOR < 6 || H5_VERS_RELEASE < 3)
#  define H5OFFSET_TYPE hssize_t
#else
#  define H5OFFSET_TYPE  hsize_t
#endif

bool GH5_FetchAttribute( hid_t loc_id, const char *pszName, 
                         CPLString &osResult, bool bReportError = false );
bool GH5_FetchAttribute( hid_t loc_id, const char *pszName,
                         double &dfResult, bool bReportError = false );
GDALDataType GH5_GetDataType(hid_t TypeID);

#endif /* ndef _GH5_CONVENIENCE_H_INCLUDED_ */
