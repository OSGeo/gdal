/******************************************************************************
 *
 * Purpose:  PCI raster namespace converter utilities.
 *
 ******************************************************************************
 * Copyright (c) 2020
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
