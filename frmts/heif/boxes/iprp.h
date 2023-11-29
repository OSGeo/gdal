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
#include "fullbox.h"
#include <cstdint>
#include <memory>

class ItemPropertiesBox : public AbstractContainerBox
{
  public:
    ItemPropertiesBox() : AbstractContainerBox("iprp")
    {
    }

    ~ItemPropertiesBox()
    {
    }

    // TODO: implement this
    // void addAssociation(std::shared_ptr<Box> property, uint32_t itemId);
};

class ItemPropertyContainerBox : public AbstractContainerBox
{
  public:
    ItemPropertyContainerBox() : AbstractContainerBox("ipco")
    {
    }

    ~ItemPropertyContainerBox()
    {
    }
};

class ItemPropertyAssociationBox : public FullBox
{
  public:
    ItemPropertyAssociationBox() : FullBox("ipma")
    {
    }

    ~ItemPropertyAssociationBox()
    {
    }

    class Entry
    {
      public:
        Entry(uint32_t id) : item_ID(id)
        {
        }

        struct Association
        {
            bool essential;
            uint16_t property_index;
        };

        void addAssociation(Association association)
        {
            associations.push_back(association);
        }

        uint32_t getItemID() const;

        uint8_t getAssociationCount() const
        {
            return associations.size();
        }

        uint8_t getAssociationAsUint8(unsigned int index) const;

        uint16_t getAssociationAsUint16(unsigned int index) const;

      private:
        uint32_t item_ID;
        std::vector<Association> associations;
    };

    void addEntry(Entry entry)
    {
        entries.push_back(entry);
    }

  protected:
    uint64_t getBodySize() override;

    void writeBodyTo(VSILFILE *fp) override;

  private:
    std::vector<Entry> entries;
};
