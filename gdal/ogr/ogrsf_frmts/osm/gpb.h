/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 * Purpose:  Google Protocol Buffer generic handling functions
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GPB_H_INCLUDED
#define GPB_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"

#ifndef CHECK_OOB
#define CHECK_OOB 1
#endif

//#define DEBUG_GPB_ERRORS
#ifdef DEBUG_GPB_ERRORS
static void error_occurred(int nLine)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Parsing error occurred at line %d", nLine);
}

#define GOTO_END_ERROR do { error_occurred(__LINE__); goto end_error; } while(0)
#else
#define GOTO_END_ERROR goto end_error
#endif

#if defined(__GNUC__)
#define CPL_NO_INLINE __attribute__ ((noinline))
#else
#define CPL_NO_INLINE
#endif

/************************************************************************/
/*                Google Protocol Buffer definitions                    */
/************************************************************************/

#define WT_VARINT       0
#define WT_64BIT        1
#define WT_DATA         2
#define WT_STARTGROUP   3
#define WT_ENDGROUP     4
#define WT_32BIT        5

#define MAKE_KEY(nFieldNumber, nWireType) ((nFieldNumber << 3) | nWireType)
#define GET_WIRETYPE(nKey) (nKey & 0x7)
#define GET_FIELDNUMBER(nKey) (nKey >> 3)

/************************************************************************/
/*                          ReadVarUInt32()                             */
/************************************************************************/

#ifndef DO_NOT_DEFINE_READ_VARUINT32
static unsigned int ReadVarUInt32(GByte** ppabyData)
{
    unsigned int nVal = 0;
    int nShift = 0;
    GByte* pabyData = *ppabyData;

    while(true)
    {
        int nByte = *pabyData;
        if (!(nByte & 0x80))
        {
            *ppabyData = pabyData + 1;
            return nVal | ((unsigned)nByte << nShift);
        }
        nVal |= (nByte & 0x7f) << nShift;
        pabyData ++;
        nShift += 7;
        if( nShift == 28 )
        {
            nByte = *pabyData;
            if (!(nByte & 0x80))
            {
                *ppabyData = pabyData + 1;
                return nVal | (((unsigned)nByte & 0xf) << nShift);
            }
            *ppabyData = pabyData;
            return nVal;
        }
    }
}

