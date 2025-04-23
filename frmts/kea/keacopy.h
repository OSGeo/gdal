/*
 *  keacopy.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEACOPY_H
#define KEACOPY_H

#include "gdal_priv.h"

#include "libkea_headers.h"

bool KEACopyFile(GDALDataset *pDataset, kealib::KEAImageIO *pImageIO,
                 GDALProgressFunc pfnProgress, void *pProgressData);

#endif
