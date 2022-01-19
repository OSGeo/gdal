#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "clock.h"
#include "myutil.h"
#include "myassert.h"
#ifdef MEMWATCH
#include "memwatch.h"
#endif

#include "cpl_port.h"

/* Take a look at the options in:
 * http://www.unet.univie.ac.at/aix/cmds/aixcmds2/date.htm#A270961
 */
/* Timezone is defined through out as the time needed to add to local time
 * to get UTC, rather than the reverse.  So EST is +5 not -5. */

#define PERIOD_YEARS 146097L
#define SEC_DAY 86400L
#define ISLEAPYEAR(y) (((y)%400 == 0) || (((y)%4 == 0) && ((y)%100 != 0)))

/*****************************************************************************
 * ThirdMonday() --
 *
 * Carl McCalla / MDL
 *
 * PURPOSE
 *   Compute the day-of-the-month which is the third Monday of the month.
 *
 * ARGUMENTS
 * monthStartDOW = starting day of the week (e.g., 0 = Sunday, 1 = Monday,
 *                 etc.) (Input)
 *
 * RETURNS
 *   int (the day-of-the-month which is the third Monday of the month)
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static int ThirdMonday (int monthStartDOW)
{
   if (monthStartDOW == 0) {
      return 16;
   } else if (monthStartDOW == 1) {
      return 15;
   } else {
      return ((7 - monthStartDOW) + 16);
   }
}

/*****************************************************************************
 * Memorialday() --
 *
 * Carl McCalla / MDL
 *
 * PURPOSE
 *   For the month of May, compute the day-of-the-month which is Memorial Day.
 *
 * ARGUMENTS
 * monthStartDOW = starting day of the week (e.g., 0 = Sunday, 1 = Monday,
 *                 etc.) (Input)
 *
 * RETURNS
 *   int (the day-of-the-month which is Memorial Day)
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static int Memorialday (int monthStartDOW)
{
   if (monthStartDOW == 0) {
      return 30;
   } else if (monthStartDOW == 6) {
      return 31;
   } else {
      return ((5 - monthStartDOW) + 25);
   }
}

/*****************************************************************************
 * Laborday() --
 *
 * Carl McCalla / MDL
 *
 * PURPOSE
 *   For the month of September, compute the day-of-the-month which is Labor
 * Day.
 *
 * ARGUMENTS
 * monthStartDOW = starting day of the week (e.g., 0 = Sunday, 1 = Monday,
 *                 etc.) (Input)
 *
 * RETURNS
 *   int (the day-of-the-month which is Labor Day)
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static int Laborday (int monthStartDOW)
{
   if (monthStartDOW == 0) {
      return 2;
   } else if (monthStartDOW == 1) {
      return 1;
   } else {
      return ((6 - monthStartDOW) + 3);
   }
}

/*****************************************************************************
 * Columbusday() --
 *
 * Carl McCalla /MDL
 *
 * PURPOSE
 *   For the month of October, compute the day-of-the-month which is Columbus
 * Day.
 *
 * ARGUMENTS
 * monthStartDOW = starting day of the week (e.g., 0 = Sunday, 1 = Monday,
 *                 etc.) (Input)
 *
 * RETURNS
 *   int (the day-of-the-month which is Columbus Day)
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static int Columbusday (int monthStartDOW)
{
   if ((monthStartDOW == 0) || (monthStartDOW == 1)) {
      return (9 - monthStartDOW);
   } else {
      return (16 - monthStartDOW);
   }
}

/*****************************************************************************
 * Thanksgivingday() --
 *
 * Carl McCalla /MDL
 *
 * PURPOSE
 *   For the month of November, compute the day-of-the-month which is
 * Thanksgiving Day.
 *
 * ARGUMENTS
 * monthStartDOW = starting day of the week (e.g., 0 = Sunday, 1 = Monday,
 *                 etc.) (Input)
 *
 * RETURNS
 *   int (the day-of-the-month which is Thanksgiving Day)
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static int Thanksgivingday (int monthStartDOW)
{
   if ((monthStartDOW >= 0) && (monthStartDOW <= 4)) {
      return (26 - monthStartDOW);
   } else if (monthStartDOW == 5) {
      return 28;
   } else {
      return 27;
   }
}

/*****************************************************************************
 * Clock_Holiday() --
 *
 * Carl McCalla /MDL
 *
 * PURPOSE
 *   Return a holiday string (e.g., Christmas Day, Thanksgiving Day, etc.), if
 * the current day of the month is a federal holiday.
 *
 * ARGUMENTS
 *         month = month of the year (e.g., 1 = Jan, 2 = Feb, etc.) (Input)
 *           day = the current day of the month (e.g., 1, 2, 3 ...) (Input)
 * monthStartDOW = the day-of-the-month which is the first day of the month
 *                 (e.g., 0 = Sunday, 1 = Monday, etc.)
 *        answer = String containing the holiday string, if the current day is
 *                 a federal holiday, or a "", if the current day is not a
 *                 federal holiday.
 *
 * RETURNS
 *   void
 *
 * HISTORY
 *   6/2006 Carl McCalla, Sr. (MDL):  Created
 *
 * NOTES
 * ***************************************************************************
 */
static void Clock_Holiday (int month, int day, int monthStartDOW,
                           char answer[100])
{
   switch (month) {
      case 1:          /* January */
         if (day == 1) {
            strcpy (answer, "New Years Day");
            return;
         } else if (ThirdMonday (monthStartDOW) == day) {
            strcpy (answer, "Martin Luther King Jr Day");
            return;
         }
         break;
      case 2:          /* February */
         if (ThirdMonday (monthStartDOW) == day) {
            strcpy (answer, "Presidents Day");
            return;
         }
         break;
      case 5:          /* May */
         if (Memorialday (monthStartDOW) == day) {
            strcpy (answer, "Memorial Day");
            return;
         }
         break;
      case 7:          /* July */
         if (day == 4) {
            strcpy (answer, "Independence Day");
            return;
         }
         break;
      case 9:          /* September */
         if (Laborday (monthStartDOW) == day) {
            strcpy (answer, "Labor Day");
            return;
         }
         break;
      case 10:         /* October */
         if (Columbusday (monthStartDOW) == day) {
            strcpy (answer, "Columbus Day");
            return;
         }
         break;
      case 11:         /* November */
         if (day == 11) {
            strcpy (answer, "Veterans Day");
            return;
         } else if (Thanksgivingday (monthStartDOW) == day) {
            strcpy (answer, "Thanksgiving Day");
            return;
         }
         break;
      case 12:         /* December */
         if (day == 25) {
            strcpy (answer, "Christmas Day");
            return;
         }
         break;
   }
   strcpy (answer, "");
   return;
}

/*****************************************************************************
 * Clock_Epock2YearDay() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To convert the days since the beginning of the epoch to days since
 * beginning of the year and years since the beginning of the epoch.
 *
 * ARGUMENTS
 * totDay = Number of days since the beginning of the epoch. (Input)
 *    Day = The days since the beginning of the year. (Output)
 *     Yr = The years since the epoch. (Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_Epoch2YearDay (sInt4 totDay, int *Day, sInt4 *Yr)
{
   sInt4 year;          /* Local copy of the year. */

   year = 1970;
   /* Jump to the correct 400 year period of time. */
   if ((totDay <= -PERIOD_YEARS) || (totDay >= PERIOD_YEARS)) {
      year += 400 * (totDay / PERIOD_YEARS);
      totDay -= PERIOD_YEARS * (totDay / PERIOD_YEARS);
   }
   if (totDay >= 0) {
      while (totDay >= 366) {
         if (ISLEAPYEAR (year)) {
            if (totDay >= 1461) {
               year += 4;
               totDay -= 1461;
            } else if (totDay >= 1096) {
               year += 3;
               totDay -= 1096;
            } else if (totDay >= 731) {
               year += 2;
               totDay -= 731;
            } else {
               year++;
               totDay -= 366;
            }
         } else {
            year++;
            totDay -= 365;
         }
      }
      if ((totDay == 365) && (!ISLEAPYEAR (year))) {
         year++;
         totDay -= 365;
      }
   } else {
      while (totDay <= -366) {
         year--;
         if (ISLEAPYEAR (year)) {
            if (totDay <= -1461) {
               year -= 3;
               totDay += 1461;
            } else if (totDay <= -1096) {
               year -= 2;
               totDay += 1096;
            } else if (totDay <= -731) {
               year--;
               totDay += 731;
            } else {
               totDay += 366;
            }
         } else {
            totDay += 365;
         }
      }
      if (totDay < 0) {
         year--;
         if (ISLEAPYEAR (year)) {
            totDay += 366;
         } else {
            totDay += 365;
         }
      }
   }
   *Day = (int) totDay;
   *Yr = year;
}

