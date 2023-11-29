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

#include "fullbox.h"
#include <cstdint>
#include <vector>

class UncompressedFrameConfigBox : public FullBox
{
  public:
    UncompressedFrameConfigBox()
        : FullBox("uncC"), profile(0), sampling_type(0), interleave_type(0),
          block_size(0), components_little_endian(false), block_pad_lsb(false),
          block_little_endian(false), block_reversed(false), pad_unknown(true),
          pixel_size(0), row_align_size(0), tile_align_size(0),
          num_tile_cols_minus_one(0), num_tile_rows_minus_one(0)
    {
    }

    ~UncompressedFrameConfigBox()
    {
    }

    struct Component
    {
        uint16_t component_index;
        uint8_t component_bit_depth_minus_one;
        uint8_t component_format;
        uint8_t component_align_size;
    };

    void addComponent(Component component)
    {
        components.push_back(component);
    }

  protected:
    uint64_t getBodySize() override;

    void writeBodyTo(VSILFILE *fp) override;

  private:
    uint32_t profile;
    std::vector<Component> components;
    uint8_t sampling_type;
    uint8_t interleave_type;
    uint8_t block_size;
    bool components_little_endian;
    bool block_pad_lsb;
    bool block_little_endian;
    bool block_reversed;
    bool pad_unknown;
    uint32_t pixel_size;
    uint32_t row_align_size;
    uint32_t tile_align_size;
    uint32_t num_tile_cols_minus_one;
    uint32_t num_tile_rows_minus_one;
};
