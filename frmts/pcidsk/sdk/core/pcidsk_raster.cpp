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

#include "pcidsk_raster.h"

using namespace PCIDSK;

/************************************************************************/
/*                             RasterDataType()                         */
/************************************************************************/
raster::ChannelType PCIDSK::RasterDataType(eChanType eChanType)
{
    switch (eChanType)
    {
        case CHN_8U:
            return raster::CT_8U;
        case CHN_16S:
            return raster::CT_16S;
        case CHN_16U:
            return raster::CT_16U;
        case CHN_32S:
            return raster::CT_32S;
        case CHN_32U:
            return raster::CT_32U;
        case CHN_32R:
            return raster::CT_32R;
        case CHN_64S:
            return raster::CT_64S;
        case CHN_64U:
            return raster::CT_64U;
        case CHN_64R:
            return raster::CT_64R;
        case CHN_C16S:
            return raster::CT_C16S;
        case CHN_C16U:
            return raster::CT_C16U;
        case CHN_C32S:
            return raster::CT_C32S;
        case CHN_C32U:
            return raster::CT_C32U;
        case CHN_C32R:
            return raster::CT_C32R;
        case CHN_BIT:
            return raster::CT_BIT;
        default:
            break;
    }

    return raster::CT_UNKNOWN;
}

/************************************************************************/
/*                             RasterDataType()                         */
/************************************************************************/
eChanType PCIDSK::RasterDataType(raster::ChannelType eChanType)
{
    switch (eChanType)
    {
        case raster::CT_8U:
            return CHN_8U;
        case raster::CT_16S:
            return CHN_16S;
        case raster::CT_16U:
            return CHN_16U;
        case raster::CT_32S:
            return CHN_32S;
        case raster::CT_32U:
            return CHN_32U;
        case raster::CT_32R:
            return CHN_32R;
        case raster::CT_64S:
            return CHN_64S;
        case raster::CT_64U:
            return CHN_64U;
        case raster::CT_64R:
            return CHN_64R;
        case raster::CT_C16S:
            return CHN_C16S;
        case raster::CT_C16U:
            return CHN_C16U;
        case raster::CT_C32S:
            return CHN_C32S;
        case raster::CT_C32U:
            return CHN_C32U;
        case raster::CT_C32R:
            return CHN_C32R;
        case raster::CT_BIT:
            return CHN_BIT;
        default:
            break;
    }

    return CHN_UNKNOWN;
}
