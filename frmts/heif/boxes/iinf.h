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

class ItemInfoEntry : public FullBox
{
  public:
    ItemInfoEntry(uint32_t id, const char *type)
        : FullBox("infe"), item_ID(id), item_protection_index(0),
          item_type(fourcc(type)), item_name("")
    {
        // TODO: be more sophisticated about this - probably alternative constructors
        version = 2;
    }

    ItemInfoEntry(uint32_t id, const char *type, const std::string &name)
        : FullBox("infe"), item_ID(id), item_protection_index(0),
          item_type(fourcc(type)), item_name(name)
    {
        // TODO: be more sophisticated about this
        version = 2;
    }

    ~ItemInfoEntry()
    {
    }

  protected:
    uint64_t getBodySize() override
    {
        // TODO: There are some more combinations we need for MIME and URI items
        return sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) +
               item_name.size() + 1;
    }

    void writeBodyTo(VSILFILE *fp) override
    {
        if (version >= 2)
        {
            if (version == 2)
            {
                writeUint16Value(fp, item_ID);
            }
            else if (version == 3)
            {
                writeUint32Value(fp, item_ID);
            }
            writeUint16Value(fp, item_protection_index);
            writeFourCC(fp, item_type);
            writeStringValue(fp, item_name.c_str());
        }
        // TODO: There are some more combinations we need for MIME and URI items
    }

  private:
    uint32_t item_ID;
    uint16_t item_protection_index;
    uint32_t item_type;
    std::string item_name;
};

class ItemInfoBox : public FullBox
{

  public:
    ItemInfoBox() : FullBox("iinf")
    {
    }

    ~ItemInfoBox()
    {
    }

    void addEntry(const std::shared_ptr<ItemInfoEntry> entry)
    {
        item_infos.push_back(entry);
    }

  protected:
    uint64_t getBodySize() override
    {
        uint64_t size = 0;
        uint32_t entry_count = (uint32_t)item_infos.size();
        if (entry_count > UINT16_MAX)
        {
            version = 1;
            size += sizeof(uint32_t);
        }
        else
        {
            {
                size += sizeof(uint16_t);
            }
        }
        for (size_t i = 0; i < entry_count; i++)
        {
            size += item_infos[i]->getFullSize();
        }
        return size;
    }

    void writeBodyTo(VSILFILE *fp) override
    {
        uint32_t entry_count = (uint32_t)item_infos.size();
        if (entry_count > UINT16_MAX)
        {
            writeUint32Value(fp, entry_count);
        }
        else
        {
            {
                writeUint16Value(fp, entry_count);
            }
        }
        for (size_t i = 0; i < entry_count; i++)
        {
            item_infos[i]->writeTo(fp);
        }
    }

  private:
    std::vector<std::shared_ptr<ItemInfoEntry>> item_infos;
};
