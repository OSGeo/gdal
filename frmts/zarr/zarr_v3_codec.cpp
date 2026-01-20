/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, ZarrV3Codec class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

/************************************************************************/
/*                          ZarrV3Codec()                               */
/************************************************************************/

ZarrV3Codec::ZarrV3Codec(const std::string &osName) : m_osName(osName)
{
}

/************************************************************************/
/*                         ~ZarrV3Codec()                               */
/************************************************************************/

ZarrV3Codec::~ZarrV3Codec() = default;