/*****************************************************************************
 * Clock_MonthNum() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Determine which numbered month it is given the day since the beginning of
 * the year, and the year since the beginning of the epoch.
 *
 * ARGUMENTS
 *  day = Day since the beginning of the year. (Input)
 * year = Year since the beginning of the epoch. (Input)
 *
 * RETURNS: int (which month it is)
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_MonthNum (int day, sInt4 year)
{
   if (day < 31)
      return 1;
   if (ISLEAPYEAR (year))
      day -= 1;
   if (day < 59)
      return 2;
   if (day <= 89)
      return 3;
   if (day == 242)
      return 8;
   return ((day + 64) * 5) / 153 - 1;
}

/*****************************************************************************
 * Clock_NumDay() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns either the number of days in the month or the number of days
 * since the beginning of the year.
 *
 * ARGUMENTS
 * month = Month in question. (Input)
 *   day = Day of month in question (Input)
 *  year = years since the epoch (Input)
 * f_tot = 1 if we want total days from beginning of year,
 *         0 if we want total days in the month. (Input)
 *
 * RETURNS: int
 *  Either the number of days in the month, or
 *  the number of days since the beginning of they year.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_NumDay (int month, int day, sInt4 year, char f_tot)
{
   if (f_tot == 1) {
      if (month > 2) {
         if (ISLEAPYEAR (year)) {
            return ((month + 1) * 153) / 5 - 63 + day;
         } else {
            return ((month + 1) * 153) / 5 - 64 + day;
         }
      } else {
         return (month - 1) * 31 + day - 1;
      }
   } else {
      if (month == 1) {
         return 31;
      } else if (month != 2) {
         if ((((month - 3) % 5) % 2) == 1) {
            return 30;
         } else {
            return 31;
         }
      } else {
         if (ISLEAPYEAR (year)) {
            return 29;
         } else {
            return 28;
         }
      }
   }
}

/*****************************************************************************
 * Clock_FormatParse() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To format part of the output clock string.
 *
 * ARGUMENTS
 *    buffer = The output string to write to. (Output)
 *       sec = Seconds since beginning of day. (Input)
 * floatSec = Part of a second since beginning of second. (Input)
 *   totDay = Days since the beginning of the epoch. (Input)
 *      year = Years since the beginning of the epoch (Input)
 *     month = Month since the beginning of the year (Input)
 *       day = Days since the beginning of the year (Input)
 *    format = Which part of the format string we are working on. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
#define SIZEOF_BUFFER   100
static void Clock_FormatParse (char buffer[SIZEOF_BUFFER], sInt4 sec, float floatSec,
                               sInt4 totDay, sInt4 year, int month, int day,
                               char format)
{
   static const char * const MonthName[] = {
      "January", "February", "March", "April", "May", "June", "July",
      "August", "September", "October", "November", "December"
   };
   static const char * const DayName[] = {
      "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
      "Saturday"
   };
   int dy;              /* # of days from start of year to start of month. */
   int i;               /* Temporary variable to help with computations. */
   int DOM;             /* Day of the Month (e.g., 1-31) */
   int DOW;             /* Numeric day of the week (e.g., 0 = Sunday, 1 =
                         * Monday, etc. */
   int monthStartDOW;   /* Numeric day of the week of the 1st day of the
                         * month */
   char temp[100];      /* Helps parse the %D, %T, %r, and %R options. */

   switch (format) {
      case 'd':
         dy = (Clock_NumDay (month, 1, year, 1) - 1);
         snprintf(buffer, SIZEOF_BUFFER, "%02d", day - dy);
         return;
      case 'm':
         snprintf(buffer, SIZEOF_BUFFER, "%02d", month);
         return;
      case 'E':
         snprintf(buffer, SIZEOF_BUFFER, "%2d", month);
         return;
      case 'Y':
         snprintf(buffer, SIZEOF_BUFFER, "%04d", year);
         return;
      case 'H':
         snprintf(buffer, SIZEOF_BUFFER, "%02d", (int) ((sec % 86400L) / 3600));
         return;
      case 'G':
         snprintf(buffer, SIZEOF_BUFFER, "%2d", (int) ((sec % 86400L) / 3600));
         return;
      case 'M':
         snprintf(buffer, SIZEOF_BUFFER, "%02d", (int) ((sec % 3600) / 60));
         return;
      case 'S':
         snprintf(buffer, SIZEOF_BUFFER, "%02d", (int) (sec % 60));
         return;
      case 'f':
         snprintf(buffer, SIZEOF_BUFFER, "%05.2f", ((int) (sec % 60)) + floatSec);
         return;
      case 'n':
         snprintf(buffer, SIZEOF_BUFFER, "\n");
         return;
      case '%':
         snprintf(buffer, SIZEOF_BUFFER, "%%");
         return;
      case 't':
         snprintf(buffer, SIZEOF_BUFFER, "\t");
         return;
      case 'y':
         snprintf(buffer, SIZEOF_BUFFER, "%02d", (int) (year % 100));
         return;
      case 'I':
         i = ((sec % 43200L) / 3600);
         if (i == 0) {
            snprintf(buffer, SIZEOF_BUFFER, "12");
         } else {
            snprintf(buffer, SIZEOF_BUFFER, "%02d", i);
         }
         return;
      case 'p':
         if (((sec % 86400L) / 3600) >= 12) {
            snprintf(buffer, SIZEOF_BUFFER, "PM");
         } else {
            snprintf(buffer, SIZEOF_BUFFER, "AM");
         }
         return;
      case 'B':
         strcpy (buffer, MonthName[month - 1]);
         return;
      case 'A':
         strcpy (buffer, DayName[(4 + totDay) % 7]);
         return;
      case 'b':
      case 'h':
         strcpy (buffer, MonthName[month - 1]);
         buffer[3] = '\0';
         return;
      case 'a':
         strcpy (buffer, DayName[(4 + totDay) % 7]);
         buffer[3] = '\0';
         return;
      case 'w':
         snprintf(buffer, SIZEOF_BUFFER, "%d", (int) ((4 + totDay) % 7));
         return;
      case 'j':
         snprintf(buffer, SIZEOF_BUFFER, "%03d", day + 1);
         return;
      case 'e':
         dy = (Clock_NumDay (month, 1, year, 1) - 1);
         snprintf(buffer, SIZEOF_BUFFER, "%d", (int) (day - dy));
         return;
      case 'W':
         i = (1 - ((4 + totDay - day) % 7)) % 7;
         if (day < i)
            snprintf(buffer, SIZEOF_BUFFER, "00");
         else
            snprintf(buffer, SIZEOF_BUFFER, "%02d", ((day - i) / 7) + 1);
         return;
      case 'U':
         i = (-((4 + totDay - day) % 7)) % 7;
         if (day < i)
            snprintf(buffer, SIZEOF_BUFFER, "00");
         else
            snprintf(buffer, SIZEOF_BUFFER, "%02d", ((day - i) / 7) + 1);
         return;
      case 'D':
         Clock_FormatParse (buffer, sec, floatSec, totDay, year, month,
                            day, 'm');
         strcat (buffer, "/");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'd');
         strcat (buffer, temp);
         strcat (buffer, "/");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'Y');
         strcat (buffer, temp);
         return;
      case 'T':
         Clock_FormatParse (buffer, sec, floatSec, totDay, year, month,
                            day, 'H');
         strcat (buffer, ":");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'M');
         strcat (buffer, temp);
         strcat (buffer, ":");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'S');
         strcat (buffer, temp);
         return;
      case 'r':
         Clock_FormatParse (buffer, sec, floatSec, totDay, year, month,
                            day, 'I');
         strcat (buffer, ":");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'M');
         strcat (buffer, temp);
         strcat (buffer, ":");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'S');
         strcat (buffer, temp);
         strcat (buffer, " ");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'p');
         strcat (buffer, temp);
         return;
      case 'R':
         Clock_FormatParse (buffer, sec, floatSec, totDay, year, month,
                            day, 'H');
         strcat (buffer, ":");
         Clock_FormatParse (temp, sec, floatSec, totDay, year, month,
                            day, 'M');
         strcat (buffer, temp);
         return;

         /* If the current day is a federal holiday, then return a pointer to
          * the appropriate holiday string (e.g., "Martin Luther King Day") */
      case 'v':
         /* Clock_FormatParse 'd' */
         dy = (Clock_NumDay (month, 1, year, 1) - 1);
         DOM = day - dy;
         /* Clock_FormatParse 'w' */
         DOW = (int) ((4 + totDay) % 7);

         if ((DOM % 7) != 1) {
            monthStartDOW = DOW - ((DOM % 7) - 1);
            if (monthStartDOW < 0) {
               monthStartDOW = 7 + monthStartDOW;
            }
         } else {
            monthStartDOW = DOW;
         }

         Clock_Holiday (month, DOM, monthStartDOW, temp);
         if (temp[0] != '\0') {
            strcpy (buffer, temp);
         } else {
            Clock_FormatParse (buffer, sec, floatSec, totDay, year, month,
                               day, 'A');
         }
         return;
      default:
         snprintf(buffer, SIZEOF_BUFFER, "unknown %c", format);
         return;
   }
}

