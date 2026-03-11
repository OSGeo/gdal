/******************************************************************************
 *
 * Project:  Memory Array Translator
 * Purpose:  C/Public declarations of MEM GDAL dataset objects.
 * Author:   GDAL contributors
 *
 ******************************************************************************
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_MEM_H_INCLUDED
#define GDAL_MEM_H_INCLUDED

/**
 * \file gdal_mem.h
 *
 * Public (C callable) entry points for MEM GDAL dataset objects.
 */

#include "gdal.h"

CPL_C_START

GDALDatasetH CPL_DLL CPL_STDCALL
MEMCreate(int nXSize, int nYSize, int nBands, GDALDataType eType,
          CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;

CPL_C_END

#endif /* GDAL_MEM_H_INCLUDED */
