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

#include "cmpd.h"
#include <cstdint>

uint64_t ComponentDefinitionBox::getBodySize()
{
    uint64_t size = 0;
    size += sizeof(uint32_t);
    for (size_t i = 0; i < components.size(); i++)
    {
        size += sizeof(uint16_t);
        if (components[i].component_type >= 0x8000)
        {
            size += components[i].component_type_uri.size();
            size += 1;
        }
    }
    return size;
}

void ComponentDefinitionBox::writeBodyTo(VSILFILE *fp)
{
    writeUint32Value(fp, components.size());
    for (size_t i = 0; i < components.size(); i++)
    {
        writeUint16Value(fp, components[i].component_type);
        if (components[i].component_type >= 0x8000)
        {
            writeStringValue(fp, components[i].component_type_uri.c_str());
        }
    }
}

void ComponentDefinitionBox::addComponent(uint16_t component_type)
{
    ComponentDefinitionBox::Component component;
    component.component_type = component_type;
    components.push_back(component);
}
