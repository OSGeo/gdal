/*
 * $Id$
 *
 * Equivalent of <time.h> - time types.
 *
 * Created by Mateusz Loskot, mloskot@taxussi.com.pl
 *
 * Copyright (c) 2006 Taxus SI Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom 
 * the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * MIT License:
 * http://opensource.org/licenses/mit-license.php
 *
 * Contact:
 * Taxus SI Ltd.
 * http://www.taxussi.com.pl
 *
 */
#ifndef _WCE_TIME_H
#define _WCE_TIME_H 1

#if !defined(_WIN32_WCE)
# error "Only Winddows CE target is supported!"
#endif


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*******************************************************************************
    Types and macros definition
*******************************************************************************/

#ifndef _TIME_T_DEFINED
typedef long    time_t;		/* time value as number of seconds of the Epoch */
# define _TIME_T_DEFINED
#endif

#ifndef _TM_DEFINED
struct tm
{
	int tm_sec;     /* seconds after the minute - [0,59] */
	int tm_min;     /* minutes after the hour - [0,59] */
	int tm_hour;	/* hours since midnight - [0,23] */
	int tm_mday;	/* day of the month - [1,31] */
	int tm_mon;     /* months since January - [0,11] */
	int tm_year;	/* years since 1900 */
	int tm_wday;	/* days since Sunday - [0,6] */
	int tm_yday;	/* days since January 1 - [0,365] */
	int tm_isdst;	/* daylight savings time flag */
};
# define _TM_DEFINED
#endif

/*
 * Constants used internally by time functions.
 */

/* Epoch base year */
#define EPOCH_YEAR 1970

/* tm struct members conversion units */
#define TM_YEAR_BASE    1900    /* tm_year base year */
#define TM_MONTH_MIN    0       /* tm_mon = 0 - January */ 
#define TM_MONTH_MAX    11      /* tm_mon = 11 - December */ 

#define MIN_SEC         60                  /* seconds in a minute */
#define HOUR_SEC        3600                /* seconds in an hour */
#define DAY_SEC         86400               /* seconds in a day */
#define YEAR_SEC        (365 * DAY_SEC)     /* seconds in a year */
#define FOUR_YEAR_SEC   (4 * YEAR_SEC + 1)  /* seconds in a 4-year period */

/*
In every, 400 year period (greg) is an interval of the same
number of days: 365 x 400 + 97 = 146097
Also, there are 97 leap days in every such 400 years interval
*/
#define LEAP_DAYS_IN_GREG   97
#define GREG_YEARS   400
#define GREG_DAYS    (365 * GREG_YEARS + LEAP_DAYS_IN_GREG)
#define GREG_SECS    (GREG_DAYS * DAY_SEC)

/* Checks if given year is a leap year. */
#define IS_LEAP_YEAR(year) \
    (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0))

/*******************************************************************************
    Time functions
*******************************************************************************/

time_t wce_time(time_t *timer);
time_t wce_mktime(struct tm *tmbuff);
time_t wce_gmmktime(struct tm *tmbuff);

struct tm * wce_localtime(const time_t *timer);
struct tm * wce_gmtime(const time_t *timer);

char * wce_ctime(const time_t *timer);
char * wce_ctime_r(const time_t *timer, char *buf);

char * wce_asctime(const struct tm *tmbuff);
char * wce_asctime_r(const struct tm *tbuff, char *buff);


/*******************************************************************************
    Internal functions prototypes.
*******************************************************************************/
 
/* Internal function to get time value from tm struc. */
extern time_t __wce_mktime_utc(struct tm *tmbuff);



#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* #ifndef _WCE_TIME_H */

