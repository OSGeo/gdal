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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_oci.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          OGROCIStringBuf()                           */
/************************************************************************/

OGROCIStringBuf::OGROCIStringBuf()
{
    nBufSize = 25;
    pszString = (char *)CPLMalloc(nBufSize);
    nLen = 0;
    pszString[0] = '\0';
}

/************************************************************************/
/*                          OGROCIStringBuf()                           */
/************************************************************************/

OGROCIStringBuf::~OGROCIStringBuf()

{
    CPLFree(pszString);
}

/************************************************************************/
/*                            MakeRoomFor()                             */
/************************************************************************/

void OGROCIStringBuf::MakeRoomFor(int nCharacters)

{
    UpdateEnd();

    if (nLen + nCharacters > nBufSize - 2)
    {
        nBufSize = (int)((nLen + nCharacters) * 1.3);
        pszString = (char *)CPLRealloc(pszString, nBufSize);
    }
}

/************************************************************************/
/*                               Append()                               */
/************************************************************************/

void OGROCIStringBuf::Append(const char *pszNewText)

{
    int nNewLen = static_cast<int>(strlen(pszNewText));

    MakeRoomFor(nNewLen);
    strcat(pszString + nLen, pszNewText);
    nLen += nNewLen;
}

/************************************************************************/
/*                              Appendf()                               */
/************************************************************************/

void OGROCIStringBuf::Appendf(int nMax, const char *pszFormat, ...)

{
    va_list args;
    char szSimpleBuf[100];
    char *pszBuffer;

    if (nMax > (int)sizeof(szSimpleBuf) - 1)
        pszBuffer = (char *)CPLMalloc(nMax + 1);
    else
        pszBuffer = szSimpleBuf;

    va_start(args, pszFormat);
    CPLvsnprintf(pszBuffer, nMax, pszFormat, args);
    va_end(args);

    Append(pszBuffer);
    if (pszBuffer != szSimpleBuf)
        CPLFree(pszBuffer);
}

/************************************************************************/
/*                             UpdateEnd()                              */
/************************************************************************/

void OGROCIStringBuf::UpdateEnd()

{
    nLen += static_cast<int>(strlen(pszString + nLen));
}

/************************************************************************/
/*                            StealString()                             */
/************************************************************************/

char *OGROCIStringBuf::StealString()

{
    char *pszStolenString = pszString;

    nBufSize = 100;
    pszString = (char *)CPLMalloc(nBufSize);
    nLen = 0;

    return pszStolenString;
}

/************************************************************************/
/*                              GetLast()                               */
/************************************************************************/

char OGROCIStringBuf::GetLast()

{
    if (nLen != 0)
        return pszString[nLen - 1];
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
