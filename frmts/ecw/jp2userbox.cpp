/******************************************************************************
 * $Id$
 *
 * Project:  GDAL ECW Driver
 * Purpose:  JP2UserBox implementation - arbitrary box read/write.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_ecw.h"

CPL_CVSID("$Id$");

#if defined(HAVE_COMPRESS)

/************************************************************************/
/*                             JP2UserBox()                             */
/************************************************************************/

JP2UserBox::JP2UserBox()
{
    pabyData = NULL;
    nDataLength = 0;

    m_nTBox = 0;
}

/************************************************************************/
/*                            ~JP2UserBox()                             */
/************************************************************************/

JP2UserBox::~JP2UserBox()

{
    if( pabyData != NULL )
    {
        CPLFree( pabyData );
        pabyData = NULL;
    }
}

/************************************************************************/
/*                              SetData()                               */
/************************************************************************/

void JP2UserBox::SetData( int nLengthIn, const unsigned char *pabyDataIn )

{
    if( pabyData != NULL )
        CPLFree( pabyData );

    nDataLength = nLengthIn;
    pabyData = (unsigned char *) CPLMalloc(nDataLength);
    memcpy( pabyData, pabyDataIn, nDataLength );

    m_bValid = true;
}

/************************************************************************/
/*                            UpdateXLBox()                             */
/************************************************************************/

void JP2UserBox::UpdateXLBox()

{
    m_nXLBox = 8 + nDataLength;
    m_nLDBox = nDataLength;
}

/************************************************************************/
/*                               Parse()                                */
/*                                                                      */
/*      Parse box, and data contents from file into memory.             */
/************************************************************************/

#if ECWSDK_VERSION >= 40
CNCSError JP2UserBox::Parse( NCS::JP2::CFile &JP2File, 
                             NCS::CIOStream &Stream )
#else
CNCSError JP2UserBox::Parse( class CNCSJP2File &JP2File, 
                             CNCSJPCIOStream &Stream )
#endif
{
    CNCSError Error = NCS_SUCCESS;
    
    return Error;
}

/************************************************************************/
/*                              UnParse()                               */
/*                                                                      */
/*      Write box meta information, and data to file.                   */
/************************************************************************/

#if ECWSDK_VERSION >= 40
CNCSError JP2UserBox::UnParse( NCS::JP2::CFile &JP2File, 
                               NCS::CIOStream &Stream )
#else
CNCSError JP2UserBox::UnParse( class CNCSJP2File &JP2File, 
                               CNCSJPCIOStream &Stream )
#endif
{
    CNCSError Error = NCS_SUCCESS;

    if( m_nTBox == 0 )
    {
        Error = NCS_UNKNOWN_ERROR;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No box type set in JP2UserBox::UnParse()" );
        return Error;
    }

    Error = CNCSJP2Box::UnParse(JP2File, Stream);

//    NCSJP2_CHECKIO_BEGIN(Error, Stream);
    Stream.Write(pabyData, nDataLength);
//    NCSJP2_CHECKIO_END();

    return Error;
}

#endif /* defined(HAVE_COMPRESS) */
