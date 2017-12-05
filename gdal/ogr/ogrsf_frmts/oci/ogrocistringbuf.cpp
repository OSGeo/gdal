/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Simple string buffer used to accumulate text of commands
 *           efficiently.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_oci.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGROCIStringBuf()                           */
/************************************************************************/

OGROCIStringBuf::OGROCIStringBuf()
{
    nBufSize = 25;
    pszString = (char *) CPLMalloc(nBufSize);
    nLen = 0;
    pszString[0] = '\0';
}

/************************************************************************/
/*                          OGROCIStringBuf()                           */
/************************************************************************/

OGROCIStringBuf::~OGROCIStringBuf()

{
    CPLFree( pszString );
}

/************************************************************************/
/*                            MakeRoomFor()                             */
/************************************************************************/

void OGROCIStringBuf::MakeRoomFor( int nCharacters )

{
    UpdateEnd();

    if( nLen + nCharacters > nBufSize - 2 )
    {
        nBufSize = (int) ((nLen + nCharacters) * 1.3);
        pszString = (char *) CPLRealloc(pszString,nBufSize);
    }
}

/************************************************************************/
/*                               Append()                               */
/************************************************************************/

void OGROCIStringBuf::Append( const char *pszNewText )

{
    int  nNewLen = static_cast<int>(strlen(pszNewText));

    MakeRoomFor( nNewLen );
    strcat( pszString+nLen, pszNewText );
    nLen += nNewLen;
}

/************************************************************************/
/*                              Appendf()                               */
/************************************************************************/

void OGROCIStringBuf::Appendf( int nMax, const char *pszFormat, ... )

{
    va_list args;
    char    szSimpleBuf[100];
    char    *pszBuffer;

    if( nMax > (int) sizeof(szSimpleBuf)-1 )
        pszBuffer = (char *) CPLMalloc(nMax+1);
    else
        pszBuffer = szSimpleBuf;

    va_start(args, pszFormat);
    CPLvsnprintf(pszBuffer, nMax, pszFormat, args);
    va_end(args);

    Append( pszBuffer );
    if( pszBuffer != szSimpleBuf )
        CPLFree( pszBuffer );
}

/************************************************************************/
/*                             UpdateEnd()                              */
/************************************************************************/

void OGROCIStringBuf::UpdateEnd()

{
    nLen += static_cast<int>(strlen(pszString+nLen));
}

/************************************************************************/
/*                            StealString()                             */
/************************************************************************/

char *OGROCIStringBuf::StealString()

{
    char *pszStolenString = pszString;

    nBufSize = 100;
    pszString = (char *) CPLMalloc(nBufSize);
    nLen = 0;

    return pszStolenString;
}

/************************************************************************/
/*                              GetLast()                               */
/************************************************************************/

char OGROCIStringBuf::GetLast()

{
    if( nLen != 0 )
        return pszString[nLen-1];
    else
        return '\0';
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

void OGROCIStringBuf::Clear()

{
    pszString[0] = '\0';
    nLen = 0;
}
