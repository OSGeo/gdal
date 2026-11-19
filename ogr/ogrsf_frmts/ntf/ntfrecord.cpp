/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  NTFRecord class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ntf.h"
#include "cpl_conv.h"

static int nFieldBufSize = 0;
static char *pszFieldBuf = nullptr;

constexpr int MAX_RECORD_LEN = 160;

/************************************************************************/
/*                             NTFRecord()                              */
/*                                                                      */
/*      The constructor is where the record is read.  This includes     */
/*      transparent merging of continuation lines.                      */
/************************************************************************/

NTFRecord::NTFRecord(VSILFILE *fp)
{
    if (fp == nullptr)
        return;

    /* ==================================================================== */
    /*      Read lines until we get to one without a continuation mark.     */
    /* ==================================================================== */
    const char *pszLine;
    while ((pszLine = CPLReadLine2L(fp, MAX_RECORD_LEN + 2, nullptr)) !=
           nullptr)
    {
        int nNewLength = static_cast<int>(strlen(pszLine));
        while (nNewLength > 0 && pszLine[nNewLength - 1] == ' ')
            --nNewLength;

        if (nNewLength < 2 || pszLine[nNewLength - 1] != '%')
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt NTF record, missing end '%%'.");
            osData.clear();
            break;
        }

        if (osData.empty())
        {
            osData.assign(pszLine, nNewLength - 2);
        }
        else
        {
            if (!STARTS_WITH_CI(pszLine, "00") || nNewLength < 4)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid line");
                osData.clear();
                return;
            }

            try
            {
                osData.append(pszLine + 2, nNewLength - 4);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too large file");
                osData.clear();
                return;
            }
        }

        if (pszLine[nNewLength - 2] != '1')
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Figure out the record type.                                     */
    /* -------------------------------------------------------------------- */
    if (osData.size() >= 2)
    {
        char szType[3] = {osData[0], osData[1], 0};
        nType = atoi(szType);
    }
}

/************************************************************************/
/*                             ~NTFRecord()                             */
/************************************************************************/

NTFRecord::~NTFRecord()

{
    if (pszFieldBuf != nullptr)
    {
        CPLFree(pszFieldBuf);
        pszFieldBuf = nullptr;
        nFieldBufSize = 0;
    }
}

/************************************************************************/
/*                              GetField()                              */
/*                                                                      */
/*      Note that the start position is 1 based, to match the           */
/*      notation in the NTF document.  The returned pointer is to an    */
/*      internal buffer, but is zero terminated.                        */
/************************************************************************/

const char *NTFRecord::GetField(int nStart, int nEnd) const

{
    const int nSize = nEnd - nStart + 1;

    if (osData.empty())
        return "";

    /* -------------------------------------------------------------------- */
    /*      Reallocate working buffer larger if needed.                     */
    /* -------------------------------------------------------------------- */
    if (nFieldBufSize < nSize + 1)
    {
        CPLFree(pszFieldBuf);
        nFieldBufSize = nSize + 1;
        pszFieldBuf = static_cast<char *>(CPLMalloc(nFieldBufSize));
    }

    /* -------------------------------------------------------------------- */
    /*      Copy out desired data.                                          */
    /* -------------------------------------------------------------------- */
    if (nStart + nSize > GetLength() + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to read %d to %d, beyond the end of %d byte long\n"
                 "type `%2.2s' record.\n",
                 nStart, nEnd, GetLength(), osData.c_str());
        memset(pszFieldBuf, ' ', nSize);
        pszFieldBuf[nSize] = '\0';
    }
    else
    {
        strncpy(pszFieldBuf, osData.c_str() + nStart - 1, nSize);
        pszFieldBuf[nSize] = '\0';
    }

    return pszFieldBuf;
}
