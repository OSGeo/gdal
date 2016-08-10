/*****************************************************************************
 * myerror.h
 *
 * DESCRIPTION
 *    This file contains the code to handle error messages.  Instead of simply
 * printing the error to stdio, it allocates some memory and stores the
 * message in it.  This is so that one can pass the error message back to
 * Tcl/Tk or another GUI program when there is no stdio.
 *    In addition a version of sprintf is provided which allocates memory for
 * the calling routine, so that one doesn't have to guess the maximum bounds
 * of the message.
 *
 * HISTORY
 *    9/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef MYERROR_H
#define MYERROR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include "type.h"

void mallocSprintf (char **Ptr, const char *fmt, ...);

void reallocSprintf (char **Ptr, const char *fmt, ...);

/* If fmt == NULL return buffer and reset, otherwise add. */
/* You are responsible for free'ing the result of errSprintf(NULL). */
char *errSprintf (const char *fmt, ...);

void preErrSprintf (const char *fmt, ...);

int myWarnRet (uChar f_errCode, int appErrCode, const char *file,
               int lineNum, const char *fmt, ...);

void myWarnSet (uChar f_outType, uChar f_detail, uChar f_fileDetail,
                FILE * warnFile);

/* You are responsible for free'ing the result of myWarnClear. */
sChar myWarnClear (char **msg, uChar f_closeFile);

uChar myWarnNotEmpty (void);

sChar myWarnLevel (void);

/* Use myWarnQ# for (quiet file/line) notes. */
/* Use myWarnN# for notes. */
/* Use myWarnW# for warnings. */
/* Use myWarnE# for errors. */
#define myWarnQ1(f) myWarnRet(0, f, NULL, __LINE__, NULL)
#define myWarnN1(f) myWarnRet(0, f, __FILE__, __LINE__, NULL)
#define myWarnW1(f) myWarnRet(1, f, __FILE__, __LINE__, NULL)
#define myWarnE1(f) myWarnRet(2, f, __FILE__, __LINE__, NULL)
#define myWarnPQ1(f) myWarnRet(3, f, NULL, __LINE__, NULL)
#define myWarnPN1(f) myWarnRet(3, f, __FILE__, __LINE__, NULL)
#define myWarnPW1(f) myWarnRet(4, f, __FILE__, __LINE__, NULL)
#define myWarnPE1(f) myWarnRet(5, f, __FILE__, __LINE__, NULL)

#define myWarnQ2(f,g) myWarnRet(0, f, NULL, __LINE__, g)
#define myWarnN2(f,g) myWarnRet(0, f, __FILE__, __LINE__, g)
#define myWarnW2(f,g) myWarnRet(1, f, __FILE__, __LINE__, g)
#define myWarnE2(f,g) myWarnRet(2, f, __FILE__, __LINE__, g)
#define myWarnPQ2(f,g) myWarnRet(3, f, NULL, __LINE__, g)
#define myWarnPN2(f,g) myWarnRet(3, f, __FILE__, __LINE__, g)
#define myWarnPW2(f,g) myWarnRet(4, f, __FILE__, __LINE__, g)
#define myWarnPE2(f,g) myWarnRet(5, f, __FILE__, __LINE__, g)

#define myWarnQ3(f,g,h) myWarnRet(0, f, NULL, __LINE__, g, h)
#define myWarnN3(f,g,h) myWarnRet(0, f, __FILE__, __LINE__, g, h)
#define myWarnW3(f,g,h) myWarnRet(1, f, __FILE__, __LINE__, g, h)
#define myWarnE3(f,g,h) myWarnRet(2, f, __FILE__, __LINE__, g, h)
#define myWarnPQ3(f,g,h) myWarnRet(3, f, NULL, __LINE__, g, h)
#define myWarnPN3(f,g,h) myWarnRet(3, f, __FILE__, __LINE__, g, h)
#define myWarnPW3(f,g,h) myWarnRet(4, f, __FILE__, __LINE__, g, h)
#define myWarnPE3(f,g,h) myWarnRet(5, f, __FILE__, __LINE__, g, h)

#define myWarnQ4(f,g,h,ff) myWarnRet(0, f, NULL, __LINE__, g, h, ff)
#define myWarnN4(f,g,h,ff) myWarnRet(0, f, __FILE__, __LINE__, g, h, ff)
#define myWarnW4(f,g,h,ff) myWarnRet(1, f, __FILE__, __LINE__, g, h, ff)
#define myWarnE4(f,g,h,ff) myWarnRet(2, f, __FILE__, __LINE__, g, h, ff)
#define myWarnPQ4(f,g,h,ff) myWarnRet(3, f, NULL, __LINE__, g, h, ff)
#define myWarnPN4(f,g,h,ff) myWarnRet(3, f, __FILE__, __LINE__, g, h, ff)
#define myWarnPW4(f,g,h,ff) myWarnRet(4, f, __FILE__, __LINE__, g, h, ff)
#define myWarnPE4(f,g,h,ff) myWarnRet(5, f, __FILE__, __LINE__, g, h, ff)

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* MYERROR_H */
