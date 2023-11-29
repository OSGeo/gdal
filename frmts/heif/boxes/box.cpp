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

#include "box.h"
#include <cstddef>
#include <cstdint>

uint64_t Box::getFullSize()
{
    uint64_t size = getSize();
    if (size > 1)
    {
        return size;
    }
    return getHeaderSize() + sizeof(uint64_t) + getBodySize();
}

void Box::writeHeader(VSILFILE *fp)
{
    // TODO: rework this to handle uuid as well
    uint32_t size = (uint32_t)getSize();
    writeUint32Value(fp, size);
    writeBoxType(fp);
    if (size == 1)
    {
        uint64_t largesize = getFullSize();
        writeUint64Value(fp, largesize);
    }
}

void Box::writeBoxType(VSILFILE *fp)
{
    writeFourCC(fp, boxtype);
}

uint64_t Box::getSize()
{
    if (getHeaderSize() + getBodySize() > UINT32_MAX)
    {
        return 1;
    }
    else
    {
        uint32_t size = (uint32_t)(getHeaderSize() + getBodySize());
        return size;
    }
}

int Box::ReadBoxHeader(VSILFILE *fp, size_t *bytesRead, uint64_t *size)
{
    uint32_t sizeBigEndian = 0;

    if (VSIFReadL(&sizeBigEndian, sizeof(uint32_t), 1, fp) != 1)
    {
        return FALSE;
    }
    *size = CPL_MSBWORD32(sizeBigEndian);
    *bytesRead += sizeof(uint32_t);

    if (VSIFReadL(&boxtype, sizeof(uint32_t), 1, fp) != 1)
    {
        return FALSE;
    }
    *bytesRead += sizeof(uint32_t);

    if (*size == 1)
    {
        if (VSIFReadL(size, sizeof(uint64_t), 1, fp) != 1)
        {
            return FALSE;
        }
        *bytesRead += sizeof(uint64_t);
    }

    // TODO: add support for 0 size

    // TODO: add support for uuid box
    return TRUE;
}

uint32_t AbstractContainerBox::addChildBox(std::shared_ptr<Box> box)
{
    boxes.push_back(box);
    // Return 1 based index.
    return (uint32_t)boxes.size();
}

uint64_t AbstractContainerBox::getBodySize()
{
    uint64_t size = 0;
    for (size_t i = 0; i < boxes.size(); i++)
    {
        size += boxes[i]->getFullSize();
    }
    return size;
}

void AbstractContainerBox::writeBodyTo(VSILFILE *fp)
{
    for (size_t i = 0; i < boxes.size(); i++)
    {
        boxes[i]->writeTo(fp);
    }
}
