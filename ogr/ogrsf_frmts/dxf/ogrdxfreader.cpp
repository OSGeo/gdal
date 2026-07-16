/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements low level DXF reading with caching and parsing of
 *           of the code/value pairs.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

#include <cinttypes>

/************************************************************************/
/*                         ~OGRDXFReaderBase()                          */
/************************************************************************/

OGRDXFReaderBase::~OGRDXFReaderBase() = default;

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void OGRDXFReaderBase::Initialize(VSILFILE *fpIn)

{
    fp = fpIn;
}

/************************************************************************/
/*                          ResetReadPointer()                          */
/************************************************************************/

void OGRDXFReaderASCII::ResetReadPointer(uint64_t iNewOffset,
                                         int nNewLineNumber /* = 0 */)

{
    nSrcBufferBytes = 0;
    iSrcBufferOffset = 0;
    iSrcBufferFileOffset = iNewOffset;
    nLastValueSize = 0;
    nLineNumber = nNewLineNumber;

    VSIFSeekL(fp, iNewOffset, SEEK_SET);
}

/************************************************************************/
/*                           LoadDiskChunk()                            */
/*                                                                      */
/*      Load another block (512 bytes) of input from the source         */
/*      file.                                                           */
/************************************************************************/

void OGRDXFReaderASCII::LoadDiskChunk()

{
    if (nSrcBufferBytes - iSrcBufferOffset > 511)
        return;

    if (iSrcBufferOffset > 0)
    {
        CPLAssert(nSrcBufferBytes <= 1024);
        CPLAssert(iSrcBufferOffset <= nSrcBufferBytes);

        memmove(achSrcBuffer.data(), achSrcBuffer.data() + iSrcBufferOffset,
                nSrcBufferBytes - iSrcBufferOffset);
        iSrcBufferFileOffset += iSrcBufferOffset;
        nSrcBufferBytes -= iSrcBufferOffset;
        iSrcBufferOffset = 0;
    }

    nSrcBufferBytes += static_cast<int>(
        VSIFReadL(achSrcBuffer.data() + nSrcBufferBytes, 1, 512, fp));
    achSrcBuffer[nSrcBufferBytes] = '\0';

    CPLAssert(nSrcBufferBytes <= 1024);
    CPLAssert(iSrcBufferOffset <= nSrcBufferBytes);
}

/************************************************************************/
/*                             ReadValue()                              */
/*                                                                      */
/*      Read one type code and value line pair from the DXF file.       */
/************************************************************************/

int OGRDXFReaderASCII::ReadValueRaw(char *pszValueBuf, int nValueBufSize)

