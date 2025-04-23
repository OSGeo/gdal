/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#ifndef OGRCADDRIVERCORE_H
#define OGRCADDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "CAD";

#define OGRCADDriverSetCommonMetadata                                          \
    PLUGIN_SYMBOL_NAME(OGRCADDriverSetCommonMetadata)

void OGRCADDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
