/**********************************************************************
 *
 * Name:     mitab_datfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the MIDDATAFile class used to handle
 *           reading/writing of the MID/MIF files
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <cstdarg>
#include <cstddef>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class MIDDATAFile
 *
 *====================================================================*/

MIDDATAFile::MIDDATAFile( const char* pszEncoding ) :
    m_fp(nullptr),
    m_pszDelimiter("\t"),  // Encom 2003 (was NULL).
    m_pszFname(nullptr),
    m_eAccessMode(TABRead),
    m_dfXMultiplier(1.0),
    m_dfYMultiplier(1.0),
    m_dfXDisplacement(0.0),
    m_dfYDisplacement(0.0),
    m_bEof(FALSE),
    m_osEncoding(pszEncoding)
{
}

MIDDATAFile::~MIDDATAFile() { Close(); }

void MIDDATAFile::SaveLine(const char *pszLine)
{
    if(pszLine == nullptr)
    {
        m_osSavedLine.clear();
    }
    else
    {
        m_osSavedLine = pszLine;
    }
}

const char *MIDDATAFile::GetSavedLine() { return m_osSavedLine.c_str(); }

int MIDDATAFile::Open(const char *pszFname, const char *pszAccess)
{
    if(m_fp)
    {
        return -1;
    }

    // Validate access mode and make sure we use Text access.
    if(STARTS_WITH_CI(pszAccess, "r"))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rt";
    }
    else if(STARTS_WITH_CI(pszAccess, "w"))
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wt";
    }
    else
    {
        return -1;
    }

    // Open file for reading.
    m_pszFname = CPLStrdup(pszFname);
    m_fp = VSIFOpenL(m_pszFname, pszAccess);

    if(m_fp == nullptr)
    {
        CPLFree(m_pszFname);
        m_pszFname = nullptr;
        return -1;
    }

    SetEof(FALSE);
    return 0;
}

int MIDDATAFile::Rewind()
{
    if(m_fp == nullptr || m_eAccessMode == TABWrite)
        return -1;

    else
    {
        VSIRewindL(m_fp);
        SetEof(FALSE);
    }
    return 0;
}

int MIDDATAFile::Close()
{
    if(m_fp == nullptr)
        return 0;

    // Close file
    VSIFCloseL(m_fp);
    m_fp = nullptr;

    // clear readline buffer.
    CPLReadLineL(nullptr);

    CPLFree(m_pszFname);
    m_pszFname = nullptr;

    return 0;
}

const char *MIDDATAFile::GetLine()
{
    if(m_eAccessMode != TABRead)
    {
        CPLAssert(false);
        return nullptr;
    }

    static const int nMaxLineLength = atoi(
        CPLGetConfigOption("MITAB_MAX_LINE_LENGTH", "1000000"));
    const char *pszLine = CPLReadLine2L(m_fp, nMaxLineLength, nullptr);

    if(pszLine == nullptr)
    {
        if( strstr(CPLGetLastErrorMsg(), "Maximum number of characters allowed reached") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Maximum number of characters allowed reached. "
                     "You can set the MITAB_MAX_LINE_LENGTH configuration option "
                     "to the desired number of bytes (or -1 for unlimited)");
        }
        SetEof(TRUE);
        m_osLastRead.clear();
    }
    else
    {
        // Skip leading spaces and tabs except if the delimiter is tab.
        while(*pszLine == ' ' ||
                          (*m_pszDelimiter != '\t' && *pszLine == '\t'))
            pszLine++;

        m_osLastRead = pszLine;
    }

#if DEBUG_VERBOSE
    if(pszLine)
        CPLDebug("MITAB", "pszLine: %s", pszLine);
#endif

    return pszLine;
}

const char *MIDDATAFile::GetLastLine()
{
    // Return NULL if EOF.
    if(GetEof())
    {
        return nullptr;
    }
    if(m_eAccessMode == TABRead)
    {
#if DEBUG_VERBOSE
        CPLDebug("MITAB", "m_osLastRead: %s", m_osLastRead.c_str());
#endif
        return m_osLastRead.c_str();
    }

    // We should never get here.  Read/Write mode not implemented.
    CPLAssert(false);
    return nullptr;
}

char** MIDDATAFile::GetTokenizedNextLine()
{
    static const int nMaxLineLength = atoi(
        CPLGetConfigOption("MITAB_MAX_LINE_LENGTH", "1000000"));
    char** papszTokens = CSVReadParseLine3L( m_fp,
                                             nMaxLineLength,
                                             m_pszDelimiter,
                                             true, // bHonourStrings
                                             false, // bKeepLeadingAndClosingQuotes
                                             false, // bMergeDelimiter
                                             false // bSkipBOM
                                            );
    if( papszTokens == nullptr )
    {
        if( strstr(CPLGetLastErrorMsg(), "Maximum number of characters allowed reached") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Maximum number of characters allowed reached. "
                     "You can set the MITAB_MAX_LINE_LENGTH configuration option "
                     "to the desired number of bytes (or -1 for unlimited)");
        }
        SetEof(TRUE);
    }
    return papszTokens;
}

void MIDDATAFile::WriteLine(const char *pszFormat, ...)
{
    va_list args;

    if(m_eAccessMode == TABWrite && m_fp)
    {
        va_start(args, pszFormat);
        CPLString osStr;
        osStr.vPrintf(pszFormat, args);
        VSIFWriteL(osStr.c_str(), 1, osStr.size(), m_fp);
        va_end(args);
    }
    else
    {
        CPLAssert(false);
    }
}

void MIDDATAFile::SetTranslation( double dfXMul,double dfYMul,
                                  double dfXTran, double dfYTran )
{
    m_dfXMultiplier = dfXMul;
    m_dfYMultiplier = dfYMul;
    m_dfXDisplacement = dfXTran;
    m_dfYDisplacement = dfYTran;
}

double MIDDATAFile::GetXTrans(double dfX)
{
    return (dfX * m_dfXMultiplier) + m_dfXDisplacement;
}

double MIDDATAFile::GetYTrans(double dfY)
{
    return (dfY * m_dfYMultiplier) + m_dfYDisplacement;
}

GBool MIDDATAFile::IsValidFeature(const char *pszString)
{
    char **papszToken = CSLTokenizeString(pszString);

    if(CSLCount(papszToken) == 0)
    {
        CSLDestroy(papszToken);
        return FALSE;
    }

    if(EQUAL(papszToken[0], "NONE") || EQUAL(papszToken[0], "POINT") ||
       EQUAL(papszToken[0], "LINE") || EQUAL(papszToken[0], "PLINE") ||
       EQUAL(papszToken[0], "REGION") || EQUAL(papszToken[0], "ARC") ||
       EQUAL(papszToken[0], "TEXT") || EQUAL(papszToken[0], "RECT") ||
       EQUAL(papszToken[0], "ROUNDRECT") || EQUAL(papszToken[0], "ELLIPSE") ||
       EQUAL(papszToken[0], "MULTIPOINT") || EQUAL(papszToken[0], "COLLECTION"))
    {
        CSLDestroy(papszToken);
        return TRUE;
    }

    CSLDestroy(papszToken);
    return FALSE;
}

GBool MIDDATAFile::GetEof() { return m_bEof; }

const CPLString& MIDDATAFile::GetEncoding() const
{
    return m_osEncoding;
}

void MIDDATAFile::SetEncoding( const CPLString& osEncoding )
{
    m_osEncoding = osEncoding;
}

void MIDDATAFile::SetEof(GBool bEof) { m_bEof = bEof; }
