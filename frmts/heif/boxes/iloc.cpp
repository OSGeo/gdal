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

#include "iloc.h"
#include <cstddef>
#include <cstdint>
#include <memory>

uint64_t ItemLocationBox::getBodySize()
{
    uint64_t size = 0;
    size += 2;
    if (version < 2)
    {
        size += sizeof(uint16_t);
    }
    else
    {
        size += sizeof(uint32_t);
    }
    uint8_t offset_size = getOffsetSize();
    uint8_t length_size = getLengthSize();
    uint8_t base_offset_size = getBaseOffsetSize();
    uint8_t index_size_or_reserved = getIndexSizeOrReserved();
    for (size_t i = 0; i < items.size(); i++)
    {
        if (version < 2)
        {
            size += sizeof(uint16_t);
        }
        else
        {
            size += sizeof(uint32_t);
        }
        if ((version == 1) || (version == 2))
        {
            size += sizeof(uint16_t);
        }
        size += sizeof(uint16_t);
        size += base_offset_size;
        size += sizeof(uint16_t);  // extent_count size
        uint8_t extent_count = items[i]->getExtentCount();
        for (size_t j = 0; j < extent_count; j++)
        {
            if (((version == 1) || (version == 2)) &&
                (index_size_or_reserved > 0))
            {
                size += index_size_or_reserved;
            }
            size += offset_size;
            size += length_size;
        }
    }
    return size;
}

void ItemLocationBox::writeBodyTo(VSILFILE *fp)
{
    uint8_t offset_size = getOffsetSize();
    uint8_t length_size = getLengthSize();
    writeUint8Value(fp, (offset_size << 4 | length_size));
    uint8_t base_offset_size = getBaseOffsetSize();
    uint8_t index_size_or_reserved = getIndexSizeOrReserved();
    writeUint8Value(fp, (base_offset_size << 4 | index_size_or_reserved));
    if (version < 2)
    {
        writeUint16Value(fp, items.size());
    }
    else
    {
        writeUint32Value(fp, items.size());
    }
    for (size_t i = 0; i < items.size(); i++)
    {
        std::shared_ptr<Item> item = items[i];
        item->writeTo(fp, version, base_offset_size, index_size_or_reserved,
                      offset_size, length_size);
    }
}

uint64_t ItemLocationBox::Item::getBaseOffset() const
{
    // This is not optimal, but common.
    return 0;
}

uint64_t ItemLocationBox::Item::getGreatestExtentOffset() const
{
    uint64_t maxExtentOffset = 0;
    for (size_t i = 0; i < extents.size(); i++)
    {
        maxExtentOffset = std::max(maxExtentOffset, extents[i]->offset);
    }
    return maxExtentOffset;
}

uint64_t ItemLocationBox::Item::getGreatestExtentLength() const
{
    uint64_t maxExtentLength = 0;
    for (size_t i = 0; i < extents.size(); i++)
    {
        maxExtentLength = std::max(maxExtentLength, extents[i]->length);
    }
    return maxExtentLength;
}

uint8_t ItemLocationBox::getOffsetSize() const
{
    uint64_t greatestOffset = 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        std::shared_ptr<Item> item = items[i];
        uint64_t greatestExtentOffset = item->getGreatestExtentOffset();
        greatestOffset = std::max(greatestOffset, greatestExtentOffset);
    }
    if (greatestOffset > UINT32_MAX)
    {
        return 8;
    }
    else
    {
        return 4;
    }
}

uint8_t ItemLocationBox::getLengthSize() const
{
    uint64_t greatestLength = 0;
    for (size_t i = 0; i < items.size(); i++)
    {
        std::shared_ptr<Item> item = items[i];
        uint64_t greatestExtentLength = item->getGreatestExtentLength();
        greatestLength = std::max(greatestLength, greatestExtentLength);
    }
    if (greatestLength > UINT32_MAX)
    {
        return 8;
    }
    else
    {
        return 4;
    }
}

uint8_t ItemLocationBox::getBaseOffsetSize() const
{
    return 4;
}

uint8_t ItemLocationBox::getIndexSizeOrReserved() const
{
    // TODO: calculate based on version when needed
    return 0;
}