/*****************************************************************************
 * Clock_GetTimeZone() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns the time zone offset in hours to add to local time to get UTC.
 * So EST is +5 not -5.
 *
 * ARGUMENTS
 *
 * RETURNS: int
 *
 * HISTORY
 *   6/2004 Arthur Taylor (MDL): Created.
 *   3/2005 AAT: Found bug... Used to use 1/1/1970 00Z and find the local
 *        hour.  If CET, this means use 1969 date, which causes it to die.
 *        Switched to 1/2/1970 00Z.
 *   3/2005 AAT: timeZone (see CET) can be < 0. don't add 24 if timeZone < 0
 *
 * NOTES
 *****************************************************************************
 */
sChar Clock_GetTimeZone ()
{
   struct tm l_time;
   time_t ansTime;
   struct tm *gmTime;
   static int timeZone = 9999;

   if (timeZone == 9999) {
      /* Cheap method of getting global time_zone variable. */
      memset (&l_time, 0, sizeof (struct tm));
      l_time.tm_year = 70;
      l_time.tm_mday = 2;
      ansTime = mktime (&l_time);
      gmTime = gmtime (&ansTime);
      timeZone = gmTime->tm_hour;
      if (gmTime->tm_mday != 2) {
         timeZone -= 24;
      }
   }
   return timeZone;
}

/*****************************************************************************
 * Clock_IsDaylightSaving() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To determine if daylight savings is in effect.  Daylight savings is in
 * effect from the first sunday in April to the last sunday in October.
 * At 2 AM ST (or 3 AM DT) in April   -> 3 AM DT (and we return 1)
 * At 2 AM DT (or 1 AM ST) in October -> 1 AM ST (and we return 0)
 *
 * ARGUMENTS
 *    l_clock = The time stored as a double. (Input)
 * TimeZone = hours to add to local time to get UTC. (Input)
 *
 * RETURNS: int
 *   0 if not in daylight savings time.
 *   1 if in daylight savings time.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *   2/2007 AAT : Updated yet again.
 *
 * NOTES
 *    From 1987 through 2006, the start and end dates were the first Sunday in
 * April and the last Sunday in October.
 *
 *    Since 1996 the European Union has observed DST from the last Sunday in
 * March to the last Sunday in October, with transitions at 01:00 UTC.
 *
 *    On August 8, 2005, President George W. Bush signed the Energy Policy Act
 * of 2005. This Act changed the time change dates for Daylight Saving Time in
 * the U.S. Beginning in 2007, DST will begin on the second Sunday in March
 * and end the first Sunday in November.

 *    The Secretary of Energy will report the impact of this change to
 * Congress. Congress retains the right to resume the 2005 Daylight Saving
 * Time schedule once the Department of Energy study is complete.
 *
 *                   1st-apr last-oct  2nd-mar 1st-nov
 * 1/1/1995 Sun (0)   4/2     10/29     3/12    11/5
 * 1/1/2001 mon (1)   4/1     10/28     3/11    11/4
 * 1/1/1991 tue (2)   4/7     10/27     3/10    11/3
 * 1/1/2003 Wed (3)   4/6     10/26     3/9     11/2
 * 1/1/1987 thu (4)   4/5     10/25     3/8     11/1
 * 1/1/1999 fri (5)   4/4     10/31     3/14    11/7
 * 1/1/2005 Sat (6)   4/3     10/30     3/13    11/6
 *
 * Leap years:
 * 1/1/2012 Sun (0)
 * 1/1/1996 Mon (1)   4/7     10/27     3/10    11/3
 * 1/1/2008 Tue (2)   4/6     10/26     3/9     11/2
 * 1/1/2020 Wed (3)   4/5     10/25     3/8     11/1

 * 1/1/2004 Thu (4)   4/4     10/31     3/14    11/7
 * 1/1/2032 Thu (4)   4/4     10/31     3/14    11/7

 * 1/1/2016 Fri (5)
 * 1/1/2028 Sat (6)
 *   --- Since there is an extra day, the delta is the same
 *   --- Problems occur with leap years pre 2007 which start on Mon or Thur
 *       (delta shift by 7 days = 604,800 seconds) After 2007, it was leap
 *       years starting only on Thur.
 *****************************************************************************
 */
int Clock_IsDaylightSaving2 (double l_clock, sChar TimeZone)
{
   sInt4 totDay, year;
   int day, first;
   double secs;
   sInt4 start, end;

   /* These are the deltas between the 1st sun in apr and beginning of year
    * in seconds + 2 hours. */
   static const sInt4 start2006[7] = {7869600, 7783200, 8301600, 8215200,
                                8128800, 8042400, 7956000};
   /* These are the deltas between the last sun in oct and beginning of year
    * in seconds + 1 hour. */
   static const sInt4 end2006[7] = {26010000, 25923600, 25837200, 25750800,
                              25664400, 26182800, 26096400};
   /* Previous version had typo ...26664400 -> 25664400 */

   /* These are the deltas between the 2nd sun in mar and beginning of year
    * in seconds + 2 hours. */
   static const sInt4 start2007[7] = {6055200, 5968800, 5882400, 5796000,
                                5709600, 6228000, 6141600};
   /* These are the deltas between the 1st sun in nov and beginning of year
    * in seconds + 1 hour. */
   static const sInt4 end2007[7] = {26614800, 26528400, 26442000, 26355600,
                              26269200, 26787600, 26701200};

   l_clock = l_clock - TimeZone * 3600.;
   /* Clock should now be in Standard Time, so comparisons later have to be
    * based on Standard Time. */

   totDay = (sInt4) floor (l_clock / SEC_DAY);
   Clock_Epoch2YearDay (totDay, &day, &year);
   /* Figure out number of seconds since beginning of year. */
   secs = l_clock - (totDay - day) * SEC_DAY;

   /* figure out if 1/1/year is mon/tue/.../sun */
   first = ((4 + (totDay - day)) % 7); /* -day should get 1/1 but may need
                                        * -day+1 => sun == 0, ... sat == 6 */

   if (year >= 2007) {
      start = start2007[first];
      end = end2007[first];
      if (((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0))) {
         if (first == 4) {
            start += 604800;
            end += 604800;
         }
      }
   } else {
      start = start2006[first];
      end = end2006[first];
      if (((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0))) {
         if (first == 1) {
            start += 604800;
         } else if (first == 4) {
            end += 604800;
         }
      }
   }
   if ((secs >= start) && (secs <= end)) {
      return 1;
   } else {
      return 0;
   }
}

/*****************************************************************************
 * Clock_PrintDate() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 * l_clock = The time stored as a double. (Input)
 *  year = The year. (Output)
 * month = The month. (Output)
 *   day = The day. (Output)
 *  hour = The hour. (Output)
 *   min = The min. (Output)
 *   sec = The second. (Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *   3/2005 Arthur Taylor (MDL): Commented.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_PrintDate (double l_clock, sInt4 *year, int *month, int *day,
                      int *hour, int *min, double *sec)
{
   sInt4 totDay;
   sInt4 intSec;

   totDay = (sInt4) floor (l_clock / SEC_DAY);
   Clock_Epoch2YearDay (totDay, day, year);
   *month = Clock_MonthNum (*day, *year);
   *day = *day - Clock_NumDay (*month, 1, *year, 1) + 1;
   *sec = l_clock - ((double) totDay) * SEC_DAY;
   intSec = (sInt4) (*sec);
   *hour = (int) ((intSec % 86400L) / 3600);
   *min = (int) ((intSec % 3600) / 60);
   *sec = (intSec % 60) + (*sec - intSec);
}

/*****************************************************************************
 * Clock_Print() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To create formatted output from a time structure that is stored as a
 * double.
 *
 * ARGUMENTS
 * buffer = Destination to write the format to. (Output)
 *      n = The number of characters in buffer. (Input)
 *  l_clock = The time stored as a double. (Input)
 * format = The desired output format. (Input)
 *  f_gmt = 0 output GMT, 1 output LDT, 2 output LST. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_Print (char *buffer, int n, double l_clock, const char *format,
                  char f_gmt)
{
   sInt4 totDay, year;
   sInt4 sec;
   double floatSec;
   int month, day;
   size_t i;
   int j;
   char f_perc;
   char locBuff[100];
   sChar timeZone;      /* Hours to add to local time to get UTC. */

   /* Handle gmt problems. */
   if (f_gmt != 0) {
      timeZone = Clock_GetTimeZone ();
      /* l_clock is currently in UTC */
      l_clock -= timeZone * 3600;
      /* l_clock is now in local standard time Note: A 0 is passed to
       * DaylightSavings so it converts from local to local standard time. */
      if ((f_gmt == 1) && (Clock_IsDaylightSaving2 (l_clock, 0) == 1)) {
         l_clock = l_clock + 3600;
      }
   }
   /* Convert from seconds to days and seconds. */
   totDay = (sInt4) floor (l_clock / SEC_DAY);
   Clock_Epoch2YearDay (totDay, &day, &year);
   month = Clock_MonthNum (day, year);
   floatSec = l_clock - ((double) totDay) * SEC_DAY;
   sec = (sInt4) floatSec;
   floatSec = floatSec - sec;

   f_perc = 0;
   j = 0;
   for (i = 0; i < strlen (format); i++) {
      if (j >= n)
         return;
      if (format[i] == '%') {
         f_perc = 1;
      } else {
         if (f_perc == 0) {
            buffer[j] = format[i];
            j++;
            buffer[j] = '\0';
         } else {
            Clock_FormatParse (locBuff, sec, (float)floatSec, totDay, year, month,
                               day, format[i]);
            buffer[j] = '\0';
            strncat (buffer, locBuff, n - j);
            j += (int)strlen (locBuff);
            f_perc = 0;
         }
      }
   }
}

