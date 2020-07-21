/******************************************************************************
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Input/output stream wrapper for usage with LizardTech's
 *           MrSID SDK, implementation of the wrapper class methods.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifdef DEBUG_BOOL
#define DO_NOT_USE_DEBUG_BOOL
#endif

#include "cpl_error.h"
#include "mrsidstream.h"

CPL_CVSID("$Id$")

using namespace LizardTech;

/************************************************************************/
/* ==================================================================== */
/*                              LTIVSIStream                             */
/* ==================================================================== */
/************************************************************************/

LTIVSIStream::LTIVSIStream() : poFileHandle(nullptr), nError(0), pnRefCount(nullptr),
bIsOpen(FALSE)
{
}

/************************************************************************/
/*                             ~LTIVSIStream()                           */
/************************************************************************/

LTIVSIStream::~LTIVSIStream()
{
    if ( poFileHandle)
    {
        (*pnRefCount)--;
        if (*pnRefCount == 0)
        {
            VSIFCloseL( (VSILFILE *)poFileHandle );
            nError = errno;
            delete pnRefCount;
        }
    }
}

/************************************************************************/
/*                              initialize()                            */
/************************************************************************/

LT_STATUS LTIVSIStream::initialize( const char *pszFilename,
                                    const char *pszAccess )
{
    CPLAssert(poFileHandle == nullptr);

    errno = 0;
    poFileHandle = (VSIVirtualHandle *)VSIFOpenL( pszFilename, pszAccess );
    if (poFileHandle)
    {
        pnRefCount = new int;
        *pnRefCount = 1;
    }
    nError = errno;

    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                              initialize()                            */
/************************************************************************/

LT_STATUS LTIVSIStream::initialize( LTIVSIStream* ltiVSIStream )
{
    CPLAssert(poFileHandle == nullptr);

    poFileHandle = ltiVSIStream->poFileHandle;
    if (poFileHandle)
    {
        pnRefCount = ltiVSIStream->pnRefCount;
        (*pnRefCount) ++;
    }

    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                                 isEOF()                              */
/************************************************************************/

bool LTIVSIStream::isEOF()
{
    CPLAssert(poFileHandle);

    errno = 0;
    bool    bIsEOF = (0 != poFileHandle->Eof());
    nError = errno;

    return bIsEOF;
}

/************************************************************************/
/*                                 isOpen()                             */
/************************************************************************/

bool LTIVSIStream::isOpen()
{
    return  poFileHandle != nullptr && bIsOpen;
}

/************************************************************************/
/*                                  open()                              */
/************************************************************************/

LT_STATUS LTIVSIStream::open()
{
    bIsOpen = poFileHandle != nullptr;
    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                                  close()                             */
/************************************************************************/

LT_STATUS LTIVSIStream::close()
{
    CPLAssert(poFileHandle);

    bIsOpen = FALSE;
    errno = 0;
    if ( poFileHandle->Seek( 0, SEEK_SET ) == 0 )
        return LT_STS_Success;
    else
    {
        nError = errno;
        return LT_STS_Failure;
    }
}

/************************************************************************/
/*                                   read()                             */
/************************************************************************/

lt_uint32 LTIVSIStream::read( lt_uint8 *pDest, lt_uint32 nBytes )
{
    CPLAssert(poFileHandle);

    errno = 0;
    lt_uint32   nBytesRead =
        (lt_uint32)poFileHandle->Read( pDest, 1, nBytes );
    nError = errno;

    return nBytesRead;
}

/************************************************************************/
/*                                  write()                             */
/************************************************************************/

lt_uint32 LTIVSIStream::write( const lt_uint8 *pSrc, lt_uint32 nBytes )
{
    CPLAssert(poFileHandle);

    errno = 0;
    lt_uint32   nBytesWritten =
        (lt_uint32)poFileHandle->Write( pSrc, 1, nBytes );
    nError = errno;

    return nBytesWritten;
}

/************************************************************************/
/*                                   seek()                             */
/************************************************************************/

LT_STATUS LTIVSIStream::seek( lt_int64 nOffset, LTIOSeekDir nOrigin )
{
    CPLAssert(poFileHandle);

    int nWhence;
    switch (nOrigin)
    {
        case (LTIO_SEEK_DIR_BEG):
            nWhence = SEEK_SET;
            break;

        case (LTIO_SEEK_DIR_CUR):
        {
            nWhence =  SEEK_CUR;
            if( nOffset < 0 )
            {
                nWhence = SEEK_SET;
                nOffset += (lt_int64)poFileHandle->Tell();
            }
            break;
        }

        case (LTIO_SEEK_DIR_END):
            nWhence = SEEK_END;
            break;

        default:
            return LT_STS_Failure;
    }

    if ( poFileHandle->Seek( (vsi_l_offset)nOffset, nWhence ) == 0 )
        return LT_STS_Success;
    else
    {
        nError = errno;
        return LT_STS_Failure;
    }
}

/************************************************************************/
/*                               tell()                                 */
/************************************************************************/

lt_int64 LTIVSIStream::tell()
{
    CPLAssert(poFileHandle);

    errno = 0;
    lt_int64    nPos = (lt_int64)poFileHandle->Tell();
    nError = errno;

    return nPos;
}

/************************************************************************/
/*                               duplicate()                            */
/************************************************************************/

LTIOStreamInf* LTIVSIStream::duplicate()
{
    LTIVSIStream *poNew = new LTIVSIStream;
    poNew->initialize( this );

    return poNew;
}

/************************************************************************/
/*                             getLastError()                           */
/************************************************************************/

LT_STATUS LTIVSIStream::getLastError() const
{
    return nError;
}

/************************************************************************/
/*                                  getID()                             */
/************************************************************************/

const char *LTIVSIStream::getID() const
{
    return "LTIVSIStream:";
}
