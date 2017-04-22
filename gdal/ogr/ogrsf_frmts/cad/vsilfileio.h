/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 ******************************************************************************/
#ifndef VSILFILEIO_H
#define VSILFILEIO_H

#include "cadfileio.h"
#include "cpl_conv.h"
#include "cpl_string.h"

class VSILFileIO : public CADFileIO
{
public:
    explicit VSILFileIO(const char* pszFilePath);
    virtual ~VSILFileIO();
    virtual const char* ReadLine() override;
    virtual bool Eof() const override;
    virtual bool Open(int mode) override;
    virtual bool Close() override;
    virtual int Seek(long int offset, SeekOrigin origin) override;
    virtual long int Tell() override;
    virtual size_t Read(void* ptr, size_t size) override;
    virtual size_t Write(void* ptr, size_t size) override;
    virtual void Rewind() override;
protected:
    VSILFILE *m_oFileStream;
};

#endif // VSILFILEIO_H