/*****************************************************************************
 * Clock_Print2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To create formatted output from a time structure that is stored as a
 * double.  This is similar to Clock_Print, except it bases the timezone
 * shift on what the user supplies rather than the system timezone, and
 * accepts a flag that indicates whether to inquire about daylight savings.
 *   If f_dayCheck, then it looks at the local time and see's if daylight is
 * in effect.  This allows for points where daylight is never in effect
 * (f_dayCheck = 0).
 *
 * ARGUMENTS
 *     buffer = Destination to write the format to. (Output)
 *          n = The number of characters in buffer. (Input)
 *      l_clock = The time stored as a double (assumed in UTC). (Input)
 *     format = The desired output format. (Input)
 *   timeZone = Hours to add to local time to get UTC. (Input)
 * f_dayCheck = True if we should check if daylight savings is in effect,
 *              after converting to LST. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2006 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_Print2 (char *buffer, int n, double l_clock, char *format,
                   sChar timeZone, sChar f_dayCheck)
{
   sInt4 totDay, year;
   sInt4 sec;
   double floatSec;
   int month, day;
   size_t i;
   int j;
   char f_perc;
   char locBuff[100];

   /* l_clock is currently in UTC */
   l_clock -= timeZone * 3600;
   /* l_clock is now in local standard time */
   if (f_dayCheck) {
      /* Note: A 0 is passed to DaylightSavings so it converts from local to
       * local standard time. */
      if (Clock_IsDaylightSaving2 (l_clock, 0) == 1) {
         l_clock += 3600;
      }
   }

   /* Convert from seconds to days and seconds. */
   totDay = (sInt4) floor (l_clock / SEC_DAY);
   Clock_Epoch2YearDay (totDay, &day, &year);
   month = Clock_MonthNum (day, year);
   floatSec = l_clock - ((double) totDay) * SEC_DAY;
   sec = (sInt4) floatSec;
   floatSec = floatSec - sec;

   f_perc = 0;
   j = 0;
   for (i = 0; i < strlen (format); i++) {
      if (j >= n)
         return;
      if (format[i] == '%') {
         f_perc = 1;
      } else {
         if (f_perc == 0) {
            buffer[j] = format[i];
            j++;
            buffer[j] = '\0';
         } else {
            Clock_FormatParse (locBuff, sec, (float)floatSec, totDay, year, month,
                               day, format[i]);
            buffer[j] = '\0';
            strncat (buffer, locBuff, n - j);
            j += (int)strlen (locBuff);
            f_perc = 0;
         }
      }
   }
}

/*****************************************************************************
 * Clock_Clicks() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns the number of clicks since the program started execution.
 *
 * ARGUMENTS
 *
 * RETURNS: double
 *  Number of clicks since the beginning of the program.
 *
 * HISTORY
 *   6/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
double Clock_Clicks (void)
{
   double ans;

   ans = (double) clock ();
   return ans;
}

/*****************************************************************************
 * Clock_Seconds() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns the current number of seconds since the beginning of the epoch.
 * Using the local system time zone.
 *
 * ARGUMENTS
 *
 * RETURNS: double
 *  Number of seconds since beginning of the epoch.
 *
 * HISTORY
 *   6/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_SetSeconds (double *ptime, sChar f_set)
{
   static double ans = 0;
   static int f_ansSet = 0;

   if (f_set) {
      ans = *ptime;
      f_ansSet = 1;
   } else if (f_ansSet) {
      *ptime = ans;
   }
   return f_ansSet;
}

double Clock_Seconds (void)
{
   double ans;

   if (Clock_SetSeconds (&ans, 0) == 0) {
      ans = time (NULL);
   }
   return ans;
}

/*****************************************************************************
 * Clock_PrintZone() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Prints the time zone based on the shift from UTC and if it is daylight
 * savings or not.
 *
 * ARGUMENTS
 *      ptr = The character string to scan. (Output)
 * TimeZone = Hours to add to local time to get UTC. (Input)
 *    f_day = True if we are dealing with daylight savings. (Input)
 *
 * RETURNS: int
 *  0 if we read TimeZone, -1 if not.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_PrintZone2 (char *ptr, sChar TimeZone, char f_day)
{
   if (TimeZone == 0) {
      strcpy (ptr, "UTC");
      return 0;
   } else if (TimeZone == 5) {
      if (f_day) {
         strcpy (ptr, "EDT");
      } else {
         strcpy (ptr, "EST");
      }
      return 0;
   } else if (TimeZone == 6) {
      if (f_day) {
         strcpy (ptr, "CDT");
      } else {
         strcpy (ptr, "CST");
      }
      return 0;
   } else if (TimeZone == 7) {
      if (f_day) {
         strcpy (ptr, "MDT");
      } else {
         strcpy (ptr, "MST");
      }
      return 0;
   } else if (TimeZone == 8) {
      if (f_day) {
         strcpy (ptr, "PDT");
      } else {
         strcpy (ptr, "PST");
      }
      return 0;
   } else if (TimeZone == 9) {
      if (f_day) {
         strcpy (ptr, "YDT");
      } else {
         strcpy (ptr, "YST");
      }
      return 0;
   }
   ptr[0] = '\0';
   return -1;
}

/*****************************************************************************
 * Clock_ScanZone() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Scans a character string to determine the timezone.
 *
 * ARGUMENTS
 *      ptr = The character string to scan. (Input)
 * TimeZone = Hours to add to local time to get UTC. (Output)
 *    f_day = True if we are dealing with daylight savings. (Output)
 *
 * RETURNS: int
 *  0 if we read TimeZone, -1 if not.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_ScanZone2 (char *ptr, sChar *TimeZone, char *f_day)
{
   switch (ptr[0]) {
      case 'G':
         if (strcmp (ptr, "GMT") == 0) {
            *f_day = 0;
            *TimeZone = 0;
            return 0;
         }
         return -1;
      case 'U':
         if (strcmp (ptr, "UTC") == 0) {
            *f_day = 0;
            *TimeZone = 0;
            return 0;
         }
         return -1;
      case 'E':
         if (strcmp (ptr, "EDT") == 0) {
            *f_day = 1;
            *TimeZone = 5;
            return 0;
         } else if (strcmp (ptr, "EST") == 0) {
            *f_day = 0;
            *TimeZone = 5;
            return 0;
         }
         return -1;
      case 'C':
         if (strcmp (ptr, "CDT") == 0) {
            *f_day = 1;
            *TimeZone = 6;
            return 0;
         } else if (strcmp (ptr, "CST") == 0) {
            *f_day = 0;
            *TimeZone = 6;
            return 0;
         }
         return -1;
      case 'M':
         if (strcmp (ptr, "MDT") == 0) {
            *f_day = 1;
            *TimeZone = 7;
            return 0;
         } else if (strcmp (ptr, "MST") == 0) {
            *f_day = 0;
            *TimeZone = 7;
            return 0;
         }
         return -1;
      case 'P':
         if (strcmp (ptr, "PDT") == 0) {
            *f_day = 1;
            *TimeZone = 8;
            return 0;
         } else if (strcmp (ptr, "PST") == 0) {
            *f_day = 0;
            *TimeZone = 8;
            return 0;
         }
         return -1;
      case 'Y':
         if (strcmp (ptr, "YDT") == 0) {
            *f_day = 1;
            *TimeZone = 9;
            return 0;
         } else if (strcmp (ptr, "YST") == 0) {
            *f_day = 0;
            *TimeZone = 9;
            return 0;
         }
         return -1;
      case 'Z':
         if (strcmp (ptr, "Z") == 0) {
            *f_day = 0;
            *TimeZone = 0;
            return 0;
         }
         return -1;
   }
   return -1;
}

/*****************************************************************************
 * Clock_ScanMonth() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Scans a string looking for a month word.  Assumes string is all caps.
 *
 * ARGUMENTS
 * ptr = The character string to scan. (Input)
 *
 * RETURNS: int
 * Returns the month number read, or -1 if no month word seen.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */
