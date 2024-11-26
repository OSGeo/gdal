/******************************************************************************
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
 * SPDX-License-Identifier: MIT
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
