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

#ifndef INCLUDE_HEIF_FULLBOX_DEFINED
#define INCLUDE_HEIF_FULLBOX_DEFINED

#include "box.h"
#include <cstdint>

class FullBox : public Box
{
  protected:
    explicit FullBox(const char *fourCC) : Box(fourCC), version(0)
    {
        memset(flags, 0, 3);
    }

    virtual uint32_t getHeaderSize() override;
    virtual void writeHeader(VSILFILE *fp) override;

    void writeFlagsTo(VSILFILE *fp);

    uint8_t version;

    /**
     * Full box flags value.
     *
     * ISO IEC 14496-12 requires this to be unsigned int(24).
     *
     * The convention is that flags[2] is the high byte which is written first, and flags[0] is the low byte which is written last.
    */
    uint8_t flags[3];

    uint8_t getFlags(uint8_t index)
    {
        return flags[index];
    }
};

#endif
