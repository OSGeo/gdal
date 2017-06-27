/**********************************************************************
 *
 * Name:     cpl_time.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Time functions.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 *
 * CPLUnixTimeToYMDHMS() is derived from timesub() in localtime.c from
 * openbsd/freebsd/netbsd.
 *
 * CPLYMDHMSToUnixTime() has been implemented by Even Rouault and is in the
 * public domain.
 *
 * c.f. http://svn.freebsd.org/viewvc/base/stable/7/lib/libc/stdtime/localtime.c?revision=178142&view=markup
 * localtime.c comes with the following header :
 *
 * This file is in the public domain, so clarified as of
 * 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
 */

#include "cpl_time.h"

#include <cstring>
#include <ctime>

#include "cpl_error.h"

CPL_CVSID("$Id$")

static const int SECSPERMIN = 60;
static const int MINSPERHOUR = 60;
static const int HOURSPERDAY = 24;
static const int SECSPERHOUR = SECSPERMIN * MINSPERHOUR;
static const int SECSPERDAY = SECSPERHOUR * HOURSPERDAY;
static const int DAYSPERWEEK = 7;
static const int MONSPERYEAR = 12;

static const int EPOCH_YEAR = 1970;
static const int EPOCH_WDAY = 4;
static const int TM_YEAR_BASE = 1900;
static const int DAYSPERNYEAR = 365;
static const int DAYSPERLYEAR = 366;

static bool isleap( int y )
{
    return ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
}

static int LEAPS_THROUGH_END_OF( int y )
{
    return y / 4 - y / 100 + y / 400;
}

static const int mon_lengths[2][MONSPERYEAR] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const int year_lengths[2] = { DAYSPERNYEAR, DAYSPERLYEAR };

/************************************************************************/
/*                   CPLUnixTimeToYMDHMS()                              */
/************************************************************************/

/** Converts a time value since the Epoch (aka "unix" time) to a broken-down
 *  UTC time.
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

    if( unixTime < -static_cast<GIntBig>(10000) * SECSPERDAY * DAYSPERLYEAR ||
        unixTime > static_cast<GIntBig>(10000) * SECSPERDAY * DAYSPERLYEAR )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid unixTime = " CPL_FRMT_GIB,
                 unixTime);
        memset(pRet, 0, sizeof(*pRet));
        return pRet;
    }

    while( rem < 0 )
    {
        rem += SECSPERDAY;
        --days;
    }

    pRet->tm_hour = static_cast<int>( rem / SECSPERHOUR );
    rem = rem % SECSPERHOUR;
    pRet->tm_min = static_cast<int>( rem / SECSPERMIN );
    /*
    ** A positive leap second requires a special
    ** representation.  This uses "... ??:59:60" et seq.
    */
    pRet->tm_sec = static_cast<int>( rem % SECSPERMIN );
    pRet->tm_wday = static_cast<int>( (EPOCH_WDAY + days) % DAYSPERWEEK );
    if( pRet->tm_wday < 0 )
        pRet->tm_wday += DAYSPERWEEK;

    int y = EPOCH_YEAR;
    int yleap = 0;
    int iters = 0;
    while( iters < 1000 &&
           (days < 0
           || days >= static_cast<GIntBig>( year_lengths[yleap = isleap(y)] )) )
    {
        int newy = y + static_cast<int>( days / DAYSPERNYEAR );
        if( days < 0 )
            --newy;
        days -= (newy - y) * DAYSPERNYEAR +
            LEAPS_THROUGH_END_OF(newy - 1) -
            LEAPS_THROUGH_END_OF(y - 1);
        y = newy;
        iters ++;
    }
    if( iters == 1000 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid unixTime = " CPL_FRMT_GIB,
                 unixTime);
        memset(pRet, 0, sizeof(*pRet));
        return pRet;
    }

    pRet->tm_year = static_cast<int>( y - TM_YEAR_BASE );
    pRet->tm_yday = static_cast<int>( days );
    const int* ip = mon_lengths[yleap];

    for( pRet->tm_mon = 0;
         days >= static_cast<GIntBig>( ip[pRet->tm_mon] );
         ++(pRet->tm_mon) )
        days = days - static_cast<GIntBig>( ip[pRet->tm_mon] );

    pRet->tm_mday = static_cast<int>( (days + 1) );
    pRet->tm_isdst = 0;

    return pRet;
}

/************************************************************************/
/*                      CPLYMDHMSToUnixTime()                           */
/************************************************************************/

/** Converts a broken-down UTC time into time since the Epoch (aka "unix" time).
 *
 * This function is similar to mktime(), but the passed structure is not
 * modified.  This function ignores the tm_wday, tm_yday and tm_isdst fields of
 * the passed value.  No timezone shift will be applied. This function
 * returns 0 for the 1/1/1970 00:00:00
 *
 * @param brokendowntime broken-downtime UTC time.
 *
 * @return a number of seconds since the Epoch encoded as a value of type
 *         GIntBig, or -1 if the time cannot be represented.
 */

GIntBig CPLYMDHMSToUnixTime( const struct tm *brokendowntime )
{

    if( brokendowntime->tm_mon < 0 || brokendowntime->tm_mon >= 12 )
        return -1;

    // Number of days of the current month.
    GIntBig days = brokendowntime->tm_mday - 1;

    // Add the number of days of the current year.
    const int* ip =
        mon_lengths[static_cast<int>(isleap(TM_YEAR_BASE +
                                            brokendowntime->tm_year))];
    for( int mon = 0; mon < brokendowntime->tm_mon; mon++ )
        days += ip[mon];

    // Add the number of days of the other years.
    days +=
        ( TM_YEAR_BASE
          + static_cast<GIntBig>(brokendowntime->tm_year)
          - EPOCH_YEAR ) * DAYSPERNYEAR
        + LEAPS_THROUGH_END_OF(static_cast<int>(TM_YEAR_BASE)
                               + static_cast<int>( brokendowntime->tm_year)
                               - static_cast<int>(1))
        - LEAPS_THROUGH_END_OF( EPOCH_YEAR - 1 );

    // Now add the secondes, minutes and hours to the number of days
    // since EPOCH.
    return
        brokendowntime->tm_sec +
        brokendowntime->tm_min * SECSPERMIN +
        brokendowntime->tm_hour * SECSPERHOUR +
        days * SECSPERDAY;
}