#define READ_VARUINT32(pabyData, pabyDataLimit, nVal)  \
    { \
        nVal = ReadVarUInt32(&pabyData); \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#define READ_SIZE(pabyData, pabyDataLimit, nSize) \
    { \
        READ_VARUINT32(pabyData, pabyDataLimit, nSize); \
        if (CHECK_OOB && nSize > (unsigned int)(pabyDataLimit - pabyData)) GOTO_END_ERROR; \
    }

#endif

/************************************************************************/
/*                          ReadVarUInt64()                             */
/************************************************************************/

static GUIntBig ReadVarUInt64(GByte** ppabyData)
{
    GUIntBig nVal = 0;
    int nShift = 0;
    GByte* pabyData = *ppabyData;

    while(true)
    {
        int nByte = *pabyData;
        if (!(nByte & 0x80))
        {
            *ppabyData = pabyData + 1;
            return nVal | ((GUIntBig)nByte << nShift);
        }
        nVal |= ((GUIntBig)(nByte & 0x7f)) << nShift;
        pabyData ++;
        nShift += 7;
        if( nShift == 63 )
        {
            nByte = *pabyData;
            if (!(nByte & 0x80))
            {
                *ppabyData = pabyData + 1;
                return nVal | (((GUIntBig)nByte & 1) << nShift);
            }
            *ppabyData = pabyData;
            return nVal;
        }
    }
}

#define READ_VARUINT64(pabyData, pabyDataLimit, nVal)  \
    { \
        nVal = ReadVarUInt64(&pabyData); \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#define READ_VARINT64(pabyData, pabyDataLimit, nVal)  \
    { \
        nVal = (GIntBig)ReadVarUInt64(&pabyData); \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#define READ_VARSINT64(pabyData, pabyDataLimit, nVal)  \
    { \
        GUIntBig l_nVal = ReadVarUInt64(&pabyData); \
        nVal = ((l_nVal & 1) == 0) ? (GIntBig)(l_nVal >> 1) : -(GIntBig)(l_nVal >> 1)-1; \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#define READ_VARSINT64_NOCHECK(pabyData, pabyDataLimit, nVal)  \
    { \
        GUIntBig l_nVal = ReadVarUInt64(&pabyData); \
        nVal = ((l_nVal & 1) == 0) ? (GIntBig)(l_nVal >> 1) : -(GIntBig)(l_nVal >> 1)-1; \
    }

#define READ_SIZE64(pabyData, pabyDataLimit, nSize) \
    { \
        READ_VARUINT64(pabyData, pabyDataLimit, nSize); \
        if (CHECK_OOB && nSize > (unsigned int)(pabyDataLimit - pabyData)) GOTO_END_ERROR; \
    }

/************************************************************************/
/*                           ReadVarInt64()                             */
/************************************************************************/

#ifndef DO_NOT_DEFINE_READ_VARINT64
static GIntBig ReadVarInt64(GByte** ppabyData)
{
    return (GIntBig)ReadVarUInt64(ppabyData);
}
#endif

/************************************************************************/
/*                           ReadVarInt32()                             */
/************************************************************************/

static int ReadVarInt32(GByte** ppabyData)
{
    /*  If you use int32 or int64 as the type for a negative number, */
    /* the resulting varint is always ten bytes long */
    GIntBig nVal = (GIntBig)ReadVarUInt64(ppabyData);
    return (int)nVal;
}

#define READ_VARINT32(pabyData, pabyDataLimit, nVal)  \
    { \
        nVal = ReadVarInt32(&pabyData); \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#define READ_VARSINT32(pabyData, pabyDataLimit, nVal)  \
    { \
        nVal = ReadVarInt32(&pabyData); \
        nVal = ((nVal & 1) == 0) ? (int)(((unsigned int)nVal) >> 1) : -(int)(((unsigned int)nVal) >> 1)-1; \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

/************************************************************************/
/*                            SkipVarInt()                              */
/************************************************************************/

#ifndef DO_NOT_DEFINE_SKIP_VARINT

static void SkipVarInt(GByte** ppabyData)
{
    GByte* pabyData = *ppabyData;
    while(true)
    {
        int nByte = *pabyData;
        if (!(nByte & 0x80))
        {
            *ppabyData = pabyData + 1;
            return;
        }
        pabyData ++;
    }
}

#define SKIP_VARINT(pabyData, pabyDataLimit) \
    { \
        SkipVarInt(&pabyData); \
        if (CHECK_OOB && pabyData > pabyDataLimit) GOTO_END_ERROR; \
    }

#endif /* DO_NOT_DEFINE_SKIP_VARINT */

#define READ_FIELD_KEY(nKey) READ_VARINT32(pabyData, pabyDataLimit, nKey)

#define READ_TEXT(pabyData, pabyDataLimit, pszTxt) \
        unsigned int nDataLength; \
        READ_SIZE(pabyData, pabyDataLimit, nDataLength); \
        pszTxt = (char*)VSI_MALLOC_VERBOSE(nDataLength + 1); \
        if( pszTxt == NULL ) GOTO_END_ERROR; \
        memcpy(pszTxt, pabyData, nDataLength); \
        pszTxt[nDataLength] = 0; \
        pabyData += nDataLength;

/************************************************************************/
/*                         SkipUnknownField()                           */
/************************************************************************/

#define SKIP_UNKNOWN_FIELD_INLINE(pabyData, pabyDataLimit, verbose) \
        int nWireType = GET_WIRETYPE(nKey); \
        if (verbose) \
        { \
            int nFieldNumber = GET_FIELDNUMBER(nKey); \
            CPLDebug("PBF", "Unhandled case: nFieldNumber = %d, nWireType = %d", nFieldNumber, nWireType); \
        } \
        switch (nWireType) \
        { \
            case WT_VARINT: \
            { \
                SKIP_VARINT(pabyData, pabyDataLimit); \
                break; \
            } \
            case WT_64BIT: \
            { \
                if (CHECK_OOB && pabyDataLimit - pabyData < 8) GOTO_END_ERROR; \
                pabyData += 8; \
                break; \
            } \
            case WT_DATA: \
            { \
                unsigned int nDataLength; \
                READ_SIZE(pabyData, pabyDataLimit, nDataLength); \
                pabyData += nDataLength; \
                break; \
            } \
            case WT_32BIT: \
            { \
                if (CHECK_OOB && pabyDataLimit - pabyData < 4) GOTO_END_ERROR; \
                pabyData += 4; \
                break; \
            } \
            default: \
                GOTO_END_ERROR; \
        }

#ifndef DO_NOT_DEFINE_SKIP_UNKNOWN_FIELD

static
int SkipUnknownField(int nKey, GByte* pabyData, GByte* pabyDataLimit, int verbose) CPL_NO_INLINE;

/* Putting statics in headers is trouble. */
static
CPL_UNUSED
int SkipUnknownField(int nKey, GByte* pabyData, GByte* pabyDataLimit, int verbose)
{
    GByte* pabyDataBefore = pabyData;
    SKIP_UNKNOWN_FIELD_INLINE(pabyData, pabyDataLimit, verbose);
    return static_cast<int>(pabyData - pabyDataBefore);
end_error:
    return -1;
}

#define SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, verbose) \
    { \
        int _nOffset = SkipUnknownField(nKey, pabyData, pabyDataLimit, verbose); \
        if (_nOffset < 0) \
            GOTO_END_ERROR; \
        pabyData += _nOffset; \
    }

#endif /* DO_NOT_DEFINE_SKIP_UNKNOWN_FIELD */

#endif /* GPB_H_INCLUDED */
