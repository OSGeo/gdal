/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Input/output stream wrapper for usage with LizardTech's
 *           MrSID SDK, implemenattion of the wrapper class methods.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "mrsidstream.h"

CPL_CVSID("$Id$");

LT_USE_NAMESPACE(LizardTech)

/************************************************************************/
/* ==================================================================== */
/*                              LTIVSIStream                             */
/* ==================================================================== */
/************************************************************************/

LTIVSIStream::LTIVSIStream() : poFileHandle(NULL)
{
}

/************************************************************************/
/*                             ~LTIVSIStream()                           */
/************************************************************************/

LTIVSIStream::~LTIVSIStream()
{
    if ( poFileHandle )
        VSIFCloseL( (FILE *)poFileHandle );
}

/************************************************************************/
/*                              initialize()                            */
/************************************************************************/

LT_STATUS LTIVSIStream::initialize( const char *pszFilename,
                                    const char *pszAccess )
{
    poFileHandle = (VSIVirtualHandle *)VSIFOpenL( pszFilename, pszAccess );
    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                              initialize()                            */
/************************************************************************/

LT_STATUS LTIVSIStream::initialize( VSIVirtualHandle *poFileHandle )
{
    this->poFileHandle = poFileHandle;
    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                                 isEOF()                              */
/************************************************************************/

bool LTIVSIStream::isEOF()
{
    return 0 != poFileHandle->Eof();
}

/************************************************************************/
/*                                 isOpen()                             */
/************************************************************************/

bool LTIVSIStream::isOpen()
{
    return  poFileHandle != NULL ;
}

/************************************************************************/
/*                                  open()                              */
/************************************************************************/

LT_STATUS LTIVSIStream::open()
{
    return poFileHandle ? LT_STS_Success : LT_STS_Failure;
}

/************************************************************************/
/*                                  close()                             */
/************************************************************************/

LT_STATUS LTIVSIStream::close()
{
    if ( poFileHandle->Seek( 0, SEEK_SET ) == 0 )
        return LT_STS_Success;
    else
        return LT_STS_Failure;
}

/************************************************************************/
/*                                   read()                             */
/************************************************************************/

lt_uint32 LTIVSIStream::read( lt_uint8 *pDest, lt_uint32 nBytes )
{
    return (lt_uint32)poFileHandle->Read( pDest, 1, nBytes );
}

/************************************************************************/
/*                                  write()                             */
/************************************************************************/

lt_uint32 LTIVSIStream::write( const lt_uint8 *pSrc, lt_uint32 nBytes )
{
    return (lt_uint32)poFileHandle->Write( pSrc, 1, nBytes );
}

/************************************************************************/
/*                                   seek()                             */
/************************************************************************/

LT_STATUS LTIVSIStream::seek( lt_int64 nOffset, LTIOSeekDir nOrigin )
{
    int nWhence;
    switch (nOrigin)
    {
        case (LTIO_SEEK_DIR_BEG):
            nWhence = SEEK_SET;
            break;
      
        case (LTIO_SEEK_DIR_CUR):
            nWhence =  SEEK_CUR;
            break;
      
        case (LTIO_SEEK_DIR_END):
            nWhence = SEEK_END;
            break;
      
        default:
            return LT_STS_Failure;
    }

    if ( poFileHandle->Seek( (vsi_l_offset)nOffset, nWhence ) == 0 )
        return LT_STS_Success;
    else
        return LT_STS_Failure;
}

/************************************************************************/
/*                               tell()                                 */
/************************************************************************/

lt_int64 LTIVSIStream::tell()
{
    return (lt_int64)poFileHandle->Tell();
}

/************************************************************************/
/*                               duplicate()                            */
/************************************************************************/

LTIOStreamInf* LTIVSIStream::duplicate()
{
    LTIVSIStream *poNew = new LTIVSIStream;
    poNew->initialize( poFileHandle );

    return poNew;
}

/************************************************************************/
/*                             getLastError()                           */
/************************************************************************/

LT_STATUS LTIVSIStream::getLastError() const
{
    return LT_STS_Success;
}

/************************************************************************/
/*                                  getID()                             */
/************************************************************************/

const char *LTIVSIStream::getID() const
{
    return "LTIVSIStream:";
}

