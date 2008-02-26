#ifndef CLOCK_H
#define CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "type.h"

#define PERIOD_YEARS 146097L
#define SEC_DAY 86400L
#define ISLEAPYEAR(y) (((y)%400 == 0) || (((y)%4 == 0) && ((y)%100 != 0)))

void Clock_Epoch2YearDay (sInt4 totDay, int *Day, sInt4 * Yr);
int Clock_MonthNum (int day, sInt4 year);
int Clock_NumDay (int month, int day, sInt4 year, char f_tot);
sChar Clock_GetTimeZone ();
int Clock_IsDaylightSaving2 (double clock, sChar TimeZone);
void Clock_PrintDate (double clock, sInt4 *year, int *month, int *day,
                      int *hour, int *min, double *sec);
void Clock_Print (char *buffer, int n, double clock, const char *format,
                  char f_gmt);
void Clock_Print2 (char *buffer, int n, double clock, char *format,
                   sChar timeZone, sChar f_dayCheck);
double Clock_Clicks (void);
int Clock_SetSeconds (double *time, sChar f_set);
double Clock_Seconds (void);
int Clock_PrintZone2 (char *ptr, sChar TimeZone, char f_day);
int Clock_ScanZone2 (char *ptr, sChar * TimeZone, char *f_day);
int Clock_ScanMonth (char *ptr);
void Clock_PrintMonth3 (int mon, char *buffer, int buffLen);
void Clock_PrintMonth (int mon, char *buffer, int buffLen);
void Clock_ScanDate (double *clock, sInt4 year, int mon, int day);
int Clock_ScanDateNumber (double *clock, char *buffer);
void Clock_PrintDateNumber (double clock, char buffer[15]);
int Clock_Scan (double *clock, char *buffer, char f_gmt);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* CLOCK_H */