int Clock_ScanMonth (char *ptr)
{
   switch (*ptr) {
      case 'A':
         if ((strcmp (ptr, "APR") == 0) || (strcmp (ptr, "APRIL") == 0))
            return 4;
         else if ((strcmp (ptr, "AUG") == 0) || (strcmp (ptr, "AUGUST") == 0))
            return 8;
         return -1;
      case 'D':
         if ((strcmp (ptr, "DEC") == 0) || (strcmp (ptr, "DECEMBER") == 0))
            return 12;
         return -1;
      case 'F':
         if ((strcmp (ptr, "FEB") == 0) || (strcmp (ptr, "FEBRUARY") == 0))
            return 2;
         return -1;
      case 'J':
         if ((strcmp (ptr, "JAN") == 0) || (strcmp (ptr, "JANUARY") == 0))
            return 1;
         else if ((strcmp (ptr, "JUN") == 0) || (strcmp (ptr, "JUNE") == 0))
            return 6;
         else if ((strcmp (ptr, "JUL") == 0) || (strcmp (ptr, "JULY") == 0))
            return 7;
         return -1;
      case 'M':
         if ((strcmp (ptr, "MAR") == 0) || (strcmp (ptr, "MARCH") == 0))
            return 3;
         else if (strcmp (ptr, "MAY") == 0)
            return 5;
         return -1;
      case 'N':
         if ((strcmp (ptr, "NOV") == 0) || (strcmp (ptr, "NOVEMBER") == 0))
            return 11;
         return -1;
      case 'O':
         if ((strcmp (ptr, "OCT") == 0) || (strcmp (ptr, "OCTOBER") == 0))
            return 10;
         return -1;
      case 'S':
         if ((strcmp (ptr, "SEP") == 0) || (strcmp (ptr, "SEPTEMBER") == 0))
            return 9;
         return -1;
   }
   return -1;
}

/*****************************************************************************
 * Clock_PrintMonth3() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 *
 * RETURNS: void
 *
 * HISTORY
 *   3/2005 Arthur Taylor (MDL/RSIS): Commented.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_PrintMonth3 (int mon, char *buffer, CPL_UNUSED int buffLen)
{
   static const char * const MonthName[] = {
      "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT",
      "NOV", "DEC"
   };
   myAssert ((mon > 0) && (mon < 13));
   myAssert (buffLen > 3);
   strcpy (buffer, MonthName[mon - 1]);
}

/*****************************************************************************
 * Clock_PrintMonth() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 *
 * RETURNS: void
 *
 * HISTORY
 *   3/2005 Arthur Taylor (MDL/RSIS): Commented.
 *
 * NOTES
 *****************************************************************************
 */
void Clock_PrintMonth (int mon, char *buffer, CPL_UNUSED int buffLen)
{
   static const char * const MonthName[] = {
      "January", "February", "March", "April", "May", "June", "July",
      "August", "September", "October", "November", "December"
   };
   myAssert ((mon > 0) && (mon < 13));
   myAssert (buffLen > 9);
   strcpy (buffer, MonthName[mon - 1]);
}

/*****************************************************************************
 * Clock_ScanWeekday() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Scans a string looking for a day word.  Assumes string is all caps.
 *
 * ARGUMENTS
 * ptr = The character string to scan. (Input)
 *
 * RETURNS: int
 * Returns the day number read, or -1 if no day word seen.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */

#ifdef unused_by_GDAL
static int Clock_ScanWeekday (char *ptr)
{
   switch (*ptr) {
      case 'S':
         if ((strcmp (ptr, "SUN") == 0) || (strcmp (ptr, "SUNDAY") == 0))
            return 0;
         else if ((strcmp (ptr, "SAT") == 0) ||
                  (strcmp (ptr, "SATURDAY") == 0))
            return 6;
         return -1;
      case 'M':
         if ((strcmp (ptr, "MON") == 0) || (strcmp (ptr, "MONDAY") == 0))
            return 1;
         return -1;
      case 'T':
         if ((strcmp (ptr, "TUE") == 0) || (strcmp (ptr, "TUESDAY") == 0))
            return 2;
         else if ((strcmp (ptr, "THU") == 0) ||
                  (strcmp (ptr, "THURSDAY") == 0))
            return 4;
         return -1;
      case 'W':
         if ((strcmp (ptr, "WED") == 0) || (strcmp (ptr, "WEDNESDAY") == 0))
            return 3;
         return -1;
      case 'F':
         if ((strcmp (ptr, "FRI") == 0) || (strcmp (ptr, "FRIDAY") == 0))
            return 5;
         return -1;
   }
   return -1;
}

/*****************************************************************************
 * Clock_ScanColon() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses a word assuming it is : separated and is dealing with
 * hours:minutes:seconds or hours:minutes.  Returns the resulting time as
 * a double.
 *
 * ARGUMENTS
 * ptr = The character string to scan. (Input)
 *
 * RETURNS: double
 * The time after converting the : separated string.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */

static double Clock_ScanColon (char *ptr)
{
   sInt4 hour, min;
   double sec;
   char *ptr3;

   ptr3 = strchr (ptr, ':');
   if( !ptr3 ) return 0;
   *ptr3 = '\0';
   hour = atoi (ptr);
   *ptr3 = ':';
   ptr = ptr3 + 1;
   /* Check for second :, other wise it is hh:mm */
   if ((ptr3 = strchr (ptr, ':')) == NULL) {
      min = atoi (ptr);
      sec = 0;
   } else {
      *ptr3 = '\0';
      min = atoi (ptr);
      *ptr3 = ':';
      ptr = ptr3 + 1;
      sec = atof (ptr);
   }
   return (sec + 60 * min + 3600 * hour);
}

/*****************************************************************************
 * Clock_ScanSlash() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parses a word assuming it is / separated and is dealing with
 * months/days/years or months/days.
 *
 * ARGUMENTS
 *   word = The character string to scan. (Input)
 *    mon = The month that was seen. (Output)
 *    day = The day that was seen. (Output)
 *   year = The year that was seen. (Output)
 * f_year = True if the year is valid. (Output)
 *
 * RETURNS: int
 * -1 if mon or day is out of range.
 *  0 if no problems.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *   6/2004 AAT (MDL): Updated.
 *
 * NOTES
 *****************************************************************************
 */

static int Clock_ScanSlash (char *word, int *mon, int *day, sInt4 *year,
                            char *f_year)
{
   char *ptr3;
   char *ptr = word;

   ptr3 = strchr (ptr, '/');
   if( !ptr3 ) return -1;
   *ptr3 = '\0';
   *mon = atoi (ptr);
   *ptr3 = '/';
   ptr = ptr3 + 1;
   /* Check for second /, other wise it is mm/dd */
   if ((ptr3 = strchr (ptr, '/')) == NULL) {
      *day = atoi (ptr);
      *year = 1970;
      *f_year = 0;
   } else {
      *ptr3 = '\0';
      *day = atoi (ptr);
      *ptr3 = '/';
      ptr = ptr3 + 1;
      *year = atoi (ptr);
      *f_year = 1;
   }
   if ((*mon < 1) || (*mon > 12) || (*day < 1) || (*day > 31)) {
      printf ("Errors parsing %s\n", word);
      return -1;
   }
   return 0;
}

/* http://www.w3.org/TR/NOTE-datetime
   Year and month:
      YYYY-MM (eg 1997-07)
   Complete date:
      YYYY-MM-DD (eg 1997-07-16)
   Complete date plus hours and minutes:
      YYYY-MM-DDThh:mmTZD (eg 1997-07-16T19:20+01:00)
   Complete date plus hours, minutes and seconds:
      YYYY-MM-DDThh:mm:ssTZD (eg 1997-07-16T19:20:30+01:00)
   Complete date plus hours, minutes, seconds and a decimal fraction of a
second
      YYYY-MM-DDThh:mm:ss.sTZD (eg 1997-07-16T19:20:30.45+01:00)

Example:
1994-11-05T08:15:30-05:00 corresponds to November 5, 1994, 8:15:30 am,
   US Eastern Standard Time.
1994-11-05T13:15:30Z corresponds to the same instant.
*/

static int Clock_ScanDash (char *word, int *mon, int *day, sInt4 *year,
                           double *ptime, char *f_time)
{
   char *ptr3;
   char *ptr = word;
   sInt4 hour, min;
   double sec;
   char temp;
   sInt4 offset;

   ptr3 = strchr (ptr, '-');
   if( !ptr3 ) return -1;
   *ptr3 = '\0';
   *year = atoi (ptr);
   *ptr3 = '-';
   ptr = ptr3 + 1;
   /* Check for second -, other wise it is yyyy-mm */
   if ((ptr3 = strchr (ptr, '-')) == NULL) {
      /* Don't touch time or f_time */
      *mon = atoi (ptr);
      *day = 1;
      if ((*mon < 1) || (*mon > 12)) {
         printf ("Errors parsing %s\n", word);
         return -1;
      }
      return 0;
   }
   *ptr3 = '\0';
   *mon = atoi (ptr);
   *ptr3 = '-';
   ptr = ptr3 + 1;
   if ((ptr3 = strchr (ptr, 'T')) == NULL) {
      /* Don't touch time or f_time */
      *day = atoi (ptr);
      if ((*mon < 1) || (*mon > 12) || (*day < 1) || (*day > 31)) {
         printf ("Errors parsing %s\n", word);
         return -1;
      }
      return 0;
   }
   *ptr3 = '\0';
   *day = atoi (ptr);
   *ptr3 = 'T';
   ptr = ptr3 + 1;
   /* hh:mmTZD */
   /* hh:mm:ssTZD */
   /* hh:mm:ss.sTZD */
   if (strlen (ptr) < 5) {
      printf ("Errors parsing %s\n", word);
      return -1;
   }
   ptr[2] = '\0';
   hour = atoi (ptr);
   ptr[2] = ':';
   ptr += 3;
   offset = 0;
   sec = 0;
   if (strlen (ptr) == 2) {
      min = atoi (ptr);
   } else {
      temp = ptr[2];
      ptr[2] = '\0';
      min = atoi (ptr);
      ptr[2] = temp;
      if (temp == ':') {
         ptr += 3;
         if ((ptr3 = strchr (ptr, '+')) == NULL) {
            if ((ptr3 = strchr (ptr, '-')) == NULL) {
               ptr3 = strchr (ptr, 'Z');
            }
         }
         if (ptr3 == NULL) {
            sec = atof (ptr);
         } else {
            temp = *ptr3;
            *ptr3 = '\0';
            sec = atof (ptr);
            *ptr3 = temp;
            if (temp != 'Z') {
               ptr = ptr3;
               ptr[3] = '\0';
               offset = atoi (ptr) * 3600;
               ptr[3] = ':';
               ptr += 4;
               offset += atoi (ptr) * 60;
            }
         }
      } else if (temp != 'Z') {
         ptr += 2;
         ptr[3] = '\0';
         offset = atoi (ptr) * 3600;
         ptr[3] = ':';
         ptr += 4;
         offset += atoi (ptr) * 60;
      }
   }
   *f_time = 1;
   *ptime = sec + min * 60 + hour * 3600 - offset;
   return 0;
}
#endif // unused_by_GDAL

