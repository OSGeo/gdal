/**********************************************************************
 *
 * Name:     cpl_time.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Time functions.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
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
#include "cpl_string.h"

#include <cstring>
#include <ctime>

#include "cpl_error.h"

CPL_CVSID("$Id$")

constexpr int SECSPERMIN = 60;
constexpr int MINSPERHOUR = 60;
constexpr int HOURSPERDAY = 24;
constexpr int SECSPERHOUR = SECSPERMIN * MINSPERHOUR;
constexpr int SECSPERDAY = SECSPERHOUR * HOURSPERDAY;
constexpr int DAYSPERWEEK = 7;
constexpr int MONSPERYEAR = 12;

constexpr int EPOCH_YEAR = 1970;
constexpr int EPOCH_WDAY = 4;
constexpr int TM_YEAR_BASE = 1900;
constexpr int DAYSPERNYEAR = 365;
constexpr int DAYSPERLYEAR = 366;

static bool isleap( int y )
{
    return ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
}

static int LEAPS_THROUGH_END_OF( int y )
{
    return y / 4 - y / 100 + y / 400;
}

constexpr int mon_lengths[2][MONSPERYEAR] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

constexpr int year_lengths[2] = { DAYSPERNYEAR, DAYSPERLYEAR };

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

    constexpr GIntBig TEN_THOUSAND_YEARS =
        static_cast<GIntBig>(10000) * SECSPERDAY * DAYSPERLYEAR;
    if( unixTime < -TEN_THOUSAND_YEARS || unixTime > TEN_THOUSAND_YEARS )
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



/************************************************************************/
/*                      OGRParseRFC822DateTime()                        */
/************************************************************************/

static const char* const aszWeekDayStr[] = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

static const char* const aszMonthStr[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/** Parse a RFC822 formatted date-time string.
 *
 * Such as [Fri,] 28 Dec 2007 05:24[:17] GMT
 *
 * @param pszRFC822DateTime formatted string.
 * @param pnYear pointer to int receiving year (like 1980, 2000, etc...), or NULL
 * @param pnMonth pointer to int receiving month (between 1 and 12), or NULL
 * @param pnDay pointer to int receiving day of month (between 1 and 31), or NULL
 * @param pnHour pointer to int receiving hour of day (between 0 and 23), or NULL
 * @param pnMinute pointer to int receiving minute (between 0 and 59), or NULL
 * @param pnSecond pointer to int receiving second (between 0 and 60, or -1 if unknown), or NULL
 * @param pnTZFlag pointer to int receiving time zone flag (0=unknown, 100=GMT,
 *                 101=GMT+15minute, 99=GMT-15minute), or NULL
 * @param pnWeekDay pointer to int receiving day of week (between 1 and 7, or 0 if invalid/unset), or NULL
 * @return TRUE if parsing is successful
 *
 * @since GDAL 2.3
 */
int CPLParseRFC822DateTime( const char* pszRFC822DateTime,
                            int* pnYear,
                            int* pnMonth,
                            int* pnDay,
                            int* pnHour,
                            int* pnMinute,
                            int* pnSecond,
                            int* pnTZFlag,
                            int* pnWeekDay )
{
    // Following
    // https://www.w3.org/Protocols/rfc822/#z28 :
    // [Fri,] 28 Dec 2007 05:24[:17] GMT
    char** papszTokens =
        CSLTokenizeStringComplex( pszRFC822DateTime, " ,:", TRUE, FALSE );
    char** papszVal = papszTokens;
    int nTokens = CSLCount(papszTokens);
    if( nTokens < 5 )
    {
        CSLDestroy(papszTokens);
        return false;
    }

    if( pnWeekDay )
        *pnWeekDay = 0;

    if( !((*papszVal)[0] >= '0' && (*papszVal)[0] <= '9') )
    {
        if( pnWeekDay )
        {
            for( size_t i = 0; i < CPL_ARRAYSIZE(aszWeekDayStr); ++i )
            {
                if( EQUAL(*papszVal, aszWeekDayStr[i]) )
                {
                    *pnWeekDay = static_cast<int>(i+1);
                    break;
                }
            }
        }

        ++papszVal;
    }

    int day = atoi(*papszVal);
    if( day <= 0 || day >= 32 )
    {
        CSLDestroy(papszTokens);
        return false;
    }
    if( pnDay )
        *pnDay = day;
    ++papszVal;

    int month = 0;
    for( int i = 0; i < 12; ++i )
    {
        if( EQUAL(*papszVal, aszMonthStr[i]) )
        {
            month = i + 1;
            break;
        }
    }
    if( month == 0 )
    {
        CSLDestroy(papszTokens);
        return false;
    }
    if( pnMonth )
        *pnMonth = month;
    ++papszVal;

    int year = atoi(*papszVal);
    if( year < 100 && year >= 30 )
        year += 1900;
    else if( year < 30 && year >= 0 )
        year += 2000;
    if( pnYear )
        *pnYear = year;
    ++papszVal;

    int hour = atoi(*papszVal);
    if( hour < 0 || hour >= 24 )
    {
        CSLDestroy(papszTokens);
        return false;
    }
    if( pnHour )
        *pnHour = hour;
    ++papszVal;

    if( *papszVal == nullptr )
    {
        CSLDestroy(papszTokens);
        return false;
    }
    int minute = atoi(*papszVal);
    if( minute < 0 || minute >= 60 )
    {
        CSLDestroy(papszTokens);
        return false;
    }
    if (pnMinute )
        *pnMinute = minute;
    ++papszVal;

    if( *papszVal != nullptr && (*papszVal)[0] >= '0' && (*papszVal)[0] <= '9' )
    {
        int second = atoi(*papszVal);
        if( second < 0 || second >= 61 )
        {
            CSLDestroy(papszTokens);
            return false;
        }
        if( pnSecond )
            *pnSecond = second;
        ++papszVal;
    }
    else if( pnSecond )
        *pnSecond = -1;

    int TZ = 0;
    if( *papszVal == nullptr )
    {
    }
    else if( strlen(*papszVal) == 5 &&
                ((*papszVal)[0] == '+' || (*papszVal)[0] == '-') )
    {
        char szBuf[3] = { (*papszVal)[1], (*papszVal)[2], 0 };
        const int TZHour = atoi(szBuf);
        if( TZHour < 0 || TZHour >= 15 )
        {
            CSLDestroy(papszTokens);
            return false;
        }
        szBuf[0] = (*papszVal)[3];
        szBuf[1] = (*papszVal)[4];
        szBuf[2] = 0;
        const int TZMinute = atoi(szBuf);
        TZ = 100 + (((*papszVal)[0] == '+') ? 1 : -1) *
                    ((TZHour * 60 + TZMinute) / 15);
    }
    else
    {
        const char* aszTZStr[] = {
            "GMT", "UT", "Z", "EST", "EDT", "CST", "CDT", "MST", "MDT",
            "PST", "PDT"
        };
        const int anTZVal[] = { 0, 0, 0, -5, -4, -6, -5, -7, -6, -8, -7 };
        TZ = -1;
        for( int i = 0; i < 11; ++i )
        {
            if( EQUAL(*papszVal, aszTZStr[i]) )
            {
                TZ = 100 + anTZVal[i] * 4;
                break;
            }
        }
        if( TZ < 0 )
        {
            CSLDestroy(papszTokens);
            return false;
        }
    }

    if( pnTZFlag )
        *pnTZFlag = TZ;

    CSLDestroy(papszTokens);
    return true;
}
