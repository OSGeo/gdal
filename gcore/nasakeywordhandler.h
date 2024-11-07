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
 * SPDX-License-Identifier: MIT
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
    CPLStringList aosKeywordList{};

    const char *pszHeaderNext = nullptr;

    CPLJSONObject oJSon{};

    bool m_bStripSurroundingQuotes = false;

    void SkipWhite();
    bool ReadWord(CPLString &osWord, bool bStripSurroundingQuotes = false,
                  bool bParseList = false, bool *pbIsString = nullptr);
    bool ReadPair(CPLString &osName, CPLString &osValue, CPLJSONObject &oCur);
    bool ReadGroup(const std::string &osPathPrefix, CPLJSONObject &oCur,
                   int nRecLevel);

    NASAKeywordHandler(const NASAKeywordHandler &) = delete;
    NASAKeywordHandler &operator=(const NASAKeywordHandler &) = delete;

  public:
    NASAKeywordHandler();
    ~NASAKeywordHandler();

    void SetStripSurroundingQuotes(bool bStripSurroundingQuotes)
    {
        m_bStripSurroundingQuotes = bStripSurroundingQuotes;
    }

    bool Ingest(VSILFILE *fp, int nOffset);
    bool Parse(const char *pszStr);

    const char *GetKeyword(const char *pszPath, const char *pszDefault);
    char **GetKeywordList();
    CPLJSONObject GetJsonObject() const;
};

//! @endcond

#endif  //  NASAKEYWORDHANDLER_H
