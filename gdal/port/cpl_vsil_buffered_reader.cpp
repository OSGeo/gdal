/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implementation of buffered reader IO functions.
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

/* The intent of this class is to be a wrapper around an underlying virtual */
/* handle and add very basic caching of last read bytes, so that a backward */
/* seek of a few bytes doesn't require a seek on the underlying virtual handle. */
/* This enable us to improve dramatically the performance of CPLReadLine2L() on */
/* a gzip file */

#include "cpl_vsi_virtual.h"

#include "cpl_port.h"

#define MAX_BUFFER_SIZE 65536

CPL_CVSID("$Id$");

class VSIBufferedReaderHandle : public VSIVirtualHandle
{
    VSIVirtualHandle* poBaseHandle;
    char              pabyBuffer[MAX_BUFFER_SIZE];
    GUIntBig          nBufferOffset;
    int               nBufferSize;
    GUIntBig          nCurOffset;
    int               bNeedBaseHandleSeek;
    int               bEOF;

  public:

    VSIBufferedReaderHandle(VSIVirtualHandle* poBaseHandle);
    ~VSIBufferedReaderHandle();

    virtual int       Seek( vsi_l_offset nOffset, int nWhence );
    virtual vsi_l_offset Tell();
    virtual size_t    Read( void *pBuffer, size_t nSize, size_t nMemb );
    virtual size_t    Write( const void *pBuffer, size_t nSize, size_t nMemb );
    virtual int       Eof();
    virtual int       Flush();
    virtual int       Close();
};

/************************************************************************/
/*                    VSICreateBufferedReaderHandle()                   */
/************************************************************************/

VSIVirtualHandle* VSICreateBufferedReaderHandle(VSIVirtualHandle* poBaseHandle)
{
    return new VSIBufferedReaderHandle(poBaseHandle);
}

/************************************************************************/
/*                        VSIBufferedReaderHandle()                     */
/************************************************************************/

VSIBufferedReaderHandle::VSIBufferedReaderHandle(VSIVirtualHandle* poBaseHandle)
{
    this->poBaseHandle = poBaseHandle;
    nBufferOffset = 0;
    nBufferSize = 0;
    nCurOffset = 0;
    bNeedBaseHandleSeek = FALSE;
    bEOF = FALSE;
}

/************************************************************************/
/*                        ~VSIBufferedReaderHandle()                    */
/************************************************************************/

VSIBufferedReaderHandle::~VSIBufferedReaderHandle()
{
    delete poBaseHandle;
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Seek( vsi_l_offset nOffset, int nWhence )
{
    //CPLDebug( "BUFFERED", "Seek(%d,%d)", (int)nOffset, (int)nWhence);
    bEOF = FALSE;
    if (nWhence == SEEK_CUR)
        nCurOffset += nOffset;
    else if (nWhence == SEEK_END)
    {
        poBaseHandle->Seek(nOffset, nWhence);
        nCurOffset = poBaseHandle->Tell();
        bNeedBaseHandleSeek = TRUE;
    }
    else
        nCurOffset = nOffset;

    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIBufferedReaderHandle::Tell()
{
    //CPLDebug( "BUFFERED", "Tell() = %d", (int)nCurOffset);
    return nCurOffset;
}
/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIBufferedReaderHandle::Read( void *pBuffer, size_t nSize, size_t nMemb )
{
    const size_t nTotalToRead = nSize * nMemb;
    //CPLDebug( "BUFFERED", "Read(%d)", (int)nTotalToRead);

    if (nSize == 0)
        return 0;

    if (nBufferSize != 0 &&
        nCurOffset >= nBufferOffset && nCurOffset <= nBufferOffset + nBufferSize)
    {
        /* We try to read from an offset located within the buffer */
        const int nReadInBuffer = (int) MIN(nTotalToRead, nBufferOffset + nBufferSize - nCurOffset);
        memcpy(pBuffer, pabyBuffer + nCurOffset - nBufferOffset, nReadInBuffer);
        const int nToReadInFile = nTotalToRead - nReadInBuffer;
        if (nToReadInFile > 0)
        {
            /* The beginning of the the data to read is located in the buffer */
            /* but the end must be read from the file */
            if (bNeedBaseHandleSeek)
                poBaseHandle->Seek(nBufferOffset + nBufferSize, SEEK_SET);
            bNeedBaseHandleSeek = FALSE;
            //CPLAssert(poBaseHandle->Tell() == nBufferOffset + nBufferSize);

            const int nReadInFile = poBaseHandle->Read((GByte*)pBuffer + nReadInBuffer, 1, nToReadInFile);
            const int nRead = nReadInBuffer + nReadInFile;

            nBufferSize = MIN(nRead, MAX_BUFFER_SIZE);
            nBufferOffset = nCurOffset + nRead - nBufferSize;
            memcpy(pabyBuffer, (GByte*)pBuffer + nRead - nBufferSize, nBufferSize);

            nCurOffset += nRead;
            //CPLAssert(poBaseHandle->Tell() == nBufferOffset + nBufferSize);
            //CPLAssert(poBaseHandle->Tell() == nCurOffset);

            bEOF = poBaseHandle->Eof();

            return nRead / nSize;
        }
        else
        {
            /* The data to read is completely located within the buffer */
            nCurOffset += nTotalToRead;
            return nTotalToRead / nSize;
        }
    }
    else
    {
        /* We try either to read before or after the buffer, so a seek is necessary */
        poBaseHandle->Seek(nCurOffset, SEEK_SET);
        bNeedBaseHandleSeek = FALSE;
        const int nReadInFile = poBaseHandle->Read(pBuffer, 1, nTotalToRead);
        nBufferSize = MIN(nReadInFile, MAX_BUFFER_SIZE);
        nBufferOffset = nCurOffset + nReadInFile - nBufferSize;
        memcpy(pabyBuffer, (GByte*)pBuffer + nReadInFile - nBufferSize, nBufferSize);

        nCurOffset += nReadInFile;
        //CPLAssert(poBaseHandle->Tell() == nBufferOffset + nBufferSize);
        //CPLAssert(poBaseHandle->Tell() == nCurOffset);

        bEOF = poBaseHandle->Eof();

        return nReadInFile / nSize;
    }

}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

size_t VSIBufferedReaderHandle::Write( CPL_UNUSED const void *pBuffer,
                                       CPL_UNUSED size_t nSize,
                                       CPL_UNUSED size_t nMemb )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "VSIFWriteL is not supported on buffer reader streams\n");
    return 0;
}


/************************************************************************/
/*                               Eof()                                  */
/************************************************************************/

int VSIBufferedReaderHandle::Eof()
{
    return bEOF;
}

/************************************************************************/
/*                              Flush()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Flush()
{
    return 0;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

int VSIBufferedReaderHandle::Close()
{
    if (poBaseHandle)
    {
        poBaseHandle->Close();
        delete poBaseHandle;
        poBaseHandle = NULL;
    }
    return 0;
}
