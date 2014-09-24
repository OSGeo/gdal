/**********************************************************************
 * $Id$
 *
 * Name:     cpl_time.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Time functions.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 *
 * CPLUnixTimeToYMDHMS() is derived from timesub() in localtime.c from openbsd/freebsd/netbsd.
 * CPLYMDHMSToUnixTime() has been implemented by Even Rouault and is in the public domain
 *
 * Cf http://svn.freebsd.org/viewvc/base/stable/7/lib/libc/stdtime/localtime.c?revision=178142&view=markup
 * localtime.c comes with the following header :
 *
 * This file is in the public domain, so clarified as of
 * 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
 */

#include "cpl_time.h"

#define SECSPERMIN      60L
#define MINSPERHOUR     60L
#define HOURSPERDAY     24L
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY      (SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK     7
#define MONSPERYEAR     12

#define EPOCH_YEAR      1970
#define EPOCH_WDAY      4
#define TM_YEAR_BASE    1900
#define DAYSPERNYEAR    365
#define DAYSPERLYEAR    366

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#define LEAPS_THRU_END_OF(y)	((y) / 4 - (y) / 100 + (y) / 400)

static const int mon_lengths[2][MONSPERYEAR] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
} ;


static const int	year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

/************************************************************************/
/*                   CPLUnixTimeToYMDHMS()                              */
/************************************************************************/

/** Converts a time value since the Epoch (aka "unix" time) to a broken-down UTC time.
 *
 * This function is similar to gmtime_r().
 * This function will always set tm_isdst to 0.
 *
 * @param unixTime number of seconds since the Epoch.
 * @param pRet address of the return structure.
 *
 * @return the structure pointed by pRet filled with a broken-down UTC time.
 */

struct tm * CPLUnixTimeToYMDHMS(GIntBig unixTime, struct tm* pRet)
{
    GIntBig days = unixTime / SECSPERDAY;
    GIntBig rem = unixTime % SECSPERDAY;
    
    while (rem < 0) {
        rem += SECSPERDAY;
        --days;
    }
    
    pRet->tm_hour = (int) (rem / SECSPERHOUR);
    rem = rem % SECSPERHOUR;
    pRet->tm_min = (int) (rem / SECSPERMIN);
    /*
    ** A positive leap second requires a special
    ** representation.  This uses "... ??:59:60" et seq.
    */
    pRet->tm_sec = (int) (rem % SECSPERMIN);
    pRet->tm_wday = (int) ((EPOCH_WDAY + days) % DAYSPERWEEK);
    if (pRet->tm_wday < 0)
        pRet->tm_wday += DAYSPERWEEK;
    GIntBig y = EPOCH_YEAR;
    int yleap;
    while (days < 0 || days >= (GIntBig) year_lengths[yleap = isleap(y)])
    {
        GIntBig	newy;

        newy = y + days / DAYSPERNYEAR;
        if (days < 0)
            --newy;
        days -= (newy - y) * DAYSPERNYEAR +
            LEAPS_THRU_END_OF(newy - 1) -
            LEAPS_THRU_END_OF(y - 1);
        y = newy;
    }
    pRet->tm_year = (int) (y - TM_YEAR_BASE);
    pRet->tm_yday = (int) days;
    const int* ip = mon_lengths[yleap];
    for (pRet->tm_mon = 0; days >= (GIntBig) ip[pRet->tm_mon]; ++(pRet->tm_mon))
        days = days - (GIntBig) ip[pRet->tm_mon];
    pRet->tm_mday = (int) (days + 1);
    pRet->tm_isdst = 0;
    
    return pRet;
}

/************************************************************************/
/*                      CPLYMDHMSToUnixTime()                           */
/************************************************************************/

/** Converts a broken-down UTC time into time since the Epoch (aka "unix" time).
 *
 * This function is similar to mktime(), but the passed structure is not modified.
 * This function ignores the tm_wday, tm_yday and tm_isdst fields of the passed value.
 * No timezone shift will be applied. This function returns 0 for the 1/1/1970 00:00:00
 *
 * @param brokendowntime broken-downtime UTC time.
 *
 * @return a number of seconds since the Epoch encoded as a value of type GIntBig,
 *         or -1 if the time cannot be represented.
 */

GIntBig CPLYMDHMSToUnixTime(const struct tm *brokendowntime)
{
  GIntBig days;
  int mon;
  
  if (brokendowntime->tm_mon < 0 || brokendowntime->tm_mon >= 12)
    return -1;
    
  /* Number of days of the current month */
  days = brokendowntime->tm_mday - 1;
  
  /* Add the number of days of the current year */
  const int* ip = mon_lengths[isleap(TM_YEAR_BASE + brokendowntime->tm_year)];
  for(mon=0;mon<brokendowntime->tm_mon;mon++)
      days += ip [mon];

  /* Add the number of days of the other years */
  days += (TM_YEAR_BASE + (GIntBig)brokendowntime->tm_year - EPOCH_YEAR) * DAYSPERNYEAR +
          LEAPS_THRU_END_OF(TM_YEAR_BASE + (GIntBig)brokendowntime->tm_year - 1) -
          LEAPS_THRU_END_OF(EPOCH_YEAR - 1);

  /* Now add the secondes, minutes and hours to the number of days since EPOCH */
  return brokendowntime->tm_sec +
         brokendowntime->tm_min * SECSPERMIN +
         brokendowntime->tm_hour * SECSPERHOUR +
         days * SECSPERDAY;
}
