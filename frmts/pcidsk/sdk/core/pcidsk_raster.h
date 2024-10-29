/******************************************************************************
 *
 * Purpose:  PCI raster namespace converter utilities.
 *
 ******************************************************************************
 * Copyright (c) 2020
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_PCIDSK_RASTER_H
#define INCLUDE_PCIDSK_RASTER_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "raster/rastertypes.hh"

namespace PCIDSK
{
    raster::ChannelType PCIDSK_DLL RasterDataType(eChanType eChanType);
    eChanType PCIDSK_DLL RasterDataType(raster::ChannelType eChanType);
}

#endif
