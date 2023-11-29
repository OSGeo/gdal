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

#include "hdlr.h"

void HandlerBox::setHandlerType(uint32_t fourCC)
{
    handler_type = fourCC;
}

void HandlerBox::setName(const std::string &s)
{
    name = s;
}

uint64_t HandlerBox::getBodySize()
{
    uint64_t bodySize = 5 * sizeof(uint32_t) + name.size() + 1;
    return bodySize;
}

void HandlerBox::writeBodyTo(VSILFILE *fp)
{
    writeUint32Value(fp, 0);  // pre_defined
    writeFourCC(fp, handler_type);
    for (int i = 0; i < 3; i++)
    {
        writeUint32Value(fp, 0);  // reserved
    }
    writeStringValue(fp, name.c_str());
}