{
    /* -------------------------------------------------------------------- */
    /*      Make sure we have lots of data in our buffer for one value.     */
    /* -------------------------------------------------------------------- */
    if (nSrcBufferBytes - iSrcBufferOffset < 512)
        LoadDiskChunk();

    /* -------------------------------------------------------------------- */
    /*      Capture the value code, and skip past it.                       */
    /* -------------------------------------------------------------------- */
    unsigned int iStartSrcBufferOffset = iSrcBufferOffset;
    int nValueCode = atoi(achSrcBuffer.data() + iSrcBufferOffset);

    nLineNumber++;

    // proceed to newline.
    while (achSrcBuffer[iSrcBufferOffset] != '\n' &&
           achSrcBuffer[iSrcBufferOffset] != '\r' &&
           achSrcBuffer[iSrcBufferOffset] != '\0')
        iSrcBufferOffset++;

    if (achSrcBuffer[iSrcBufferOffset] == '\0')
        return -1;

    // skip past newline.  CR, CRLF, or LFCR
    if ((achSrcBuffer[iSrcBufferOffset] == '\r' &&
         achSrcBuffer[iSrcBufferOffset + 1] == '\n') ||
        (achSrcBuffer[iSrcBufferOffset] == '\n' &&
         achSrcBuffer[iSrcBufferOffset + 1] == '\r'))
        iSrcBufferOffset += 2;
    else
        iSrcBufferOffset += 1;

    if (achSrcBuffer[iSrcBufferOffset] == '\0')
        return -1;

    /* -------------------------------------------------------------------- */
    /*      Capture the value string.                                       */
    /* -------------------------------------------------------------------- */
    unsigned int iEOL = iSrcBufferOffset;
    CPLString osValue;

    nLineNumber++;

    // proceed to newline.
    while (achSrcBuffer[iEOL] != '\n' && achSrcBuffer[iEOL] != '\r' &&
           achSrcBuffer[iEOL] != '\0')
        iEOL++;

    bool bLongLine = false;
    while (achSrcBuffer[iEOL] == '\0' ||
           (achSrcBuffer[iEOL] == '\r' && achSrcBuffer[iEOL + 1] == '\0'))
    {
        // The line is longer than the buffer (or the line ending is split at
        // end of buffer). Let's copy what we have so far into our string, and
        // read more
        const auto nValueLength = osValue.length();

        if (nValueLength + iEOL - iSrcBufferOffset > 1048576)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Line %d is too long",
                     nLineNumber);
            return -1;
        }

        osValue.resize(nValueLength + iEOL - iSrcBufferOffset, '\0');
        std::copy(achSrcBuffer.data() + iSrcBufferOffset,
                  achSrcBuffer.data() + iEOL, osValue.begin() + nValueLength);

        iSrcBufferOffset = iEOL;
        LoadDiskChunk();
        iEOL = iSrcBufferOffset;
        bLongLine = true;

        // Have we prematurely reached the end of the file?
        if (achSrcBuffer[iEOL] == '\0')
            return -1;

        // Proceed to newline again
        while (achSrcBuffer[iEOL] != '\n' && achSrcBuffer[iEOL] != '\r' &&
               achSrcBuffer[iEOL] != '\0')
            iEOL++;
    }

    size_t nValueBufLen = 0;

    // If this was an extremely long line, copy from osValue into the buffer
    if (!osValue.empty())
    {
        strncpy(pszValueBuf, osValue.c_str(), nValueBufSize - 1);
        pszValueBuf[nValueBufSize - 1] = '\0';

        nValueBufLen = strlen(pszValueBuf);

        if (static_cast<int>(osValue.length()) > nValueBufSize - 1)
        {
            CPLDebug("DXF", "Long line truncated to %d characters.\n%s...",
                     nValueBufSize - 1, pszValueBuf);
        }
    }

    // Copy the last (normally, the only) section of this line into the buffer
    if (static_cast<int>(iEOL - iSrcBufferOffset) >
        nValueBufSize - static_cast<int>(nValueBufLen) - 1)
    {
        strncpy(pszValueBuf + nValueBufLen,
                achSrcBuffer.data() + iSrcBufferOffset,
                nValueBufSize - static_cast<int>(nValueBufLen) - 1);
        pszValueBuf[nValueBufSize - 1] = '\0';

        CPLDebug("DXF", "Long line truncated to %d characters.\n%s...",
                 nValueBufSize - 1, pszValueBuf);
    }
    else
    {
        strncpy(pszValueBuf + nValueBufLen,
                achSrcBuffer.data() + iSrcBufferOffset,
                iEOL - iSrcBufferOffset);
        pszValueBuf[nValueBufLen + iEOL - iSrcBufferOffset] = '\0';
    }

    iSrcBufferOffset = iEOL;

    // skip past newline.  CR, CRLF, or LFCR
    if ((achSrcBuffer[iSrcBufferOffset] == '\r' &&
         achSrcBuffer[iSrcBufferOffset + 1] == '\n') ||
        (achSrcBuffer[iSrcBufferOffset] == '\n' &&
         achSrcBuffer[iSrcBufferOffset + 1] == '\r'))
        iSrcBufferOffset += 2;
    else
        iSrcBufferOffset += 1;

    /* -------------------------------------------------------------------- */
    /*      Record how big this value was, so it can be unread safely.      */
    /* -------------------------------------------------------------------- */
    if (bLongLine)
        nLastValueSize = 0;
    else
    {
        nLastValueSize = iSrcBufferOffset - iStartSrcBufferOffset;
        CPLAssert(nLastValueSize > 0);
    }

    return nValueCode;
}

