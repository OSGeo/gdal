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

#include "iprp.h"
#include <cstddef>
#include <cstdint>

uint64_t ItemPropertyAssociationBox::getBodySize()
{
    uint64_t size = 0;
    size += sizeof(uint32_t);
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (version < 1)
        {
            size += sizeof(uint16_t);
        }
        else
        {
            size += sizeof(uint32_t);
        }
        size += sizeof(uint8_t);
        uint8_t associationCount = entries[i].getAssociationCount();
        if (getFlags(0) & 0x01)
        {
            size += (associationCount * sizeof(uint16_t));
        }
        else
        {
            size += (associationCount * sizeof(uint8_t));
        }
    }
    return size;
}

void ItemPropertyAssociationBox::writeBodyTo(VSILFILE *fp)
{
    writeUint32Value(fp, entries.size());
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (version < 1)
        {
            writeUint16Value(fp, entries[i].getItemID());
        }
        else
        {
            writeUint32Value(fp, entries[i].getItemID());
        }
        writeUint8Value(fp, entries[i].getAssociationCount());
        for (size_t j = 0; j < entries[i].getAssociationCount(); j++)
        {
            if (getFlags(0) & 0x01)
            {
                uint16_t assoc = entries[i].getAssociationAsUint16(j);
                writeUint16Value(fp, assoc);
            }
            else
            {
                uint8_t assoc = entries[i].getAssociationAsUint8(j);
                writeUint8Value(fp, assoc);
            }
        }
    }
}

uint32_t ItemPropertyAssociationBox::Entry::getItemID() const
{
    return item_ID;
}

uint16_t ItemPropertyAssociationBox::Entry::getAssociationAsUint16(
    unsigned int index) const
{
    Association association = associations[index];
    uint16_t v = association.property_index & 0x7fff;
    if (association.essential)
    {
        v |= 0x8000;
    }
    return v;
}

uint8_t ItemPropertyAssociationBox::Entry::getAssociationAsUint8(
    unsigned int index) const
{
    Association association = associations[index];
    uint8_t v = association.property_index & 0x7f;
    if (association.essential)
    {
        v |= 0x80;
    }
    return v;
}