/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  IO Redirection via VSI services for shp/dbf io.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007,  Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef SHP_VSI_H_INCLUDED
#define SHP_VSI_H_INCLUDED

#ifdef RENAME_INTERNAL_SHAPELIB_SYMBOLS
#include "gdal_shapelib_symbol_rename.h"
#endif

#include "cpl_vsi.h"
#include "shapefil.h"

CPL_C_START

const SAHooks *VSI_SHP_GetHook(int b2GBLimit);

VSILFILE *VSI_SHP_GetVSIL(SAFile file);
const char *VSI_SHP_GetFilename(SAFile file);
int VSI_SHP_WriteMoreDataOK(SAFile file, SAOffset nExtraBytes);

CPL_C_END

#endif /* SHP_VSI_H_INCLUDED */
