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

class FileTypeBox : public Box
{
  public:
    FileTypeBox() : Box("ftyp"), minorVersion(0)
    {
    }

    uint32_t getMajorBrand() const
    {
        return majorBrand;
    }

    void setMajorBrand(uint32_t brand)
    {
        majorBrand = brand;
    }

    void addCompatibleBrand(uint32_t brand)
    {
        compatibleBrands.push_back(brand);
    }

    bool hasCompatibleBrand(uint32_t brand)
    {
        return (std::find(compatibleBrands.begin(), compatibleBrands.end(),
                          brand) != compatibleBrands.end());
    }

    ~FileTypeBox()
    {
    }

    int ReadBox(VSILFILE *fp)
    {
        size_t bytesRead = 0;
        uint64_t size = 0;
        int errCode = ReadBoxHeader(fp, &bytesRead, &size);
        if (errCode != TRUE)
        {
            return errCode;
        }
        if (VSIFReadL(&majorBrand, sizeof(uint32_t), 1, fp) != 1)
        {
            return FALSE;
        }
        bytesRead += sizeof(uint32_t);

        if (VSIFReadL(&minorVersion, sizeof(uint32_t), 1, fp) != 1)
        {
            return FALSE;
        }
        minorVersion = CPL_MSBWORD32(minorVersion);
        bytesRead += sizeof(uint32_t);
        size_t bytesRemaining = size - bytesRead;
        for (size_t i = 0; i < bytesRemaining / sizeof(uint32_t); i++)
        {
            uint32_t brand;
            if (VSIFReadL(&brand, sizeof(uint32_t), 1, fp) != 1)
            {
                return FALSE;
            }
            compatibleBrands.push_back(brand);
        }

        return TRUE;
    }

  protected:
    uint64_t getBodySize() override
    {
        return 8 + sizeof(uint32_t) * compatibleBrands.size();
        //  + sizeof(uint32_t);
    }

    void writeBodyTo(VSILFILE *fp) override
    {
        writeFourCC(fp, majorBrand);
        writeUint32Value(fp, minorVersion);
        for (size_t i = 0; i < compatibleBrands.size(); i++)
        {
            writeFourCC(fp, compatibleBrands[i]);
        }
        // Not clear if this is required or not, include to be safe
        // writeFourCC(fp, majorBrand);
    }

  private:
    uint32_t majorBrand;
    uint32_t minorVersion;
    std::vector<uint32_t> compatibleBrands;
};
