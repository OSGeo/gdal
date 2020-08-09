/******************************************************************************
 *
 * Project:  PDS Driver; Planetary Data System Format
 * Purpose:  Implementation of NASAKeywordHandler - a class to read
 *           keyword data from PDS, ISIS2 and ISIS3 data products.
 * Author:   Frank Warmerdam <warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2017 Hobu Inc
 * Copyright (c) 2017, Dmitry Baryshnikov <polimax@mail.ru>
 * Copyright (c) 2017, NextGIS <info@nextgis.com>
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

#ifndef NASAKEYWORDHANDLER_H
#define NASAKEYWORDHANDLER_H

//! @cond Doxygen_Suppress

#include "cpl_json.h"
#include "cpl_string.h"

/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

// Only exported for HDF4 plugin needs. Do not use outside of GDAL please.

class CPL_DLL NASAKeywordHandler
{
    char     **papszKeywordList = nullptr;

    CPLString osHeaderText{};
    const char *pszHeaderNext = nullptr;

    CPLJSONObject oJSon{};

    bool m_bStripSurroundingQuotes = false;

    void    SkipWhite();
    int     ReadWord( CPLString &osWord,
                      bool bStripSurroundingQuotes = false,
                      bool bParseList = false,
                      bool* pbIsString = nullptr);
    int     ReadPair( CPLString &osName, CPLString &osValue, CPLJSONObject &oCur );
    int     ReadGroup( const char *pszPathPrefix, CPLJSONObject &oCur, int nRecLevel );

    NASAKeywordHandler(const NASAKeywordHandler&) = delete;
    NASAKeywordHandler& operator=(const NASAKeywordHandler&) = delete;

public:
    NASAKeywordHandler();
    ~NASAKeywordHandler();

    void SetStripSurroundingQuotes( bool bStripSurroundingQuotes )
                { m_bStripSurroundingQuotes = bStripSurroundingQuotes; }

    int     Ingest( VSILFILE *fp, int nOffset );

    const char *GetKeyword( const char *pszPath, const char *pszDefault );
    char **GetKeywordList();
    CPLJSONObject GetJsonObject() const;
};

//! @endcond

#endif //  NASAKEYWORDHANDLER_H
