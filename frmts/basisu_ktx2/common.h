/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"

void GDALInitBasisUTranscoder();
void GDALInitBasisUEncoder();

bool GDAL_KTX2_BASISU_CreateCopy(const char *pszFilename, GDALDataset *poSrcDS,
                                 bool bIsKTX2, CSLConstList papszOptions,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressData);
