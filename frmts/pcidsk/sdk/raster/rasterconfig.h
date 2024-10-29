/******************************************************************************
 *
 * Purpose:  Configurations for the raster library.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RASTER_CONFIG_H
#define RASTER_CONFIG_H

// Use raster namespace.
#define USE_RASTER_NAMESPACE

#ifdef USE_RASTER_NAMESPACE
#define RASTER_NAMESPACE_BEGIN namespace raster {
#define RASTER_NAMESPACE_END }
#define USING_RASTER_NAMESPACE using namespace raster;
#else
#define RASTER_NAMESPACE_BEGIN
#define RASTER_NAMESPACE_END
#define USING_RASTER_NAMESPACE
#endif

#endif
