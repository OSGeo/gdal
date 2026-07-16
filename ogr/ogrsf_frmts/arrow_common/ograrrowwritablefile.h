/******************************************************************************
 *
 * Project:  Arrow generic code
 * Purpose:  Arrow generic code
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ARROW_WRITABLE_FILE_H
#define OGR_ARROW_WRITABLE_FILE_H

#include "cpl_vsi_virtual.h"

#include "arrow/buffer.h"
#include "arrow/io/file.h"
#include "arrow/io/interfaces.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

/************************************************************************/
/*                         OGRArrowWritableFile                         */
/************************************************************************/

class OGRArrowWritableFile final : public arrow::io::OutputStream
{
    VSIVirtualHandleUniquePtr m_fp;

    OGRArrowWritableFile(const OGRArrowWritableFile &) = delete;
    OGRArrowWritableFile &operator=(const OGRArrowWritableFile &) = delete;

  public:
    explicit OGRArrowWritableFile(VSIVirtualHandleUniquePtr fp)
        : m_fp(std::move(fp))
    {
    }

    arrow::Status Close() override
    {
        int ret = m_fp->Close();
        m_fp.reset();
        return ret == 0 ? arrow::Status::OK()
                        : arrow::Status::IOError("Error while closing");
    }

    arrow::Result<int64_t> Tell() const override
    {
        return static_cast<int64_t>(m_fp->Tell());
    }

    bool closed() const override
    {
        return m_fp == nullptr;
    }

    arrow::Status Write(const void *data, int64_t nbytes) override
    {
        CPLAssert(static_cast<int64_t>(static_cast<size_t>(nbytes)) == nbytes);
        if (m_fp->Write(data, 1, static_cast<size_t>(nbytes)) ==
            static_cast<size_t>(nbytes))
            return arrow::Status::OK();
        return arrow::Status::IOError("Error while writing");
    }

    arrow::Status Write(const std::shared_ptr<arrow::Buffer> &data) override
    {
        return Write(data->data(), data->size());
    }
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif  // OGR_ARROW_WRITABLE_FILE_H
