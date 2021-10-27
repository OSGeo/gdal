/******************************************************************************
 *
 * Purpose:  Enumerations and data types used within PCI Geomatics.
 *
 ******************************************************************************
 * Copyright (c) 2009
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

#ifndef RASTER_TYPES_HH
#define RASTER_TYPES_HH

#include "raster/rasterconfig.h"
#include <vector>
#include <map>

RASTER_NAMESPACE_BEGIN

/// Enumeration of the different buffer types.
enum BufferType
{
    BT_UNKNOWN = -1,    /// Unknown buffer type.
    BT_BIT = 0,         /// 1 bit buffer type.
    BT_8S,              /// 8 bit signed integer buffer type.
    BT_8U,              /// 8 bit unsigned integer buffer type.
    BT_16S,             /// 16 bit signed integer buffer type.
    BT_16U,             /// 16 bit unsigned integer buffer type.
    BT_32S,             /// 32 bit signed integer buffer type.
    BT_32U,             /// 32 bit unsigned integer buffer type.
    BT_32R,             /// 32 bit real buffer type.
    BT_64S,             /// 64 bit signed integer buffer type.
    BT_64U,             /// 64 bit unsigned integer buffer type.
    BT_64R,             /// 64 bit real buffer type.
    BT_LAST             /// The last buffer type. Useful for array size.
};

/// Enumeration of the different channel types.
enum ChannelType
{
    CT_UNKNOWN = -1,    /// Unknown channel type.
    CT_BIT = 0,         /// 1 bit channel type.
    CT_8S,              /// 8 bit signed integer channel type.
    CT_8U,              /// 8 bit unsigned integer channel type.
    CT_16S,             /// 16 bit signed integer channel type.
    CT_16U,             /// 16 bit unsigned integer channel type.
    CT_32S,             /// 32 bit signed integer channel type.
    CT_32U,             /// 32 bit unsigned integer channel type.
    CT_32R,             /// 32 bit real channel type.
    CT_64S,             /// 64 bit signed integer channel type.
    CT_64U,             /// 64 bit unsigned integer channel type.
    CT_64R,             /// 64 bit real channel type.
    CT_C16S,            /// 16 bit signed integer complex channel type.
    CT_C16U,            /// 16 bit unsigned integer complex channel type.
    CT_C32S,            /// 32 bit signed integer complex channel type.
    CT_C32U,            /// 32 bit unsigned integer complex channel type.
    CT_C32R,            /// 32 bit real complex channel type.
    CT_LAST             /// The last channel type. Useful for array size.
};

/// Enumeration of the different interleaving types.
enum InterleaveType
{
    IT_PIXEL,           /// The pixel interleave type.
    IT_LINE,            /// The line interleave type.
    IT_BAND,            /// The band interleave type.
    IT_LAST             /// The last interleave type. Useful for array size.
};

typedef std::vector<int> ChannelList;

typedef std::vector<ChannelType> ChannelTypeList;

typedef std::map<ChannelType, unsigned> ChannelTypeMap;

RASTER_NAMESPACE_END

#endif
