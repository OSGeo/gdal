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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gdal_priv.h"

#include "fullbox.h"

class ItemLocationBox : public FullBox
{
  public:
    ItemLocationBox() : FullBox("iloc")
    {
    }

    ~ItemLocationBox()
    {
    }

    class Item
    {
      public:
        explicit Item(uint32_t id)
            : item_ID(id), construction_method(0), data_reference_index(0)
        {
        }

        ~Item()
        {
        }

        struct Extent
        {
            uint64_t index;
            uint64_t offset;
            uint64_t length;
        };

        void addExtent(std::shared_ptr<Extent> extent)
        {
            extents.push_back(extent);
        }

        uint16_t getExtentCount() const
        {
            return extents.size();
        }

        uint64_t getGreatestExtentOffset() const;

        uint64_t getGreatestExtentLength() const;

        void writeTo(VSILFILE *fp, uint8_t version, uint8_t base_offset_size,
                     uint8_t index_size, uint8_t offset_size,
                     uint8_t length_size)
        {
            if (version < 2)
            {
                writeUint16Value(fp, item_ID);
            }
            else
            {
                writeUint32Value(fp, item_ID);
            }
            if ((version == 1) || (version == 2))
            {
                writeUint16Value(fp, (construction_method & 0x0F));
            }
            writeUint16Value(fp, data_reference_index);
            if (base_offset_size == 4)
            {
                writeUint32Value(fp, (uint32_t)getBaseOffset());
            }
            else if (base_offset_size == 8)
            {
                writeUint64Value(fp, getBaseOffset());
            }
            uint16_t extent_count = extents.size();
            writeUint16Value(fp, extent_count);
            for (int j = 0; j < extent_count; j++)
            {
                std::shared_ptr<Extent> extent = extents[j];
                if (((version == 1) || (version == 2)) && (index_size > 0))
                {
                    if (index_size == 4)
                    {
                        writeUint32Value(fp, extent->index);
                    }
                    else if (index_size == 8)
                    {
                        writeUint64Value(fp, extent->index);
                    }
                }
                if (offset_size == 4)
                {
                    writeUint32Value(
                        fp, (uint32_t)(extent->offset - getBaseOffset()));
                }
                else if (offset_size == 8)
                {
                    writeUint64Value(fp, extent->offset - getBaseOffset());
                }
                if (length_size == 4)
                {
                    writeUint32Value(fp, (uint32_t)extent->length);
                }
                else if (length_size == 8)
                {
                    writeUint64Value(fp, extent->length);
                }
            }
        }

      private:
        static uint64_t getBaseOffset();
        uint32_t item_ID;
        uint8_t construction_method;
        uint16_t data_reference_index;
        std::vector<std::shared_ptr<Extent>> extents;
    };

    void addItem(std::shared_ptr<Item> item)
    {
        items.push_back(item);
    }

  protected:
    uint64_t getBodySize() override;
    void writeBodyTo(VSILFILE *fp) override;

  private:
    uint8_t getOffsetSize() const;
    uint8_t getLengthSize() const;
    static uint8_t getBaseOffsetSize();
    static uint8_t getIndexSizeOrReserved();

    std::vector<std::shared_ptr<Item>> items;
};
