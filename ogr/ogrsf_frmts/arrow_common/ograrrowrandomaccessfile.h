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

#ifndef OGR_ARROW_RANDOM_ACCESS_FILE_H
#define OGR_ARROW_RANDOM_ACCESS_FILE_H

#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

#include "arrow/buffer.h"
#include "arrow/io/file.h"
#include "arrow/io/interfaces.h"

#include <atomic>
#include <cinttypes>

/************************************************************************/
/*                        OGRArrowRandomAccessFile                      */
/************************************************************************/

class OGRArrowRandomAccessFile final : public arrow::io::RandomAccessFile
{
    int64_t m_nSize = -1;
    const std::string m_osFilename;
    VSILFILE *m_fp;
    const bool m_bOwnFP;
    std::atomic<bool> m_bAskedToClosed = false;

#ifdef OGR_ARROW_USE_PREAD
    const bool m_bDebugReadAt;
    const bool m_bUsePRead;
#endif

    OGRArrowRandomAccessFile(const OGRArrowRandomAccessFile &) = delete;
    OGRArrowRandomAccessFile &
    operator=(const OGRArrowRandomAccessFile &) = delete;

  public:
    OGRArrowRandomAccessFile(const std::string &osFilename, VSILFILE *fp,
                             bool bOwnFP)
        : m_osFilename(osFilename), m_fp(fp), m_bOwnFP(bOwnFP)
#ifdef OGR_ARROW_USE_PREAD
          ,
          m_bDebugReadAt(!VSIIsLocal(m_osFilename.c_str())),
          // Due to the lack of caching for current /vsicurl PRead(), do not
          // use the PRead() implementation on those files
          m_bUsePRead(m_fp->HasPRead() &&
                      CPLTestBool(CPLGetConfigOption(
                          "OGR_ARROW_USE_PREAD",
                          VSIIsLocal(m_osFilename.c_str()) ? "YES" : "NO")))
#endif
    {
    }

    OGRArrowRandomAccessFile(const std::string &osFilename,
                             VSIVirtualHandleUniquePtr &&fp)
        : m_osFilename(osFilename), m_fp(fp.release()), m_bOwnFP(true)
#ifdef OGR_ARROW_USE_PREAD
          ,
          m_bDebugReadAt(!VSIIsLocal(m_osFilename.c_str())),
          // Due to the lack of caching for current /vsicurl PRead(), do not
          // use the PRead() implementation on those files
          m_bUsePRead(m_fp->HasPRead() &&
                      CPLTestBool(CPLGetConfigOption(
                          "OGR_ARROW_USE_PREAD",
                          VSIIsLocal(m_osFilename.c_str()) ? "YES" : "NO")))
#endif
    {
    }

    void AskToClose()
    {
        m_bAskedToClosed = true;
        if (m_fp)
            m_fp->Interrupt();
    }

    ~OGRArrowRandomAccessFile() override
    {
        if (m_fp && m_bOwnFP)
            VSIFCloseL(m_fp);
    }

    arrow::Status Close() override
    {
        if (!m_bOwnFP)
            return arrow::Status::IOError(
                "Cannot close a file that we don't own");
        int ret = VSIFCloseL(m_fp);
        m_fp = nullptr;
        return ret == 0 ? arrow::Status::OK()
                        : arrow::Status::IOError("Error while closing");
    }

    arrow::Result<int64_t> Tell() const override
    {
        return static_cast<int64_t>(VSIFTellL(m_fp));
    }

    bool closed() const override
    {
        return m_bAskedToClosed || m_fp == nullptr;
    }

    arrow::Status Seek(int64_t position) override
    {
        if (m_bAskedToClosed)
            return arrow::Status::IOError("File requested to close");

        if (VSIFSeekL(m_fp, static_cast<vsi_l_offset>(position), SEEK_SET) == 0)
            return arrow::Status::OK();
        return arrow::Status::IOError("Error while seeking");
    }

    arrow::Result<int64_t> Read(int64_t nbytes, void *out) override
    {
        if (m_bAskedToClosed)
            return arrow::Status::IOError("File requested to close");

        CPLAssert(static_cast<int64_t>(static_cast<size_t>(nbytes)) == nbytes);
        return static_cast<int64_t>(
            VSIFReadL(out, 1, static_cast<size_t>(nbytes), m_fp));
    }

    arrow::Result<std::shared_ptr<arrow::Buffer>> Read(int64_t nbytes) override
    {
        if (m_bAskedToClosed)
            return arrow::Status::IOError("File requested to close");

        // CPLDebug("ARROW", "Reading %d bytes", int(nbytes));
        auto buffer = arrow::AllocateResizableBuffer(nbytes);
        if (!buffer.ok())
        {
            return buffer;
        }
        uint8_t *buffer_data = (*buffer)->mutable_data();
        auto nread = Read(nbytes, buffer_data);
        CPL_IGNORE_RET_VAL(
            (*buffer)->Resize(*nread));  // shrink --> cannot fail
        return buffer;
    }

#ifdef OGR_ARROW_USE_PREAD
    using arrow::io::RandomAccessFile::ReadAt;

    arrow::Result<std::shared_ptr<arrow::Buffer>>
    ReadAt(int64_t position, int64_t nbytes) override
    {
        if (m_bAskedToClosed)
            return arrow::Status::IOError("File requested to close");

        if (m_bUsePRead)
        {
            auto buffer = arrow::AllocateResizableBuffer(nbytes);
            if (!buffer.ok())
            {
                return buffer;
            }
            if (m_bDebugReadAt)
            {
                CPLDebug(
                    "ARROW",
                    "Start ReadAt() called on %s (this=%p) from "
                    "thread=" CPL_FRMT_GIB ": pos=%" PRId64 ", nbytes=%" PRId64,
                    m_osFilename.c_str(), this, CPLGetPID(), position, nbytes);
            }
            uint8_t *buffer_data = (*buffer)->mutable_data();
            auto nread = m_fp->PRead(buffer_data, static_cast<size_t>(nbytes),
                                     static_cast<vsi_l_offset>(position));
            CPL_IGNORE_RET_VAL(
                (*buffer)->Resize(nread));  // shrink --> cannot fail
            if (m_bDebugReadAt)
            {
                CPLDebug(
                    "ARROW",
                    "End ReadAt() called on %s (this=%p) from "
                    "thread=" CPL_FRMT_GIB ": pos=%" PRId64 ", nbytes=%" PRId64,
                    m_osFilename.c_str(), this, CPLGetPID(), position, nbytes);
            }
            return buffer;
        }
        return arrow::io::RandomAccessFile::ReadAt(position, nbytes);
    }
#endif

    arrow::Result<int64_t> GetSize() override
    {
        if (m_bAskedToClosed)
            return arrow::Status::IOError("File requested to close");

        if (m_nSize < 0)
        {
            const auto nPos = VSIFTellL(m_fp);
            VSIFSeekL(m_fp, 0, SEEK_END);
            m_nSize = static_cast<int64_t>(VSIFTellL(m_fp));
            VSIFSeekL(m_fp, nPos, SEEK_SET);
        }
        return m_nSize;
    }
};

#endif  // OGR_ARROW_RANDOM_ACCESS_FILE_H
