/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Implementation of CPLKeywordParser - a class for parsing
 *           the keyword format used for files like QuickBird .RPB files.
 *           This is a slight variation on the NASAKeywordParser used for
 *           the PDS/ISIS2/ISIS3 formats.
 * Author:   Frank Warmerdam <warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef CPL_KEYWORD_PARSER
#define CPL_KEYWORD_PARSER

#include "cpl_string.h"

/************************************************************************/
/* ==================================================================== */
/*                          CPLKeywordParser                          */
/* ==================================================================== */
/************************************************************************/

/*! @cond Doxygen_Suppress */

class CPL_DLL CPLKeywordParser
{
    char **papszKeywordList = nullptr;

    CPLString osHeaderText{};
    const char *pszHeaderNext = nullptr;

    void SkipWhite();
    bool ReadWord(CPLString &osWord);
    bool ReadPair(CPLString &osName, CPLString &osValue);
    bool ReadGroup(const char *pszPathPrefix, int nRecLevel);

    CPL_DISALLOW_COPY_ASSIGN(CPLKeywordParser)

  public:
    CPLKeywordParser();
    ~CPLKeywordParser();

    int Ingest(VSILFILE *fp);

    const char *GetKeyword(const char *pszPath,
                           const char *pszDefault = nullptr);
    char **GetAllKeywords()
    {
        return papszKeywordList;
    }
};

/*! @endcond */

#endif /* def CPL_KEYWORD_PARSER */
