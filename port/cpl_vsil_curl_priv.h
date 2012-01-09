/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Private API for VSICurl
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

#ifndef CPL_VSIL_CURL_PRIV_H_INCLUDED
#define CPL_VSIL_CURL_PRIV_H_INCLUDED

#include "cpl_vsi_virtual.h"

/* NOTE: this is private API for GDAL internal use. May change without notice. */
/* Used by the MBTiles driver for now */

/* Return TRUE to go on downloading, FALSE to stop */
typedef int (*VSICurlReadCbkFunc) (VSILFILE* fp, void *pabyBuffer, size_t nBufferSize, void* pfnUserData);

/* fp must be a VSICurl file handle, otherwise bad things will happen ! */
/* bStopOnInterrruptUntilUninstall must be set to TRUE if all downloads */
/* must be cancelled after a first one has been stopped by the callback function. */
/* In that case, downloads will restart after uninstalling the callback. */
int VSICurlInstallReadCbk(VSILFILE* fp, VSICurlReadCbkFunc pfnReadCbk, void* pfnUserData,
                          int bStopOnInterrruptUntilUninstall);
int VSICurlUninstallReadCbk(VSILFILE* fp);

#endif // CPL_VSIL_CURL_PRIV_H_INCLUDED
