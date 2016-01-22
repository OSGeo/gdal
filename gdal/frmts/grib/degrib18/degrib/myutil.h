/*****************************************************************************
 * myutil.c
 *
 * DESCRIPTION
 *    This file contains some simple utility functions.
 *
 * HISTORY
 *   12/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef MYUTIL_H
#define MYUTIL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <time.h>
#include "type.h"

size_t reallocFGets (char **Ptr, size_t *LenBuff, FILE * fp);

void mySplit (const char *data, char symbol, size_t *Argc, char ***Argv,
              char f_trim);

int myAtoI (const char *ptr, sInt4 *value);

int myAtoF (const char *ptr, double *value);
/* Change of name was to deprecate usage... Switch to myAtoF */
int myIsReal_old (const char *ptr, double *value);

#define MYSTAT_ISFILE 2
#define MYSTAT_ISDIR 1
int myStat (char *filename, char *perm, sInt4 *size, double *mtime);

int myGlob (const char *dirName, const char *filter, size_t * Argc,
            char ***Argv);

int FileCopy (const char *fileIn, const char *fileOut);

void FileTail (const char *fileName, char **tail);

double myRound (double data, uChar place);

void strTrim (char *str);

void strTrimRight (char *str, char c);

void strCompact (char *str, char c);

void strReplace (char *str, char c1, char c2);

void strToUpper (char *str);

void strToLower (char *str);

int strcmpNoCase (const char *str1, const char *str2);

int GetIndexFromStr (const char *str, char **Opt, int *Index);

/* Rename because changed error return from -1 to 1 */
/* Rename because trying to phase it out. */
int myParseTime3 (const char *is, time_t * AnsTime);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* MYUTIL_H */