/*****************************************************************************
 * Clock_ScanDate() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 *
 * RETURNS: void
 *
 * HISTORY
 *   3/2005 Arthur Taylor (MDL/RSIS): Commented.
 *
 * NOTES
 *****************************************************************************
 */
/* prj::slosh prj::stm2trk and prj::degrib use this with l_clock zero'ed
   out, so I have now made sure l_clock is zero'ed. */
void Clock_ScanDate (double *l_clock, sInt4 year, int mon, int day)
{
   int i;
   sInt4 delt, temp, totDay;

   /* Makes sure l_clock is zero'ed out. */
   *l_clock = 0;

   if ((mon < 1) || (mon > 12) || (day < 0) || (day > 31))
      return;
   if( year < -10000 || year > 10000 )
       return;
   totDay = Clock_NumDay (mon, day, year, 0);
   if (day > totDay)
      return;
   totDay = Clock_NumDay (mon, day, year, 1);
   temp = 1970;
   delt = year - temp;
   if ((delt >= 400) || (delt <= -400)) {
      i = (delt / 400);
      temp += 400 * i;
      totDay += 146097L * i;
   }
   if (temp < year) {
      while (temp < year) {
         if (((temp % 4) == 0) &&
             (((temp % 100) != 0) || ((temp % 400) == 0))) {
            if ((temp + 4) < year) {
               totDay += 1461;
               temp += 4;
            } else if ((temp + 3) < year) {
               totDay += 1096;
               temp += 3;
            } else if ((temp + 2) < year) {
               totDay += 731;
               temp += 2;
            } else {
               totDay += 366;
               temp++;
            }
         } else {
            totDay += 365;
            temp++;
         }
      }
   } else if (temp > year) {
      while (temp > year) {
         temp--;
         if (((temp % 4) == 0) &&
             (((temp % 100) != 0) || ((temp % 400) == 0))) {
            if (year < temp - 3) {
               totDay -= 1461;
               temp -= 3;
            } else if (year < (temp - 2)) {
               totDay -= 1096;
               temp -= 2;
            } else if (year < (temp - 1)) {
               totDay -= 731;
               temp--;
            } else {
               totDay -= 366;
            }
         } else {
            totDay -= 365;
         }
      }
   }
   *l_clock = *l_clock + ((double) (totDay)) * 24 * 3600;
}

#ifdef unused_by_GDAL

int Clock_ScanDateNumber (double *l_clock, char *buffer)
{
   int buffLen = (int)strlen (buffer);
   sInt4 year;
   int mon = 1;
   int day = 1;
   int hour = 0;
   int min = 0;
   int sec = 0;
   char c_temp;

   *l_clock = 0;
   if ((buffLen != 4) && (buffLen != 6) && (buffLen != 8) &&
       (buffLen != 10) && (buffLen != 12) && (buffLen != 14)) {
      return 1;
   }
   c_temp = buffer[4];
   buffer[4] = '\0';
   year = atoi (buffer);
   buffer[4] = c_temp;
   if (buffLen > 4) {
      c_temp = buffer[6];
      buffer[6] = '\0';
      mon = atoi (buffer + 4);
      buffer[6] = c_temp;
   }
   if (buffLen > 6) {
      c_temp = buffer[8];
      buffer[8] = '\0';
      day = atoi (buffer + 6);
      buffer[8] = c_temp;
   }
   if (buffLen > 8) {
      c_temp = buffer[10];
      buffer[10] = '\0';
      hour = atoi (buffer + 8);
      buffer[10] = c_temp;
   }
   if (buffLen > 10) {
      c_temp = buffer[12];
      buffer[12] = '\0';
      min = atoi (buffer + 10);
      buffer[12] = c_temp;
   }
   if (buffLen > 12) {
      c_temp = buffer[14];
      buffer[14] = '\0';
      sec = atoi (buffer + 12);
      buffer[14] = c_temp;
   }
   Clock_ScanDate (l_clock, year, mon, day);
   *l_clock = *l_clock + sec + min * 60 + hour * 3600;
   return 0;
}

void Clock_PrintDateNumber (double l_clock, char buffer[15])
{
   sInt4 year;
   int month, day, hour, min, sec;
   double d_sec;

   Clock_PrintDate (l_clock, &year, &month, &day, &hour, &min, &d_sec);
   sec = (int) d_sec;
   snprintf(buffer, 15, "%04d%02d%02d%02d%02d%02d", year, month, day, hour, min,
            sec);
}

/* Word_types: none, ':' word, '/' word, '-' word, integer word, 'AM'/'PM'
 * word, timeZone word, month word, weekDay word, Preceeder to a relativeDate
 * word, Postceeder to a relativeDate word, relativeDate word unit, Adjust Day
 * word
 */
enum {
   WT_NONE, WT_COLON, WT_SLASH, WT_DASH, WT_INTEGER, WT_AMPM, WT_TIMEZONE,
   WT_MONTH, WT_DAY, WT_PRE_RELATIVE, WT_POST_RELATIVE, WT_RELATIVE_UNIT,
   WT_ADJDAY
};

/*****************************************************************************
 * Clock_GetWord() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 *
 * RETURNS: void
 *
 * HISTORY
 *   3/2005 Arthur Taylor (MDL/RSIS): Commented.
 *
 * NOTES
 *****************************************************************************
 */
/* Start at *Start.  Advance Start until it is at first non-space,
 * non-',' non-'.' character.  Move End to first space, ',' or '.' after
 * new Start location.  Copy up to 30 characters (in caps) into word. */
/* return -1 if no next word, 0 otherwise */

static int Clock_GetWord (char **Start, char **End, char word[30],
                          int *wordType)
{
   char *ptr;
   int cnt;
   int f_integer;

   *wordType = WT_NONE;
   if (*Start == NULL) {
      return -1;
   }
   ptr = *Start;
   /* Find start of next word (first non-space non-',' non-'.' char.) */
   while ((*ptr == ' ') || (*ptr == ',') || (*ptr == '.')) {
      ptr++;
   }
   /* There is no next word. */
   if (*ptr == '\0') {
      return -1;
   }
   *Start = ptr;
   /* Find end of next word. */
   cnt = 0;
   f_integer = 1;
   while ((*ptr != ' ') && (*ptr != ',') && (*ptr != '\0')) {
      if (cnt < 29) {
         word[cnt] = (char) toupper (*ptr);
         cnt++;
      }
      if (*ptr == ':') {
         if (*wordType == WT_NONE)
            *wordType = WT_COLON;
         f_integer = 0;
      } else if (*ptr == '/') {
         if (*wordType == WT_NONE)
            *wordType = WT_SLASH;
         f_integer = 0;
      } else if (*ptr == '-') {
         if (ptr != *Start) {
            if (*wordType == WT_NONE)
               *wordType = WT_DASH;
            f_integer = 0;
         }
      } else if (*ptr == '.') {
         if (!isdigit (*(ptr + 1))) {
            break;
         } else {
            f_integer = 0;
         }
      } else if (!isdigit (*ptr)) {
         f_integer = 0;
      }
      ptr++;
   }
   word[cnt] = '\0';
   *End = ptr;
   if (f_integer) {
      *wordType = WT_INTEGER;
   }
   return 0;
}

typedef struct {
   sInt4 val;
   int len;             /* read from len char string? */
} stackType;

typedef struct {
   int relUnit;
   int f_negate;
   int amount;
} relType;

/*****************************************************************************
 * Clock_Scan() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *
 * ARGUMENTS
 *
 * RETURNS: void
 *
 * HISTORY
 *
 * NOTES
 * * f_gmt == 0 no adjust, 1 adjust as LDT, 2 adjust as LST *
 *  Adjusted from:
 * if ((f_gmt == 2) && (Clock_IsDaylightSaving2 (*l_clock, 0) == 1)) {
 * to:
 * if ((f_gmt == 1) && (Clock_IsDaylightSaving2 (*l_clock, 0) == 1)) {

 *****************************************************************************
 */

