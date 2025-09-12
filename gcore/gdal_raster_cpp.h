/******************************************************************************
 *
 * Name:     gdal_raster_cpp.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core Raster C++ declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_RASTER_CPP_H_INCLUDED
#define GDAL_RASTER_CPP_H_INCLUDED

/**
 * \file gdal_raster_cpp.h
 *
 * C++ GDAL raster entry points.
 *
 * Before GDAL 3.12, the equivalent file to include is gdal_priv.h (which still
 * exits in the GDAL 3.x series)
 *
 * \since GDAL 3.12
 */

#include "gdal_multidomainmetadata.h"
#include "gdal_majorobject.h"
#include "gdal_defaultoverviews.h"
#include "gdal_openinfo.h"
#include "gdal_gcp.h"
#include "gdal_geotransform.h"
#include "gdal_dataset.h"
#include "gdal_rasterblock.h"
#include "gdal_colortable.h"
#include "gdal_rasterband.h"
#include "gdal_maskbands.h"
#include "gdal_driver.h"
#include "gdal_drivermanager.h"
#include "gdal_asyncreader.h"
#include "gdal_relationship.h"
#include "gdal_cpp_functions.h"

#endif