int OGRDXFReaderASCII::ReadValue(char *pszValueBuf, int nValueBufSize)
{
    int nValueCode;
    while (true)
    {
        nValueCode = ReadValueRaw(pszValueBuf, nValueBufSize);
        if (nValueCode == 999)
        {
            // Skip comments
            continue;
        }
        break;
    }
    return nValueCode;
}

/************************************************************************/
/*                            UnreadValue()                             */
/*                                                                      */
/*      Unread the last value read, accomplished by resetting the       */
/*      read pointer.                                                   */
/************************************************************************/

void OGRDXFReaderASCII::UnreadValue()

{
    if (nLastValueSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot UnreadValue(), likely due to a previous long line");
        return;
    }
    CPLAssert(iSrcBufferOffset >= nLastValueSize);
    CPLAssert(nLastValueSize > 0);

    iSrcBufferOffset -= nLastValueSize;
    nLineNumber -= 2;
    nLastValueSize = 0;
}

int OGRDXFReaderBinary::ReadValue(char *pszValueBuffer, int nValueBufferSize)
{
    if (VSIFTellL(fp) == 0)
    {
        VSIFSeekL(
            fp, static_cast<vsi_l_offset>(AUTOCAD_BINARY_DXF_SIGNATURE.size()),
            SEEK_SET);
    }
    if (VSIFTellL(fp) == AUTOCAD_BINARY_DXF_SIGNATURE.size())
    {
        // Detect if the file is AutoCAD Binary r12
        GByte abyZeroSection[8] = {0};
        if (VSIFReadL(abyZeroSection, 1, sizeof(abyZeroSection), fp) !=
            sizeof(abyZeroSection))
        {
            CPLError(CE_Failure, CPLE_FileIO, "File too short");
            return -1;
        }
        m_bIsR12 = memcmp(abyZeroSection, "\x00SECTION", 8) == 0;
        VSIFSeekL(
            fp, static_cast<vsi_l_offset>(AUTOCAD_BINARY_DXF_SIGNATURE.size()),
            SEEK_SET);
    }

    m_nPrevPos = VSIFTellL(fp);

    uint16_t nCode = 0;
    bool bReadCodeUINT16 = true;
    if (m_bIsR12)
    {
        GByte nCodeByte = 0;
        if (VSIFReadL(&nCodeByte, 1, 1, fp) != 1)
        {
            CPLError(CE_Failure, CPLE_FileIO, "File too short");
            return -1;
        }
        bReadCodeUINT16 = (nCodeByte == 255);
        if (!bReadCodeUINT16)
            nCode = nCodeByte;
    }
    if (bReadCodeUINT16)
    {
        if (VSIFReadL(&nCode, 1, sizeof(uint16_t), fp) != sizeof(uint16_t))
        {
            CPLError(CE_Failure, CPLE_FileIO, "File too short");
            return -1;
        }
        CPL_LSBPTR16(&nCode);
    }

    // Credits to ezdxf for the ranges
    bool bRet = true;
    if (nCode >= 290 && nCode < 300)
    {
        GByte nVal = 0;
        bRet = VSIFReadL(&nVal, 1, sizeof(nVal), fp) == 1;
        CPLsnprintf(pszValueBuffer, nValueBufferSize, "%d", nVal);
        // CPLDebug("DXF", "Read %d: %d", nCode, nVal);
    }
    else if ((nCode >= 60 && nCode < 80) || (nCode >= 170 && nCode < 180) ||
             (nCode >= 270 && nCode < 290) || (nCode >= 370 && nCode < 390) ||
             (nCode >= 400 && nCode < 410) || (nCode >= 1060 && nCode < 1071))
    {
        int16_t nVal = 0;
        bRet = VSIFReadL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
        CPL_LSBPTR16(&nVal);
        CPLsnprintf(pszValueBuffer, nValueBufferSize, "%d", nVal);
        // CPLDebug("DXF", "Read %d: %d", nCode, nVal);
    }
    else if ((nCode >= 90 && nCode < 100) || (nCode >= 420 && nCode < 430) ||
             (nCode >= 440 && nCode < 460) || (nCode == 1071))
    {
        int32_t nVal = 0;
        bRet = VSIFReadL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
        CPL_LSBPTR32(&nVal);
        CPLsnprintf(pszValueBuffer, nValueBufferSize, "%d", nVal);
        // CPLDebug("DXF", "Read %d: %d", nCode, nVal);
    }
    else if (nCode >= 160 && nCode < 170)
    {
        int64_t nVal = 0;
        bRet = VSIFReadL(&nVal, 1, sizeof(nVal), fp) == sizeof(nVal);
        CPL_LSBPTR64(&nVal);
        CPLsnprintf(pszValueBuffer, nValueBufferSize, "%" PRId64, nVal);
        // CPLDebug("DXF", "Read %d: %" PRId64, nCode, nVal);
    }
    else if ((nCode >= 10 && nCode < 60) || (nCode >= 110 && nCode < 150) ||
             (nCode >= 210 && nCode < 240) || (nCode >= 460 && nCode < 470) ||
             (nCode >= 1010 && nCode < 1060))
    {
        double dfVal = 0;
        bRet = VSIFReadL(&dfVal, 1, sizeof(dfVal), fp) == sizeof(dfVal);
        CPL_LSBPTR64(&dfVal);
        CPLsnprintf(pszValueBuffer, nValueBufferSize, "%.17g", dfVal);
        // CPLDebug("DXF", "Read %d: %g", nCode, dfVal);
    }
    else if ((nCode >= 310 && nCode < 320) || nCode == 1004)
    {
        // Binary
        GByte nChunkLength = 0;
        bRet = VSIFReadL(&nChunkLength, 1, sizeof(nChunkLength), fp) ==
               sizeof(nChunkLength);
        std::vector<GByte> abyData(nChunkLength);
        bRet &= VSIFReadL(abyData.data(), 1, nChunkLength, fp) == nChunkLength;
        if (2 * nChunkLength + 1 > nValueBufferSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Provided buffer too small to store string");
            return -1;
        }
        for (int i = 0; i < nChunkLength; ++i)
        {
            snprintf(pszValueBuffer + 2 * i, nValueBufferSize - 2 * i, "%02X",
                     abyData[i]);
        }
        pszValueBuffer[2 * nChunkLength] = 0;
        // CPLDebug("DXF", "Read %d: '%s'", nCode, pszValueBuffer);
    }
    else
    {
        // Zero terminated string
        bool bEOS = false;
        for (int i = 0; bRet && i < nValueBufferSize; ++i)
        {
            char ch = 0;
            bRet = VSIFReadL(&ch, 1, 1, fp) == 1;
            pszValueBuffer[i] = ch;
            if (ch == 0)
            {
                // CPLDebug("DXF", "Read %d: '%s'", nCode, pszValueBuffer);
                bEOS = true;
                break;
            }
        }
        if (!bEOS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Provided buffer too small to store string");
            while (bRet)
            {
                char ch = 0;
                bRet = VSIFReadL(&ch, 1, 1, fp) == 1;
                if (ch == 0)
                {
                    break;
                }
            }
            return -1;
        }
    }

    if (!bRet)
    {
        CPLError(CE_Failure, CPLE_FileIO, "File too short");
        return -1;
    }
    return nCode;
}

void OGRDXFReaderBinary::UnreadValue()
{
    if (m_nPrevPos == static_cast<uint64_t>(-1))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "UnreadValue() can be called just once after ReadValue()");
    }
    else
    {
        VSIFSeekL(fp, m_nPrevPos, SEEK_SET);
        m_nPrevPos = static_cast<uint64_t>(-1);
    }
}

void OGRDXFReaderBinary::ResetReadPointer(uint64_t nPos, int nNewLineNumber)
{
    VSIFSeekL(fp, nPos, SEEK_SET);
    nLineNumber = nNewLineNumber;
}
