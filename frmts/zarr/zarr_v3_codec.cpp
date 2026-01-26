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
/*                            ZarrV3Codec()                             */
/************************************************************************/

ZarrV3Codec::ZarrV3Codec(const std::string &osName) : m_osName(osName)
{
}

/************************************************************************/
/*                            ~ZarrV3Codec()                            */
/************************************************************************/

ZarrV3Codec::~ZarrV3Codec() = default;

/************************************************************************/
/*                     ZarrV3Codec::DecodePartial()                     */
/************************************************************************/

bool ZarrV3Codec::DecodePartial(VSIVirtualHandle *,
                                const ZarrByteVectorQuickResize &,
                                ZarrByteVectorQuickResize &,
                                std::vector<size_t> &, std::vector<size_t> &)
{
    // Normally we should not hit that...
    CPLError(CE_Failure, CPLE_NotSupported,
             "Codec %s does not support partial decoding", m_osName.c_str());
    return false;
}
