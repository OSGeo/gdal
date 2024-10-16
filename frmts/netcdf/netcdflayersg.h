/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Winor Chen <wchen329 at wisc.edu>
 *
 ******************************************************************************
 * Copyright (c) 2019, Winor Chen <wchen329 at wisc.edu>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef __NETCDFLAYERSG_H__
#define __NETCDFLAYERSG_H__
#include "netcdfsg.h"
#include "ogr_core.h"

namespace nccfdriver
{
OGRwkbGeometryType RawToOGR(geom_t type, int axis_count);

geom_t OGRtoRaw(OGRwkbGeometryType type);

bool OGRHasZandSupported(OGRwkbGeometryType type);

}  // namespace nccfdriver

#endif
