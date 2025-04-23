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

#include <string>

std::string GDAL_KTX2_BASISU_GetCreationOptions(bool bIsKTX2);

void DeclareDeferredBASISUPlugin();
void DeclareDeferredKTX2Plugin();
