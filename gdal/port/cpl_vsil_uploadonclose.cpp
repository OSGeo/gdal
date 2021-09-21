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

class VSIUploadOnCloseHandle final: public VSIVirtualHandle {
    VSIVirtualHandle* m_poBaseHandle;
    CPLString m_osTmpFilename;
    VSILFILE* m_fpTemp;

    VSIUploadOnCloseHandle(const VSIUploadOnCloseHandle&) = delete;
    VSIUploadOnCloseHandle& operator= (const VSIUploadOnCloseHandle&) = delete;

  public:
    VSIUploadOnCloseHandle( VSIVirtualHandle* poBaseHandle,
                            const CPLString& osTmpFilename,
                            VSILFILE* fpTemp ):
        m_poBaseHandle(poBaseHandle),
        m_osTmpFilename(osTmpFilename),
        m_fpTemp(fpTemp)
    {}

    ~VSIUploadOnCloseHandle() override;

    int       Seek( vsi_l_offset nOffset, int nWhence ) override {
        return VSIFSeekL(m_fpTemp, nOffset, nWhence);
    }

    vsi_l_offset Tell() override {
        return VSIFTellL(m_fpTemp);
    }

    size_t    Read( void *pBuffer, size_t nSize, size_t nCount ) override {
        return VSIFReadL(pBuffer, nSize, nCount, m_fpTemp);
    }

    size_t    Write( const void *pBuffer, size_t nSize,size_t nCount) override {
        return VSIFWriteL(pBuffer, nSize, nCount, m_fpTemp);
    }

    int       Eof() override { return VSIFEofL(m_fpTemp); }

    int       Flush() override { return VSIFFlushL(m_fpTemp); }

    int       Close() override;

    int       Truncate( vsi_l_offset nNewSize ) override { return VSIFTruncateL(m_fpTemp, nNewSize); }

    VSIRangeStatus GetRangeStatus( vsi_l_offset nOffset, vsi_l_offset nLength ) override {
        return VSIFGetRangeStatusL(m_fpTemp, nOffset, nLength);
    }
};

/************************************************************************/
/*                      ~VSIUploadOnCloseHandle()                       */
/************************************************************************/

VSIUploadOnCloseHandle::~VSIUploadOnCloseHandle()
{
    VSIUploadOnCloseHandle::Close();
    if( m_fpTemp )
        VSIFCloseL(m_fpTemp);
    if( !m_osTmpFilename.empty() )
        VSIUnlink(m_osTmpFilename);
    delete m_poBaseHandle;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIUploadOnCloseHandle::Close()
{
    if( m_fpTemp == nullptr )
        return -1;

    // Copy temporary files to m_poBaseHandle
    if( VSIFSeekL(m_fpTemp, 0, SEEK_END) != 0 )
    {
        VSIFCloseL(m_fpTemp);
        m_fpTemp = nullptr;
        return -1;
    }
    const auto nSize = VSIFTellL(m_fpTemp);
    VSIFSeekL(m_fpTemp, 0, SEEK_SET);
    constexpr size_t CHUNK_SIZE = 1024 * 1024;
    vsi_l_offset nOffset = 0;
    std::vector<GByte> abyBuffer(CHUNK_SIZE);
    while( nOffset < nSize )
    {
        size_t nToRead = static_cast<size_t>(
            std::min(nSize - nOffset, static_cast<vsi_l_offset>(CHUNK_SIZE)));
        if( VSIFReadL(&abyBuffer[0], nToRead, 1, m_fpTemp) != 1 ||
            m_poBaseHandle->Write(&abyBuffer[0], nToRead, 1) != 1 )
        {
            VSIFCloseL(m_fpTemp);
            m_fpTemp = nullptr;
            return -1;
        }
        nOffset += nToRead;
    }
    VSIFCloseL(m_fpTemp);
    m_fpTemp = nullptr;
    return m_poBaseHandle->Close();
}

/************************************************************************/
/*                    VSICreateUploadOnCloseFile()                      */
/************************************************************************/

VSIVirtualHandle *VSICreateUploadOnCloseFile( VSIVirtualHandle* poBaseHandle )
{
    CPLString osTmpFilename(CPLGenerateTempFilename(nullptr));
    VSILFILE* fpTemp = VSIFOpenL(osTmpFilename, "wb+");
    if( fpTemp == nullptr )
        return nullptr;
    const bool deleted = VSIUnlink(osTmpFilename) == 0;
    return new VSIUploadOnCloseHandle(
        poBaseHandle, deleted ? CPLString(): osTmpFilename, fpTemp );
}