int Clock_Scan (double *l_clock, char *buffer, char f_gmt)
{
   char *ptr, *ptr2;
   char *ptr3;
   char word[30];
   int wordType;
   int lastWordType;
   sChar TimeZone = Clock_GetTimeZone ();
   /* hours to add to local time to get UTC. */
   char f_dayLight = 0;
   int month = 0;
   int day;
   sInt4 year;
   char f_year = 0;
   int l_index;
   int ans;
   stackType *Stack = NULL;
   relType *Rel = NULL;
   int lenRel = 0;
   int lenStack = 0;
   static const char * const PreRel[] = { "LAST", "THIS", "NEXT", NULL };
   static const char * const RelUnit[] = {
      "YEAR", "YEARS", "MONTH", "MONTHS", "FORTNIGHT", "FORTNIGHTS", "WEEK",
      "WEEKS", "DAY", "DAYS", "HOUR", "HOURS", "MIN", "MINS", "MINUTE",
      "MINUTES", "SEC", "SECS", "SECOND", "SECONDS", NULL
   };
   static const char * const AdjDay[] = { "YESTERDAY", "TODAY", "TOMORROW", NULL };
   sChar f_ampm = -1;
   char f_timeZone = 0;
   char f_time = 0;
   /* char f_date = 0; */
   char f_slashWord = 0;
   char f_dateWord = 0;
   char f_monthWord = 0;
   char f_dayWord = 0;
   double curTime;
   sInt4 sec;
   int i;
   int monthAdj;
   int yearAdj;

   /* Check that they gave us a string */
   ptr = buffer;
   if (*ptr == '\0')
      return 0;

   f_time = 0;
   /* f_date = 0; */
   lastWordType = WT_NONE;
   curTime = 0;
   while (Clock_GetWord (&ptr, &ptr2, word, &wordType) == 0) {
      if (wordType == WT_COLON) {
         if (f_time) {
            printf ("Detected multiple time pieces\n");
            goto errorReturn;
         }
         curTime = Clock_ScanColon (word);
         f_time = 1;
      } else if (wordType == WT_SLASH) {
         if ((f_slashWord) || (f_dateWord)) {
            printf ("Detected multiple date pieces\n");
            goto errorReturn;
         }
         Clock_ScanSlash (word, &month, &day, &year, &f_year);
         f_slashWord = 1;
      } else if (wordType == WT_DASH) {
         if ((f_slashWord) || (f_dateWord)) {
            printf ("Detected multiple date pieces\n");
            goto errorReturn;
         }
         Clock_ScanDash (word, &month, &day, &year, &curTime, &f_time);
         f_year = 1;
         f_slashWord = 1;
         TimeZone = 0;
      } else if (wordType == WT_INTEGER) {
         lenStack++;
         Stack = (stackType *) realloc ((void *) Stack,
                                        lenStack * sizeof (stackType));
         Stack[lenStack - 1].val = atoi (word);
         Stack[lenStack - 1].len = (int)strlen (word);
      } else if (strcmp (word, "AM") == 0) {
         if (f_ampm != -1) {
            printf ("Detected multiple am/pm\n");
            goto errorReturn;
         }
         f_ampm = 1;
         wordType = WT_AMPM;
      } else if (strcmp (word, "PM") == 0) {
         if (f_ampm != -1) {
            printf ("Detected multiple am/pm\n");
            goto errorReturn;
         }
         f_ampm = 2;
         wordType = WT_AMPM;
      } else if (Clock_ScanZone2 (word, &TimeZone, &f_dayLight) == 0) {
         if (f_timeZone) {
            printf ("Detected multiple time zones.\n");
            goto errorReturn;
         }
         if (f_dayLight == 0) {
            f_gmt = 2;
         } else {
            f_gmt = 1;
         }
         f_timeZone = 1;
         wordType = WT_TIMEZONE;
      } else if ((l_index = Clock_ScanMonth (word)) != -1) {
         if ((f_slashWord) || (f_monthWord)) {
            printf ("Detected multiple months or already defined month.\n");
            goto errorReturn;
         }
         month = l_index;
         /* Get the next word? First preserve the pointer */
         ptr3 = ptr2;
         ptr = ptr2;
         ans = Clock_GetWord (&ptr, &ptr2, word, &wordType);
         if ((ans != 0) || (wordType != WT_INTEGER)) {
            /* Next word not integer, so previous word is integral day. */
            if (lastWordType != WT_INTEGER) {
               printf ("Problems with month word and finding the day.\n");
               goto errorReturn;
            }
            lenStack--;
            if( Stack ) day = Stack[lenStack].val;
            /* Put the next word back under consideration. */
            wordType = WT_MONTH;
            ptr2 = ptr3;
         } else {
            /* If word is trailed by comma, then it is day, and the next one
             * is the year, otherwise it is a year, and the number before the
             * month is the day.  */
            if (*ptr2 == ',') {
               day = atoi (word);
               ptr = ptr2;
               ans = Clock_GetWord (&ptr, &ptr2, word, &wordType);
               if ((ans != 0) || (wordType != WT_INTEGER)) {
                  printf ("Couldn't find the year after the day.\n");
                  goto errorReturn;
               }
               year = atoi (word);
               f_year = 1;
            } else {
               year = atoi (word);
               f_year = 1;
               if (lastWordType != WT_INTEGER) {
                  printf ("Problems with month word and finding the day.\n");
                  goto errorReturn;
               }
               lenStack--;
               if( Stack ) day = Stack[lenStack].val;
            }
         }
         f_monthWord = 1;
         f_dateWord = 1;

         /* Ignore the day of the week info? */
      } else if ((l_index = Clock_ScanWeekday (word)) != -1) {
         if ((f_slashWord) || (f_dayWord)) {
            printf ("Detected multiple day of week or already defined "
                    "day.\n");
            goto errorReturn;
         }
         wordType = WT_DAY;
         f_dayWord = 1;
         f_dateWord = 1;
      } else if (GetIndexFromStr (word, PreRel, &l_index) != -1) {
         wordType = WT_PRE_RELATIVE;
         /* Next word must be a unit word. */
         ptr = ptr2;
         if (Clock_GetWord (&ptr, &ptr2, word, &wordType) != 0) {
            printf ("Couldn't get the next word after Pre-Relative time "
                    "word\n");
            goto errorReturn;
         }
         if (GetIndexFromStr (word, RelUnit, &ans) == -1) {
            printf ("Couldn't get the Relative unit\n");
            goto errorReturn;
         }
         if (l_index != 1) {
            lenRel++;
            Rel = (relType *) realloc ((void *) Rel,
                                       lenRel * sizeof (relType));
            Rel[lenRel - 1].relUnit = ans;
            Rel[lenRel - 1].amount = 1;
            if (l_index == 0) {
               Rel[lenRel - 1].f_negate = 1;
            } else {
               Rel[lenRel - 1].f_negate = 0;
            }
         }
         printf ("Pre Relative Word: %s %d\n", word, l_index);

      } else if (strcmp (word, "AGO") == 0) {
         if ((lastWordType != WT_PRE_RELATIVE) &&
             (lastWordType != WT_RELATIVE_UNIT)) {
            printf ("Ago did not follow relative words\n");
            goto errorReturn;
         }
         Rel[lenRel - 1].f_negate = 1;
         wordType = WT_POST_RELATIVE;
      } else if (GetIndexFromStr (word, RelUnit, &l_index) != -1) {
         lenRel++;
         Rel = (relType *) realloc ((void *) Rel, lenRel * sizeof (relType));
         Rel[lenRel - 1].relUnit = l_index;
         Rel[lenRel - 1].amount = 1;
         Rel[lenRel - 1].f_negate = 0;
         if (lastWordType == WT_INTEGER && Stack) {
            lenStack--;
            Rel[lenRel - 1].amount = Stack[lenStack].val;
         }
         wordType = WT_RELATIVE_UNIT;
      } else if (GetIndexFromStr (word, AdjDay, &l_index) != -1) {
         if (l_index != 1) {
            lenRel++;
            Rel = (relType *) realloc ((void *) Rel,
                                       lenRel * sizeof (relType));
            Rel[lenRel - 1].relUnit = 13; /* DAY in RelUnit list */
            Rel[lenRel - 1].amount = 1;
            if (l_index == 0) {
               Rel[lenRel - 1].f_negate = 1;
            } else {
               Rel[lenRel - 1].f_negate = 0;
            }
         }
         wordType = WT_ADJDAY;
      } else {
         printf ("unknown: %s\n", word);
         goto errorReturn;
      }
      ptr = ptr2;
      lastWordType = wordType;
   }

   /* Deal with time left on the integer stack. */
   if (lenStack > 1) {
      printf ("Too many integers on the stack?\n");
      goto errorReturn;
   }
   if (lenStack == 1) {
      if (Stack[0].val < 0) {
         printf ("Unable to deduce a negative time?\n");
         goto errorReturn;
      }
      if (f_time) {
         if (f_dateWord || f_slashWord) {
            printf ("Already have date and time...\n");
            goto errorReturn;
         }
         if ((Stack[0].len == 6) || (Stack[0].len == 8)) {
            year = Stack[0].val / 10000;
            f_year = 1;
            month = (Stack[0].val % 10000) / 100;
            day = Stack[0].val % 100;
            f_slashWord = 1;
            if ((month < 1) || (month > 12) || (day < 1) || (day > 31)) {
               printf ("Unable to deduce the integer value\n");
               return -1;
            }
         } else {
            printf ("Unable to deduce the integer value\n");
            goto errorReturn;
         }
      } else {
         if (Stack[0].len < 3) {
            curTime = Stack[0].val * 3600;
            f_time = 1;
         } else if (Stack[0].len < 5) {
            curTime = ((Stack[0].val / 100) * 3600. +
                       (Stack[0].val % 100) * 60.);
            f_time = 1;
         } else if ((Stack[0].len == 6) || (Stack[0].len == 8)) {
            year = Stack[0].val / 10000;
            f_year = 1;
            month = (Stack[0].val % 10000) / 100;
            day = Stack[0].val % 100;
            f_slashWord = 1;
            if ((month < 1) || (month > 12) || (day < 1) || (day > 31)) {
               printf ("Unable to deduce the integer value\n");
               free( Rel );
               free( Stack );
               return -1;
            }
         } else {
            printf ("Unable to deduce the time\n");
            goto errorReturn;
         }
      }
      /*lenStack = 0;*/
   }
   if (!f_time) {
      if (f_ampm != -1) {
         printf ("Problems setting the time to 0\n");
         goto errorReturn;
      }
      curTime = 0;
   }
   if (f_ampm == 1) {
      /* Adjust for 12 am */
      sec = (sInt4) (curTime - (floor (curTime / SEC_DAY)) * SEC_DAY);
      if (((sec % 43200L) / 3600) == 0) {
         curTime -= 43200L;
      }
   } else if (f_ampm == 2) {
      /* Adjust for 12 pm */
      curTime += 43200L;
      sec = (sInt4) (curTime - (floor (curTime / SEC_DAY)) * SEC_DAY);
      if (((sec % 43200L) / 3600) == 0) {
         curTime -= 43200L;
      }
   }
   for (i = 0; i < lenRel; i++) {
      if (Rel[i].f_negate) {
         Rel[i].amount = -1 * Rel[i].amount;
      }
   }
   /* Deal with adjustments by year or month. */
   if (f_dateWord || f_slashWord) {
      /* Check if we don't have the year. */
      if (!f_year) {
         *l_clock = Clock_Seconds ();
         Clock_Epoch2YearDay ((sInt4) (floor (*l_clock / SEC_DAY)), &i, &year);
      }
      /* Deal with relative adjust by year and month. */
      for (i = 0; i < lenRel; i++) {
         if ((Rel[i].relUnit == 0) || (Rel[i].relUnit == 1)) {
            year += Rel[i].amount;
         } else if ((Rel[i].relUnit == 2) || (Rel[i].relUnit == 3)) {
            month += Rel[i].amount;
         }
      }
      if (month > 12) {
         int incrYearDueToMonth = (month-1) / 12;
         year += incrYearDueToMonth;
         month -= 12 * incrYearDueToMonth;
      }
      else if( month <= 0) {
         int incrYearDueToMonth = (month-12) / 12;
         year += incrYearDueToMonth;
         month -= 12 * incrYearDueToMonth;
      }
      *l_clock = 0;
      Clock_ScanDate (l_clock, year, month, day);

   } else {
      /* Pure Time words. */
      *l_clock = Clock_Seconds ();
      /* round off to start of day */
      *l_clock = (floor (*l_clock / SEC_DAY)) * SEC_DAY;
      /* Deal with relative adjust by year and month. */
      monthAdj = 0;
      yearAdj = 0;
      for (i = 0; i < lenRel; i++) {
         if ((Rel[i].relUnit == 0) || (Rel[i].relUnit == 1)) {
            if (Rel[i].f_negate) {
               yearAdj -= Rel[i].amount;
            } else {
               yearAdj += Rel[i].amount;
            }
         } else if ((Rel[i].relUnit == 2) || (Rel[i].relUnit == 3)) {
            if (Rel[i].f_negate) {
               monthAdj -= Rel[i].amount;
            } else {
               monthAdj += Rel[i].amount;
            }
         }
      }
      if ((monthAdj != 0) || (yearAdj != 0)) {
         /* Break l_clock into mon/day/year */
         Clock_Epoch2YearDay ((sInt4) (floor (*l_clock / SEC_DAY)),
                              &day, &year);
         month = Clock_MonthNum (day, year);
         day -= (Clock_NumDay (month, 1, year, 1) - 1);
         month += monthAdj;
         year += yearAdj;
         if (month > 12) {
            int incrYearDueToMonth = (month-1) / 12;
            year += incrYearDueToMonth;
            month -= 12 * incrYearDueToMonth;
         }
         else if( month <= 0) {
            int incrYearDueToMonth = (month-12) / 12;
            year += incrYearDueToMonth;
            month -= 12 * incrYearDueToMonth;
         }
         *l_clock = 0;
         Clock_ScanDate (l_clock, year, month, day);
      }
   }

   /* Join the date and the time. */
   *l_clock += curTime;

   /* Finish the relative adjustments. */
   for (i = 0; i < lenRel; i++) {
      switch (Rel[i].relUnit) {
         case 3:       /* Fortnight. */
         case 4:
            *l_clock += (Rel[i].amount * 14 * 24 * 3600.);
            break;
         case 5:       /* Week. */
         case 6:
            *l_clock += (Rel[i].amount * 7 * 24 * 3600.);
            break;
         case 7:       /* Day. */
         case 8:
            *l_clock += (Rel[i].amount * 24 * 3600.);
            break;
         case 9:       /* Hour. */
         case 10:
            *l_clock += (Rel[i].amount * 3600.);
            break;
         case 11:      /* Minute. */
         case 12:
         case 13:
         case 14:
            *l_clock += (Rel[i].amount * 60.);
            break;
         case 15:      /* Second. */
         case 16:
         case 17:
         case 18:
            *l_clock += Rel[i].amount;
            break;
      }
   }

   if (f_gmt != 0) {
      /* IsDaylightSaving takes l_clock in GMT, and Timezone. */
      /* Note: A 0 is passed to DaylightSavings so it converts from LST to
       * LST. */
      if ((f_gmt == 1) && (Clock_IsDaylightSaving2 (*l_clock, 0) == 1)) {
         *l_clock = *l_clock - 3600;
      }
      /* Handle gmt problems. We are going from Local time to GMT so we add
       * the TimeZone here. */
      *l_clock = *l_clock + TimeZone * 3600;
   }

   free (Stack);
   free (Rel);
   return 0;

 errorReturn:
   free (Stack);
   free (Rel);
   return -1;
}

