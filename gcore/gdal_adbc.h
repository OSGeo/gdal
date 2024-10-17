/******************************************************************************
 * Name:     gdal_adbc.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core ADBC related declarations.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2024, Dewey Dunnington <dewey@voltrondata.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_ADBC_H_INCLUDED
#define GDAL_ADBC_H_INCLUDED

/**
 * \file gdal_adbc.h
 *
 * C GDAL entry points for Arrow Database Connectivity (ADBC)
 *
 * These functions provide an opportunity to override the mechanism
 * that locates and loads ADBC drivers, or provide one if GDAL was
 * not built with ADBC driver manager support.
 *
 * \since GDAL 3.11
 */

#include "cpl_port.h"

#include <stdint.h>

CPL_C_START

/** Type of a callback function to load a ADBC driver. */
typedef uint8_t (*GDALAdbcLoadDriverFunc)(const char *driver_name,
                                          const char *entrypoint, int version,
                                          void *driver, void *error);

void CPL_DLL GDALSetAdbcLoadDriverOverride(GDALAdbcLoadDriverFunc init_func);

GDALAdbcLoadDriverFunc CPL_DLL GDALGetAdbcLoadDriverOverride(void);

CPL_C_END

#endif
