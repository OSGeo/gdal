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

#ifndef INCLUDE_HEIF_BOX_DEFINED
#define INCLUDE_HEIF_BOX_DEFINED

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gdal_priv.h"

static uint32_t fourcc(const char *pszType)
{
    uint32_t fourCC;
    CPLAssert(strlen(pszType) == 4);
    memcpy(&fourCC, pszType, 4);
    return fourCC;
}

class Box
{
  public:
    virtual void writeTo(VSILFILE *fp)
    {
        writeHeader(fp);
        writeBodyTo(fp);
    }

    virtual ~Box() = default;

    virtual uint64_t getFullSize();

    virtual uint32_t getHeaderSize()
    {
        return 8;
    }

  protected:
    Box(const char *fourCC) : boxtype(fourcc(fourCC))
    {
    }

    int ReadBoxHeader(VSILFILE *fp, size_t *bytesRead, uint64_t *size);

    virtual void writeBodyTo(VSILFILE *fp) = 0;

    virtual uint64_t getBodySize() = 0;

    virtual void writeHeader(VSILFILE *fp);

    void writeBoxType(VSILFILE *fp);

    static void writeFourCC(VSILFILE *fp, uint32_t fourCC)
    {
        VSIFWriteL(&fourCC, sizeof(uint32_t), 1, fp);
    }

    static void writeUint8Value(VSILFILE *fp, uint8_t value)
    {
        VSIFWriteL(&value, sizeof(uint8_t), 1, fp);
    }

    static void writeUint16Value(VSILFILE *fp, uint16_t value)
    {
        CPL_MSBPTR16(&value);
        VSIFWriteL(&value, sizeof(uint16_t), 1, fp);
    }

    static void writeUint32Value(VSILFILE *fp, uint32_t value)
    {
        CPL_MSBPTR32(&value);
        VSIFWriteL(&value, sizeof(uint32_t), 1, fp);
    }

    static void writeUint64Value(VSILFILE *fp, uint64_t value)
    {
        CPL_MSBPTR64(&value);
        VSIFWriteL(&value, sizeof(uint64_t), 1, fp);
    }

    static void writeStringValue(VSILFILE *fp, const char *value)
    {
        VSIFWriteL(value, sizeof(uint8_t), strlen(value) + 1, fp);
    }

    static void writeBytes(VSILFILE *fp,
                           std::shared_ptr<std::vector<uint8_t>> values)
    {
        VSIFWriteL(values->data(), sizeof(uint8_t), values->size(), fp);
    }

  private:
    uint32_t boxtype;
    uint64_t getSize();
};

class AbstractContainerBox : public Box
{
  public:
    uint32_t addChildBox(std::shared_ptr<Box> box);

  protected:
    explicit AbstractContainerBox(const char *fourCC) : Box(fourCC)
    {
    }

    virtual void writeBodyTo(VSILFILE *fp) override;

    virtual uint64_t getBodySize() override;

  private:
    std::vector<std::shared_ptr<Box>> boxes;
};

#endif
