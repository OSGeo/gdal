/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
 * Author:   Even Rouault, <even dot rouault at mines-dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _FILEGDBTABLE_PRIV_H_INCLUDED
#define _FILEGDBTABLE_PRIV_H_INCLUDED

#include "filegdbtable.h"
#include "cpl_error.h"

namespace OpenFileGDB
{

/************************************************************************/
/*                              GetInt16()                              */
/************************************************************************/

static GInt16 GetInt16(const GByte* pBaseAddr, int iOffset)
{
    GInt16 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetUInt16()                             */
/************************************************************************/

static GUInt16 GetUInt16(const GByte* pBaseAddr, int iOffset)
{
    GUInt16 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetInt32()                              */
/************************************************************************/

static GInt32 GetInt32(const GByte* pBaseAddr, int iOffset)
{
    GInt32 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR32(&nVal);
    return nVal;
}

/************************************************************************/
/*                              GetUInt32()                             */
/************************************************************************/

static GUInt32 GetUInt32(const GByte* pBaseAddr, int iOffset)
{
    GUInt32 nVal;
    memcpy(&nVal, pBaseAddr + sizeof(nVal) * iOffset, sizeof(nVal));
    CPL_LSBPTR32(&nVal);
    return nVal;
}

/************************************************************************/
/*                             GetFloat32()                             */
/************************************************************************/

static float GetFloat32(const GByte* pBaseAddr, int iOffset)
{
    float fVal;
    memcpy(&fVal, pBaseAddr + sizeof(fVal) * iOffset, sizeof(fVal));
    CPL_LSBPTR32(&fVal);
    return fVal;
}

/************************************************************************/
/*                             GetFloat64()                             */
/************************************************************************/

static double GetFloat64(const GByte* pBaseAddr, int iOffset)
{
    double dfVal;
    memcpy(&dfVal, pBaseAddr + sizeof(dfVal) * iOffset, sizeof(dfVal));
    CPL_LSBPTR64(&dfVal);
    return dfVal;
}

void FileGDBTablePrintError(const char* pszFile, int nLineNumber);

#define PrintError()        FileGDBTablePrintError(__FILE__, __LINE__)

/************************************************************************/
/*                          returnError()                               */
/************************************************************************/

#define returnError() \
    do { PrintError(); return (errorRetValue); } while(0)

/************************************************************************/
/*                         returnErrorIf()                              */
/************************************************************************/

#define returnErrorIf(expr) \
    do { if( (expr) ) returnError(); } while(0)

/************************************************************************/
/*                       returnErrorAndCleanupIf()                      */
/************************************************************************/

#define returnErrorAndCleanupIf(expr, cleanup) \
    do { if( (expr) ) { cleanup; returnError(); } } while(0)

}; /* namespace OpenFileGDB */

#endif /* _FILEGDBTABLE_PRIV_H_INCLUDED */
