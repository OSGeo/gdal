/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Functions for reading and scanning CSV (comma separated,
 *           variable length text files holding tables) files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_CSV_H_INCLUDED
#define GDAL_CSV_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
const char *GDALDefaultCSVFilename(const char *pszBasename);
CPL_C_END

#endif
