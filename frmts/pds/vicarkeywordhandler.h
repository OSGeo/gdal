/******************************************************************************
 *
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARKeywordHandler - a class to read
 *           keyword data from VICAR data products.
 * Authors:  Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 *
 * NOTE: This driver code is loosely based on the ISIS and PDS drivers.
 * It is not intended to diminish the contribution of the authors.
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot
 *de>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VICARKEYWORDHANDLER_H
#define VICARKEYWORDHANDLER_H

#include "cpl_json.h"

class VICARKeywordHandler
{
    char **papszKeywordList;

    CPLString osHeaderText;
    const char *pszHeaderNext;

    CPLJSONObject oJSon;

    void SkipWhite();
    bool ReadName(CPLString &osWord);
    bool ReadValue(CPLString &osWord, bool bInList, bool &bIsString);
    bool ReadPair(CPLString &osName, CPLString &osValue, CPLJSONObject &oCur);
    bool Parse();

  public:
    VICARKeywordHandler();
    ~VICARKeywordHandler();

    bool Ingest(VSILFILE *fp, const GByte *pabyHeader);

    const char *GetKeyword(const char *pszPath, const char *pszDefault) const;

    const CPLJSONObject &GetJsonObject() const
    {
        return oJSon;
    }
};

#endif  // VICARKEYWORDHANDLER_H
