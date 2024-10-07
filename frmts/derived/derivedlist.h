/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of derived subdatasets
 * Author:   Julien Michel <julien dot michel at cnes dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2016 Julien Michel <julien dot michel at cnes dot fr>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/
#ifndef DERIVEDLIST_H_INCLUDED
#define DERIVEDLIST_H_INCLUDED

#include "cpl_port.h"

CPL_C_START

typedef struct
{
    const char *pszDatasetName;
    const char *pszDatasetDescription;
    const char *pszPixelFunction;
    const char *pszInputPixelType;
    const char *pszOutputPixelType;
} DerivedDatasetDescription;

const DerivedDatasetDescription CPL_DLL *CPL_STDCALL
GDALGetDerivedDatasetDescriptions(unsigned int *pnDescriptionCount);

CPL_C_END

#endif
