/**********************************************************************
 * $Id$
 *
 * Name:     cpl_time.h
 * Project:  CPL - Common Portability Library
 * Purpose:  Time functions.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_TIME_H_INCLUDED
#define CPL_TIME_H_INCLUDED

#include <time.h>

#include "cpl_port.h"

struct tm CPL_DLL *CPLUnixTimeToYMDHMS(GIntBig unixTime, struct tm *pRet);
GIntBig CPL_DLL CPLYMDHMSToUnixTime(const struct tm *brokendowntime);

int CPL_DLL CPLParseRFC822DateTime(const char *pszRFC822DateTime, int *pnYear,
                                   int *pnMonth, int *pnDay, int *pnHour,
                                   int *pnMinute, int *pnSecond, int *pnTZFlag,
                                   int *pnWeekDay);
#endif  // CPL_TIME_H_INCLUDED
