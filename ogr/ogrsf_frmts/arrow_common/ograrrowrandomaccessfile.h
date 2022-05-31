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

#ifndef OGR_ARROW_RANDOM_ACCESS_FILE_H
#define OGR_ARROW_RANDOM_ACCESS_FILE_H

#include "cpl_vsi.h"

#include "arrow/buffer.h"
#include "arrow/io/file.h"
#include "arrow/io/interfaces.h"

/************************************************************************/
/*                        OGRArrowRandomAccessFile                      */
/************************************************************************/

class OGRArrowRandomAccessFile final: public arrow::io::RandomAccessFile
{
    int64_t     m_nSize = -1;
    VSILFILE*   m_fp;
    bool        m_bOwnFP;

    OGRArrowRandomAccessFile(const OGRArrowRandomAccessFile&) = delete;
    OGRArrowRandomAccessFile& operator= (const OGRArrowRandomAccessFile&) = delete;

public:
    explicit OGRArrowRandomAccessFile(VSILFILE* fp, bool bOwnFP = true): m_fp(fp), m_bOwnFP(bOwnFP)
    {
    }

    ~OGRArrowRandomAccessFile() override
    {
        if( m_fp  && m_bOwnFP )
            VSIFCloseL(m_fp);
    }

    arrow::Status Close() override
    {
        if( !m_bOwnFP )
            return arrow::Status::IOError("Cannot close a file that we don't own");
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

    arrow::Status Seek(int64_t position) override
    {
        if( VSIFSeekL(m_fp, static_cast<vsi_l_offset>(position), SEEK_SET) == 0 )
            return arrow::Status::OK();
        return arrow::Status::IOError("Error while seeking");
    }

    arrow::Result<int64_t> Read(int64_t nbytes, void* out) override
    {
        CPLAssert(static_cast<int64_t>(static_cast<size_t>(nbytes)) == nbytes);
        return static_cast<int64_t>(
            VSIFReadL(out, 1, static_cast<size_t>(nbytes), m_fp));
    }

    arrow::Result<std::shared_ptr<arrow::Buffer>> Read(int64_t nbytes) override
    {
        auto buffer = arrow::AllocateResizableBuffer(nbytes);
        if (!buffer.ok())
        {
            return buffer;
        }
        uint8_t* buffer_data = (*buffer)->mutable_data();
        auto nread = Read(nbytes, buffer_data);
        CPL_IGNORE_RET_VAL((*buffer)->Resize(*nread)); // shrink --> cannot fail
        return buffer;
    }

    arrow::Result<int64_t> GetSize() override
    {
        if( m_nSize < 0 )
        {
            const auto nPos = VSIFTellL(m_fp);
            VSIFSeekL(m_fp, 0, SEEK_END);
            m_nSize = static_cast<int64_t>(VSIFTellL(m_fp));
            VSIFSeekL(m_fp, nPos, SEEK_SET);
        }
        return m_nSize;
    }
};

#endif // OGR_ARROW_RANDOM_ACCESS_FILE_H
