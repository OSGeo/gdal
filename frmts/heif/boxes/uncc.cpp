/******************************************************************************
 *
 * Project:  HEIF driver
 * Author:   Brad Hards <bradh@frogmouth.net>
 *
 ******************************************************************************
 * Copyright (c) 2023, Brad Hards <bradh@frogmouth.net>
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

#include "uncc.h"
#include <cstdint>

uint64_t UncompressedFrameConfigBox::getBodySize()
{
    uint64_t size = 0;
    size += sizeof(uint32_t);  // profile
    if (version == 1)
    {
    }
    else if (version == 0)
    {
        size += sizeof(uint32_t);         // component_count
        size += (5 * components.size());  // 5 bytes per component
        size +=
            (4 * sizeof(uint8_t));  // sampling, interleave, block + flag bits
        size += (5 * sizeof(uint32_t));  // pixel_size onwards
    }
    return size;
}

void UncompressedFrameConfigBox::writeBodyTo(VSILFILE *fp)
{
    writeUint32Value(fp, profile);
    if (version == 1)
    {
    }
    else if (version == 0)
    {
        writeUint32Value(fp, components.size());
        for (size_t i = 0; i < components.size(); i++)
        {
            writeUint16Value(fp, components[i].component_index);
            writeUint8Value(fp, components[i].component_bit_depth_minus_one);
            writeUint8Value(fp, components[i].component_format);
            writeUint8Value(fp, components[i].component_align_size);
        }
        writeUint8Value(fp, sampling_type);
        writeUint8Value(fp, interleave_type);
        writeUint8Value(fp, block_size);
        uint8_t flagBits = 0;
        flagBits |= components_little_endian ? 0x80 : 0x00;
        flagBits |= block_pad_lsb ? 0x40 : 0x00;
        flagBits |= block_little_endian ? 0x20 : 0x00;
        flagBits |= block_reversed ? 0x10 : 0x00;
        flagBits |= pad_unknown ? 0x08 : 0x00;
        writeUint8Value(fp, flagBits);
        writeUint32Value(fp, pixel_size);
        writeUint32Value(fp, row_align_size);
        writeUint32Value(fp, tile_align_size);
        writeUint32Value(fp, num_tile_cols_minus_one);
        writeUint32Value(fp, num_tile_rows_minus_one);
    }
}
