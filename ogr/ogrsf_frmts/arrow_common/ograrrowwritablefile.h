/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#ifndef OGR_ARROW_WRITABLE_FILE_H
#define OGR_ARROW_WRITABLE_FILE_H

#include "cpl_vsi.h"

#include "arrow/buffer.h"
#include "arrow/io/file.h"
#include "arrow/io/interfaces.h"

/************************************************************************/
/*                        OGRArrowWritableFile                          */
/************************************************************************/

class OGRArrowWritableFile final: public arrow::io::OutputStream
{
    VSILFILE*   m_fp;

    OGRArrowWritableFile(const OGRArrowWritableFile&) = delete;
    OGRArrowWritableFile& operator= (const OGRArrowWritableFile&) = delete;

public:
    explicit OGRArrowWritableFile(VSILFILE* fp): m_fp(fp)
    {
    }

    ~OGRArrowWritableFile() override
    {
        if( m_fp )
            VSIFCloseL(m_fp);
    }

    arrow::Status Close() override
    {
        int ret = VSIFCloseL(m_fp);
        m_fp = nullptr;
        return ret == 0 ? arrow::Status::OK() : arrow::Status::IOError("Error while closing");
    }

    arrow::Result<int64_t> Tell() const override
    {
        return static_cast<int64_t>(VSIFTellL(m_fp));
    }

    bool closed() const override
    {
        return m_fp == nullptr;
    }

    arrow::Status Write(const void* data, int64_t nbytes) override
    {
        CPLAssert(static_cast<int64_t>(static_cast<size_t>(nbytes)) == nbytes);
        if( VSIFWriteL(data, 1, static_cast<size_t>(nbytes), m_fp) == static_cast<size_t>(nbytes) )
            return arrow::Status::OK();
        return arrow::Status::IOError("Error while writing");
    }

    arrow::Status Write(const std::shared_ptr<arrow::Buffer>& data) override
    {
        return Write(data->data(), data->size());
    }
};


#endif // OGR_ARROW_WRITABLE_FILE_H
