/******************************************************************************
 *
 * Project:  MEM GDAL Datasets
 * Purpose:  C/Public declarations of MEM GDAL dataset objects.
 * Author:   Kristin Cowalcijk <kristincowalcijk@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Kristin Cowalcijk <kristincowalcijk@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_MEM_H_INCLUDED
#define GDAL_MEM_H_INCLUDED

/**
 * \file gdal_mem.h
 *
 * Public (C callable) entry points for MEM GDAL dataset objects.
 */

#include "cpl_port.h"
#include "gdal.h"

CPL_C_START

GDALDatasetH CPL_DLL MEMCreate(int nXSize, int nYSize, int nBands,
                               GDALDataType eType, CSLConstList papszOptions);

CPL_C_END

#endif /* GDAL_MEM_H_INCLUDED */
