/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Private API for VSICurl
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_VSIL_CURL_PRIV_H_INCLUDED
#define CPL_VSIL_CURL_PRIV_H_INCLUDED

#include "cpl_vsi_virtual.h"

/* NOTE: this is private API for GDAL internal use. */
/* May change without notice. */
/* Used by the MBTiles driver for now. */

/* Return TRUE to go on downloading, FALSE to stop. */
typedef int (*VSICurlReadCbkFunc)(VSILFILE *fp, void *pabyBuffer,
                                  size_t nBufferSize, void *pfnUserData);

/* fp must be a VSICurl file handle, otherwise bad things will happen. */
/* bStopOnInterruptUntilUninstall must be set to TRUE if all downloads */
/* must be canceled after a first one has been stopped by the callback */
/* function.  In that case, downloads will restart after uninstalling the */
/* callback. */
int VSICurlInstallReadCbk(VSILFILE *fp, VSICurlReadCbkFunc pfnReadCbk,
                          void *pfnUserData,
                          int bStopOnInterruptUntilUninstall);
int VSICurlUninstallReadCbk(VSILFILE *fp);

void VSICurlAuthParametersChanged();

#endif  // CPL_VSIL_CURL_PRIV_H_INCLUDED