#endif // unused_by_GDAL

double Clock_AddMonthYear (double refTime, int incrMonth, int incrYear)
{
   sInt4 totDay;
   int day;
   sInt4 year;
   int month;
   double d_remain;
   int i;

   if( !(fabs(refTime) < (double)SEC_DAY * 365 * 10000) )
   {
       fprintf(stderr, "invalid refTime = %f\n", refTime);
       return 0;
   }

   totDay = (sInt4) floor (refTime / SEC_DAY);
   Clock_Epoch2YearDay (totDay, &day, &year);
   month = Clock_MonthNum (day, year);
   day = day - Clock_NumDay (month, 1, year, 1) + 1;
   d_remain = refTime - (double)totDay * 3600 * 24.0;

   /* Add the month */
   if (incrMonth != 0) {
      if( incrMonth > 0 && month > INT_MAX - incrMonth )
      {
          fprintf(stderr, "invalid incrMonth = %d\n", incrMonth);
          return 0;
      }
      if( incrMonth < 0 && month < INT_MIN-(-12) - incrMonth )
      {
          fprintf(stderr, "invalid incrMonth = %d\n", incrMonth);
          return 0;
      }
      month += incrMonth;
      if (month > 12) {
         int incrYearDueToMonth = (month-1) / 12;
         year += incrYearDueToMonth;
         month -= 12 * incrYearDueToMonth;
      }
      else if( month <= 0) {
         int incrYearDueToMonth = (month-12) / 12;
         year += incrYearDueToMonth;
         month -= 12 * incrYearDueToMonth;
      }
   }
   /* Add the year. */
   if (incrYear != 0) {
      if (incrYear > 0 && year > INT_MAX - incrYear) {
         fprintf(stderr, "overflow. year: %d incrYear: %d\n", year, incrYear);
         return 0;
      }
      if (incrYear < 0 && year < INT_MIN - incrYear) {
         fprintf(stderr, "overflow. year: %d incrYear: %d\n", year, incrYear);
         return 0;
      }
      year += incrYear;
   }

   /* Recompose the date */
   i = Clock_NumDay (month, 1, year, 0);
   if (day > i) {
      day = i;
   }
   refTime = 0;
   Clock_ScanDate (&refTime, year, month, day);
   refTime += d_remain;
   return refTime;
}

#ifdef CLOCK_PROGRAM
/* See clockstart.c */
#endif
