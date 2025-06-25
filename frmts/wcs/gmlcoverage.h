/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WCS.
 * Author:   Frank Warmerdam, warmerdam at pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_minixml.h"
#include "gdal_priv.h"

CPLErr WCSParseGMLCoverage(CPLXMLNode *psTree, int *pnXSize, int *pnYSize,
                           GDALGeoTransform &gt, char **ppszProjection);
