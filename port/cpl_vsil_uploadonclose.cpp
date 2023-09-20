/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  File handler that uses a temporary file, and upload file on close
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_vsi_virtual.h"

#include <algorithm>
#include <vector>

/************************************************************************/
/*                        VSIUploadOnCloseHandle                        */
/************************************************************************/

class VSIUploadOnCloseHandle final : public VSIVirtualHandle
{
    VSIVirtualHandleUniquePtr m_poWritableHandle;
    std::string m_osTmpFilename;
    VSIVirtualHandleUniquePtr m_fpTemp;

    VSIUploadOnCloseHandle(const VSIUploadOnCloseHandle &) = delete;
    VSIUploadOnCloseHandle &operator=(const VSIUploadOnCloseHandle &) = delete;

  public:
    VSIUploadOnCloseHandle(VSIVirtualHandleUniquePtr &&poWritableHandle,
                           const std::string &osTmpFilename,
                           VSIVirtualHandleUniquePtr &&fpTemp)
        : m_poWritableHandle(std::move(poWritableHandle)),
          m_osTmpFilename(osTmpFilename), m_fpTemp(std::move(fpTemp))
    {
    }

    ~VSIUploadOnCloseHandle() override;

    int Seek(vsi_l_offset nOffset, int nWhence) override
    {
        return m_fpTemp->Seek(nOffset, nWhence);
    }

    vsi_l_offset Tell() override
    {
        return m_fpTemp->Tell();
    }

    size_t Read(void *pBuffer, size_t nSize, size_t nCount) override
    {
        return m_fpTemp->Read(pBuffer, nSize, nCount);
    }

    size_t Write(const void *pBuffer, size_t nSize, size_t nCount) override
    {
        return m_fpTemp->Write(pBuffer, nSize, nCount);
    }

    int Eof() override
    {
        return m_fpTemp->Eof();
    }

    int Flush() override
    {
        return m_fpTemp->Flush();
    }

    int Close() override;

    int Truncate(vsi_l_offset nNewSize) override
    {
        return m_fpTemp->Truncate(nNewSize);
    }

    VSIRangeStatus GetRangeStatus(vsi_l_offset nOffset,
                                  vsi_l_offset nLength) override
    {
        return m_fpTemp->GetRangeStatus(nOffset, nLength);
    }
};

/************************************************************************/
/*                      ~VSIUploadOnCloseHandle()                       */
/************************************************************************/

VSIUploadOnCloseHandle::~VSIUploadOnCloseHandle()
{
    VSIUploadOnCloseHandle::Close();
    if (!m_osTmpFilename.empty())
        VSIUnlink(m_osTmpFilename.c_str());
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIUploadOnCloseHandle::Close()
{
    if (m_fpTemp == nullptr)
        return -1;

    // Copy temporary files to m_poWritableHandle
    if (m_fpTemp->Seek(0, SEEK_END) != 0)
    {
        m_fpTemp.reset();
        return -1;
    }
    const auto nSize = m_fpTemp->Tell();
    m_fpTemp->Seek(0, SEEK_SET);
    constexpr size_t CHUNK_SIZE = 1024 * 1024;
    vsi_l_offset nOffset = 0;
    std::vector<GByte> abyBuffer(CHUNK_SIZE);
    while (nOffset < nSize)
    {
        size_t nToRead = static_cast<size_t>(
            std::min(nSize - nOffset, static_cast<vsi_l_offset>(CHUNK_SIZE)));
        if (m_fpTemp->Read(&abyBuffer[0], nToRead, 1) != 1 ||
            m_poWritableHandle->Write(&abyBuffer[0], nToRead, 1) != 1)
        {
            m_fpTemp.reset();
            return -1;
        }
        nOffset += nToRead;
    }
    m_fpTemp.reset();
    return m_poWritableHandle->Close();
}

/************************************************************************/
/*                    VSICreateUploadOnCloseFile()                      */
/************************************************************************/

VSIVirtualHandle *
VSICreateUploadOnCloseFile(VSIVirtualHandleUniquePtr &&poWritableHandle,
                           VSIVirtualHandleUniquePtr &&poTmpFile,
                           const std::string &osTmpFilename)
{
    const bool deleted = VSIUnlink(osTmpFilename.c_str()) == 0;
    return new VSIUploadOnCloseHandle(std::move(poWritableHandle),
                                      deleted ? std::string() : osTmpFilename,
                                      std::move(poTmpFile));
}
