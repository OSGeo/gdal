/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 convenience functions.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GH5_CONVENIENCE_H_INCLUDED_
#define GH5_CONVENIENCE_H_INCLUDED_

#include "hdf5_api.h"

#include "cpl_string.h"
#include "gdal.h"

/* release 1.6.3 or 1.6.4 changed the type of count in some api functions */

#if H5_VERS_MAJOR == 1 && H5_VERS_MINOR <= 6 &&                                \
    (H5_VERS_MINOR < 6 || H5_VERS_RELEASE < 3)
#define H5OFFSET_TYPE hssize_t
#else
#define H5OFFSET_TYPE hsize_t
#endif

bool GH5_FetchAttribute(hid_t loc_id, const char *pszName, CPLString &osResult,
                        bool bReportError = false);
bool GH5_FetchAttribute(hid_t loc_id, const char *pszName, double &dfResult,
                        bool bReportError = false);
GDALDataType GH5_GetDataType(hid_t TypeID);
bool GH5_CreateAttribute(hid_t loc_id, const char *pszAttrName, hid_t TypeID,
                         unsigned nMaxLen = 0);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName,
                        const char *pszValue);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, double dfValue);
bool GH5_WriteAttribute(hid_t loc_id, const char *pszAttrName, unsigned nValue);

#endif /* ndef GH5_CONVENIENCE_H_INCLUDED_ */
