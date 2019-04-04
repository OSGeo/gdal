/*****************************************************************************
 * myerror.c
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
 *  9/2002 Arthur Taylor (MDL / RSIS): Created.
 * 12/2002 Rici Yu, Fangyu Chi, Mark Armstrong, & Tim Boyer
 *         (RY,FC,MA,&TB): Code Review 2.
 * 12/2005 AAT Added myWarn routines.
 *
 * NOTES
 *   See Kernighan & Ritchie C book (2nd edition) page 156.
 *****************************************************************************
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "myassert.h"
#include "myerror.h"
#ifdef MEMWATCH
#include "memwatch.h"
#endif

/*****************************************************************************
 * AllocSprintf() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   Based on minprintf (see K&R C book (2nd edition) page 156.  This code
 * tries to provide some of the functionality of sprintf, while at the same
 * time it handles the memory allocation.
 *   In addition, it provides a %S option, which allows one to pass in an
 * array of strings, and get back a comma delimited string.
 *
 * ARGUMENTS
 *     Ptr = An array of data that is of size LenBuff. (Input/Output)
 * LenBuff = The allocated length of Ptr. (Input/Output)
 *     fmt = Format similar to the one used by sprintf to define how to
 *           print the message (Input)
 *      ap = argument list initialized by a call to va_start.  Contains the
 *           data needed by fmt. (Input)
 *
 * RETURNS: void
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 * 12/2002 AAT: Fixed the mallocSprintf ("") error.
 *  2/2003 AAT: increased bufpart[80] to bufpart[330] because the largest
 *         64 bit double is: +1.7E+308, and I want 20 "slots" for stuff
 *         after the decimal place. There is the possibility of "Long
 *         doubles" (80 bits) which would have a max of: +3.4E+4932, but
 *         that is excessive for now.
 *  2/2004 AAT: if lenBuff != 0, switch from ipos-- to strlen (buffer);
 *  3/2004 AAT: Added %c option.
 * 11/2005 AAT: Added %e option.
 *  1/2006 AAT: Found a bug with multiple errSprintf.  Doesn't seem to be
 *              able to handle lenBuff > strlen(buffer) when procedure is
 *              first called.  Something like format = "aaa%s", lenBuff = 3,
 *              buff = 'n' would result in 'naaa__<string>', instead of
 *              'naaa<string>'.  Simple solution set lenBuff = strlen (buff).
 *              better solution: Maybe calculate correct place for ipos
 *              before switch.
 *
 * NOTES
 * Supported formats:
 *  %0.4f => float, double
 *  %03d %ld %10ld => int, sInt4.
 *  %s => Null terminated char string. (no range specification)
 *  %S => take a char ** and turn it into a comma delimited string.
 *
 * Assumes that no individual float or int will be more than 80 characters
 * Assumes that no % option is more than 20 char.
 *****************************************************************************
 */
static void AllocSprintf (char **Ptr, size_t *LenBuff, const char *fmt,
                          va_list ap)
{
   char *buffer = *Ptr; /* Local copy of Ptr. */
   size_t lenBuff = *LenBuff; /* Local copy of LenBuff. */
   const char *p;       /* Points to % char in % option. */
   const char *p1;      /* Points to end of % option. */
   char bufpart[330];   /* Used for formatting the int / float options. */
   char format[20];     /* Used to store the % option. */
   char *sval;          /* For pulling strings off va_list. */
   char **Sval;         /* For pulling lists of strings off va_list. */
   size_t slen;         /* Length of used part of temp. */
   char f_inLoop;       /* Flag to state whether we got into %S , loop. */
   char flag;           /* If they have a l,L,h in string. */
   /* size_t ipos = *LenBuff; *//* The current index to start storing data. */
   size_t ipos;         /* The current index to start storing data. */
   int c_type;          /* Used when handling %c option. */

   myAssert (sizeof (char) == 1);

   if ((fmt == NULL) || (strlen (fmt) == 0)) {
      return;
   }
   p = fmt;
   /* If lenBuff = 0, then make room for the '\0' character. */
   if (lenBuff == 0) {
      lenBuff++;
      buffer = (char *) realloc ((void *) buffer, lenBuff);
      /* Added following 1 line on 1/2006 */
      ipos = 0;
   } else {
      /* Added following 3 lines on 1/2006 */
      myAssert (lenBuff >= strlen (buffer) + 1);
      lenBuff = strlen (buffer) + 1;
      ipos = lenBuff - 1;
/*     ipos = strlen (buffer); */
   }
   while (p < fmt + strlen (fmt)) {
      p1 = p;
      p = strchr (p1, '%');
      /* Handle simple case when no more % in format string. */
      if (p == NULL) {
         /* No more format strings; copy rest of format and return */
         lenBuff += strlen (p1);
         buffer = (char *) realloc ((void *) buffer, lenBuff);
         strcpy (buffer + ipos, p1);
         goto done;
      }
      /* Handle data up to the current % in format string. */
      lenBuff += p - p1;
      buffer = (char *) realloc ((void *) buffer, lenBuff);
      strncpy (buffer + ipos, p1, p - p1);
      ipos = lenBuff - 1;
      /* Start dealing with % of format. */
      p1 = p + strspn (p + 1, "0123456789.");
      p1++;
      /* p1 points to first letter after %. */
      switch (*p1) {
         case 'h':
         case 'l':
         case 'L':
            flag = *p1;
            p1++;
            break;
         case '\0':
            /* Handle improper use of '%' for example: '%##' */
            lenBuff += p1 - p - 1;
            buffer = (char *) realloc ((void *) buffer, lenBuff);
            strncpy (buffer + ipos, p + 1, p1 - p - 1);
            goto done;
         default:
            flag = ' ';
      }
      if ((p1 - p + 1) > (int) (sizeof (format)) - 1) {
         /* Protect against overflow of format string. */
         lenBuff += p1 - p + 1;
         buffer = (char *) realloc ((void *) buffer, lenBuff);
         strncpy (buffer + ipos, p, p1 - p + 1);
         ipos = lenBuff - 1;
      } else {
         strncpy (format, p, p1 - p + 1);
         format[p1 - p + 1] = '\0';
         switch (*p1) {
            case 'd':
               switch (flag) {
                  case 'l':
                  case 'L':
                     sprintf (bufpart, format, va_arg (ap, sInt4));
                     break;
                     /*
                      * gcc warning for 'h': "..." promotes short int to
                      * int.  Could get rid of 'h' option but decided to
                      * leave it in since we might have a different
                      * compiler.
                      */
/*
              case 'h':
                sprintf (bufpart, format, va_arg(ap, short int));
                break;
*/
                  default:
                     sprintf (bufpart, format, va_arg (ap, int));
               }
               slen = strlen (bufpart);
               lenBuff += slen;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               memcpy (buffer + ipos, bufpart, slen);
               ipos = lenBuff - 1;
               break;
            case 'f':
               sprintf (bufpart, format, va_arg (ap, double));
               slen = strlen (bufpart);
               lenBuff += slen;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               memcpy (buffer + ipos, bufpart, slen);
               ipos = lenBuff - 1;
               break;
            case 'e':
               sprintf (bufpart, format, va_arg (ap, double));
               slen = strlen (bufpart);
               lenBuff += slen;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               memcpy (buffer + ipos, bufpart, slen);
               ipos = lenBuff - 1;
               break;
            case 'g':
               sprintf (bufpart, format, va_arg (ap, double));
               slen = strlen (bufpart);
               lenBuff += slen;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               memcpy (buffer + ipos, bufpart, slen);
               ipos = lenBuff - 1;
               break;
            case 'c':
               c_type = va_arg (ap, int);
               lenBuff += 1;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               buffer[ipos] = (char) c_type;
               buffer[ipos + 1] = '\0';
               ipos = lenBuff - 1;
               break;
            case 's':
               if ((p1 - p) == 1) {
                  sval = va_arg (ap, char *);
/*    printf (":: sval :: '%s'\n", sval);*/
                  slen = strlen (sval);
                  lenBuff += slen;
                  buffer = (char *) realloc ((void *) buffer, lenBuff);
                  memcpy (buffer + ipos, sval, slen);
                  ipos = lenBuff - 1;
                  break;
               }
               /* Intentionally fall through. */
            case 'S':
               if ((p1 - p) == 1) {
                  f_inLoop = 0;
                  for (Sval = va_arg (ap, char **); *Sval; Sval++) {
                     slen = strlen (*Sval);
                     lenBuff += slen + 1;
                     buffer = (char *) realloc ((void *) buffer, lenBuff);
                     strcpy (buffer + ipos, *Sval);
                     strcat (buffer + ipos + slen, ",");
                     ipos = lenBuff - 1;
                     f_inLoop = 1;
                  }
                  if (f_inLoop) {
                     lenBuff--;
                     buffer[lenBuff] = '\0';
                     ipos = lenBuff - 1;
                  }
                  break;
               }
               /* Intentionally fall through. */
            default:
               lenBuff += p1 - p;
               buffer = (char *) realloc ((void *) buffer, lenBuff);
               strncpy (buffer + ipos, p + 1, p1 - p);
               ipos = lenBuff - 1;
         }
      }
      p = p1 + 1;
   }
 done:
   buffer[lenBuff - 1] = '\0';
   *Ptr = buffer;
   *LenBuff = lenBuff;
}

/*****************************************************************************
 * mallocSprintf() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   This is a front end for AllocSprintf, when you want to malloc memory.
 * In other words when the pointer is not pointing to anything in particular.
 * It allocates the memory, prints the message, and then sets Ptr to point to
 * it.
 *
 * ARGUMENTS
 * Ptr = Place to point to new memory which contains the message (Output)
 * fmt = Format similar to the one used by sprintf to define how to print the
 *       message (Input)
 *
 * RETURNS: void
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 * Supported formats:  See AllocSprintf
 *****************************************************************************
 */
void mallocSprintf (char **Ptr, const char *fmt, ...)
{
   va_list ap;          /* Contains the data needed by fmt. */
   size_t buff_len = 0; /* Allocated length of buffer. */

   *Ptr = NULL;
   if (fmt != NULL) {
      va_start (ap, fmt); /* make ap point to 1st unnamed arg. */
      AllocSprintf (Ptr, &buff_len, fmt, ap);
      va_end (ap);      /* clean up when done. */
   }
}

/*****************************************************************************
 * reallocSprintf() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   This is a front end for AllocSprintf, when you want to realloc memory.
 * In other words, the pointer is pointing to NULL, or to some memory that
 * you want to tack a message onto the end of.  It allocates extra memory,
 * and prints the message.
 *
 *   KEY WORDS: "Tack a message onto the end of"
 *
 * ARGUMENTS
 * Ptr = Pointer to memory to add the message to. (Input/Output)
 * fmt = Format similar to the one used by sprintf to define how to print the
 *       message (Input)
 *
 * RETURNS: void
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 * Supported formats:  See AllocSprintf
 *****************************************************************************
 */
void reallocSprintf (char **Ptr, const char *fmt, ...)
{
   va_list ap;          /* Contains the data needed by fmt. */
   size_t buff_len;     /* Allocated length of buffer. */

   if (fmt != NULL) {
      va_start (ap, fmt); /* make ap point to 1st unnamed arg. */
      if (*Ptr == NULL) {
         buff_len = 0;
      } else {
         buff_len = strlen (*Ptr) + 1;
      }
      AllocSprintf (Ptr, &buff_len, fmt, ap);
      va_end (ap);      /* clean up when done. */
   }
}

/*****************************************************************************
 * errSprintf() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   This uses AllocSprintf to generate a message, which it stores in a static
 * variable.  If it is called with a (NULL), it returns the built up message,
 * and resets its pointer to NULL.  The idea being that errors can be stacked
 * up, and you pop them off when you need to report them.  The reporting could
 * be done by printing them to stdio, or by passing them back to Tcl/Tk.
 *   Note: It is the caller's responsibility to free the memory, and it is
 * the caller's responsibility to make sure the last call to this is with
 * (NULL), or else the memory won't get freed.
 *
 * ARGUMENTS
 * fmt = Format similar to the one used by sprintf to define how to print the
 *       message (Input)
 *
 * RETURNS: char *
 *   if (fmt == NULL) returns built up string
 *   else             returns NULL.
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 * Supported formats:  See AllocSprintf
 *****************************************************************************
 */
/* Following 2 variables used in both errSprintf and preErrSprintf */
static char *errBuffer = NULL; /* Stores the current built up message. */
static size_t errBuff_len = 0; /* Allocated length of errBuffer. */

char *errSprintf (const char *fmt, ...)
{
   va_list ap;          /* Contains the data needed by fmt. */
   char *ans;           /* Pointer to the final message while we reset
                         * buffer. */

   if (fmt == NULL) {
      ans = errBuffer;
      errBuffer = NULL;
      errBuff_len = 0;
      return ans;
   }
   va_start (ap, fmt);  /* make ap point to 1st unnamed arg. */
   AllocSprintf (&errBuffer, &errBuff_len, fmt, ap);
   va_end (ap);         /* clean up when done. */
   return NULL;
}

/*****************************************************************************
 * preErrSprintf() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This uses AllocSprintf to generate a message, which it prepends to the
 * static variable used by errSprinf.  If it is called with a (NULL), it
 * does nothing... Use errSprintf (NULL) to get the message, and reset the
 * pointer to NULL.
 *   The idea here is that we want to prepend calling info when there was an
 * error.
 *   Note: It is the caller's responsibility to free the memory, by
 * eventually making one last call to errSprintf (NULL) and freeing the
 * returned memory.
 *
 * ARGUMENTS
 * fmt = Format similar to the one used by sprintf to define how to print the
 *       message (Input)
 *
 * RETURNS: void
 *
 * 12/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * Supported formats:  See AllocSprintf
 *****************************************************************************
 */
void preErrSprintf (const char *fmt, ...)
{
   char *preBuffer = NULL; /* Stores the prepended message. */
   size_t preBuff_len = 0; /* Allocated length of preBuffer. */
   va_list ap;          /* Contains the data needed by fmt. */

   myAssert (sizeof (char) == 1);

   if (fmt == NULL) {
      return;
   }
   va_start (ap, fmt);  /* make ap point to 1st unnamed arg. */
   AllocSprintf (&preBuffer, &preBuff_len, fmt, ap);
   va_end (ap);         /* clean up when done. */

   if (errBuff_len != 0) {
      /* Increase preBuffer to have enough room for errBuffer */
      preBuff_len += errBuff_len;
      preBuffer = (char *) realloc ((void *) preBuffer, preBuff_len);
      /* concat errBuffer to end of preBuffer, and free errBuffer */
      strcat (preBuffer, errBuffer);
      free (errBuffer);
   }
   /* Finally point errBuffer to preBuffer, and update errBuff_len. */
   errBuffer = preBuffer;
   errBuff_len = preBuff_len;
   return;
}

#ifdef unused_by_GDAL
/*****************************************************************************
 * _myWarn() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This is an update to my errSprintf routines.  This procedure uses
 * AllocSprintf to generate a message, which it stores in a static variable.
 * It allows for prepending or appending error messages, and allows one to
 * set the error level of a message.
 *
 * ARGUMENTS
 * f_errCode = 0 => append notation msg, 1 => append warning msg
 *             2 => append error msg, 3 => prepend notation msg
 *             4 => prepend warning msg, 5 => prepend error msg (Input)
 *       fmt = Format to define how to print the msg (Input)
 *        ap = The arguments for the message. (Input)
 *
 * RETURNS: void
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
/* Following variables used in the myWarn routines */
static char *warnBuff = NULL; /* Stores the current built up message. */
static size_t warnBuffLen = 0; /* Allocated length of warnBuff. */
static sChar warnLevel = -1; /* Current warning level. */
static uChar warnOutType = 0; /* Output type as set in myWarnSet. */
static uChar warnDetail = 0; /* Detail level as set in myWarnSet. */
static uChar warnFileDetail = 0; /* Detail level as set in myWarnSet. */
static FILE *warnFP = NULL; /* Warn File as set in myWarnSet. */

static void _myWarn (uChar f_errCode, const char *fmt, va_list ap)
{
   char *buff = NULL;   /* Stores the message. */
   size_t buffLen = 0;  /* Allocated length of buff. */
   uChar f_prepend = 0; /* Flag to prepend (or not) the message. */
   uChar f_filePrt = 1; /* Flag to print to file. */
   uChar f_memPrt = 1;  /* Flag to print to memory. */

   if (fmt == NULL) {
      return;
   }
   if (f_errCode > 5) {
      f_errCode = 0;
   }
   if (f_errCode > 2) {
      f_errCode -= (uChar) 3;
      f_prepend = 1;
   }
   /* Update the warning level */
   if (f_errCode > warnLevel) {
      warnLevel = f_errCode;
   }

   /* Check if the warnDetail level allows this message. */
   if ((warnOutType >= 4) ||
       (warnDetail == 2) || ((warnDetail == 1) && (f_errCode < 2))) {
      f_memPrt = 0;
   }
   if ((warnOutType == 0) ||
       (warnFileDetail == 2) || ((warnFileDetail == 1) && (f_errCode < 2))) {
      if (!f_memPrt) {
         return;
      }
      f_filePrt = 0;
   }

   AllocSprintf (&buff, &buffLen, fmt, ap);

   /* Handle the file writing. */
   if (f_filePrt) {
      fprintf (warnFP, "%s", buff);
   }
   /* Handle the memory writing.  */
   if (f_memPrt) {
      if (f_prepend) {
         if (warnBuffLen != 0) {
            /* Add warnBuff to end of buff, and free warnBuff. */
            buffLen += warnBuffLen;
            myAssert (sizeof (char) == 1);
            buff = (char *) realloc (buff, buffLen);
            strcat (buff, warnBuff);
            free (warnBuff);
         }
         /* Point warnBuff to buff. */
         warnBuff = buff;
         warnBuffLen = buffLen;
      } else {
         if (warnBuffLen == 0) {
            warnBuff = buff;
            warnBuffLen = buffLen;
         } else {
            warnBuffLen += buffLen;
            myAssert (sizeof (char) == 1);
            warnBuff = (char *) realloc (warnBuff, warnBuffLen);
            strcat (warnBuff, buff);
            free (buff);
         }
      }
   }
}

/*****************************************************************************
 * myWarn() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This does the transformation of the "..." parameters, and calls _myWarn.
 * This was broken out when we started to implement myWarnRet, so we had two
 * ways to call _myWarn.  A complicated way (myWarnRet), and a simpler way
 * (myWarn).  After creating the myWarnW# #defines, thought to deprecate use
 * of myWarn by making it static.  Still need it, because myWarnRet uses it.
 *
 * ARGUMENTS
 * f_errCode = 0 => append notation msg, 1 => append warning msg
 *             2 => append error msg, 3 => prepend notation msg
 *             4 => prepend warning msg, 5 => prepend error msg (Input)
 *       fmt = Format to define how to print the msg (Input)
 *       ... = The actual message arguments. (Input)
 *
 * RETURNS: void
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
static void myWarn (uChar f_errCode, const char *fmt, ...)
{
   va_list ap;          /* Contains the data needed by fmt. */

   /* Create the message in buff. */
   va_start (ap, fmt);  /* make ap point to 1st unnamed arg. */
   _myWarn (f_errCode, fmt, ap);
   va_end (ap);         /* clean up when done. */
}

/*****************************************************************************
 * myWarnRet() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This does the transformation of the "..." parameters, and calls _myWarn.
 * This was created, so that the user could pass in where (file and line
 * number) the error took place, and get a uniform handling of the file and
 * line numbers.  In addition the user could pass in a value for the procedure
 * to return, which allows the user to have something like:
 *   "return myWarnW2 (-1, "foobar\n");"
 * Which after the #define is evaluated becomes:
 *   "return myWarnRet (1, -1, __FILE__, __LINE__, "foobar\n");
 *
 * Without myWarnRet, one would need something like:
 *   "myWarn (1, "(%s line %d) foobar\n", __FILE__, __LINE__);"
 *   "return (-1);
 * Trying to come up with a #define to make that easier on the user was
 * difficult.  The first attempt was:
 *   #define myWarnLine myWarn(1, "(%s, line %d) " __FILE__, __LINE__); myWarn
 * but this had difficulties with "if () myWarnLine" since it became two
 * statements, which could confuse the use of {}.  A better solition was:
 *   #define myWarnLineW1(f) myWarnLine (1, __FILE__, __LINE__, f)
 * Particularly since the user didn't have to remember that Warn is flag of 1,
 * and error is flag of 2.  Since I already had to create myWarnW# #defines,
 * it was easy to add the user specified return values.
 *
 * ARGUMENTS
 *  f_errCode = 0 => append notation msg, 1 => append warning msg
 *              2 => append error msg, 3 => prepend notation msg
 *              4 => prepend warning msg, 5 => prepend error msg (Input)
 * appErrCode = User defined error code for myWarnRet to return.
 *       file = Filename that the call to myWarnRet was in. (Input)
 *              If NULL, then it skips the __FILE__, __LINE__ print routine.
 *    lineNum = Line number of call to myWarnRet. (Input)
 *        fmt = Format to define how to print the msg (Input)
 *        ... = The actual message arguments. (Input)
 *
 * RETURNS: int
 *   The value of appErrCode.
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *   Is in "Quiet" mode if "file" is NULL (no __FILE__, __LINE__ prints)
 *****************************************************************************
 */
int myWarnRet (uChar f_errCode, int appErrCode, const char *file,
               int lineNum, const char *fmt, ...)
{
   va_list ap;          /* Contains the data needed by fmt. */

   if (fmt != NULL) {
      if (file != NULL) {
         myWarn (f_errCode, "(%s, line %d) ", file, lineNum);
      }
      /* Create the message in buff. */
      va_start (ap, fmt); /* make ap point to 1st unnamed arg. */
      _myWarn (f_errCode, fmt, ap);
      va_end (ap);      /* clean up when done. */
   } else if (file != NULL) {
      myWarn (f_errCode, "(%s, line %d)\n", file, lineNum);
   }
   return appErrCode;
}

/*****************************************************************************
 * myWarnSet() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This sets warnOutType, warnDetail, and warnFile for myWarn.
 *
 * ARGUMENTS
 *    f_outType = 0 => memory, 1 => memory + stdout, 2 => memory + stderr,
 *                3 => memory + warnFile, 4 => stdout, 5 => stderr,
 *                6 => warnFile. (Input)
 *     f_detail = 0 => report all, 1 => report errors, 2 => silent. (Input)
 * f_fileDetail = 0 => report all, 1 => report errors, 2 => silent. (Input)
 *     warnFile = An already opened alternate file to log errors to. (Input)
 *
 * RETURNS: void
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *   The reason someone may want memory + warnFile is so that they can log the
 * errors in a logFile, but still have something come to stdout.
 *****************************************************************************
 */
void myWarnSet (uChar f_outType, uChar f_detail, uChar f_fileDetail,
                FILE *warnFile)
{
   if (f_outType > 6) {
      f_outType = 0;
   }
   if (f_detail > 2) {
      f_detail = 0;
   }
   warnOutType = f_outType;
   warnDetail = f_detail;
   warnFileDetail = f_fileDetail;
   if ((f_outType == 1) || (f_outType == 4)) {
      warnFP = stdout;
   } else if ((f_outType == 2) || (f_outType == 5)) {
      warnFP = stderr;
   } else if ((f_outType == 3) || (f_outType == 6)) {
      if (warnFile == NULL) {
         warnFP = stderr;
      } else {
         warnFP = warnFile;
      }
   } else {
      warnFP = NULL;
   }
}

/*****************************************************************************
 * myWarnClear() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This clears the warning stack, returns what is on there in msg, resets
 * the memory to NULL, and returns the error code.
 *
 * ARGUMENTS
 *         msg = Whatever has been written to the warning memory
 *               (NULL, or allocated memory) (Out)
 * f_closeFile = flag to close the warnFile or not (Input)
 *
 * RETURNS: sChar
 *   -1 means no messages in msg (msg should be null)
 *    0 means up to notation msg in msg, but msg should not be null.
 *    1 means up to warning messages in msg, msg should not be null.
 *    2 means up to error messages in msg, msg should not be null.
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
sChar myWarnClear (char **msg, uChar f_closeFile)
{
   sChar ans;

   *msg = warnBuff;
   warnBuff = NULL;
   warnBuffLen = 0;
   ans = warnLevel;
   warnLevel = -1;
   if (f_closeFile) {
      fclose (warnFP);
   }
   return ans;
}

/*****************************************************************************
 * myWarnNotEmpty() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This returns whether the warning message is null or not.
 *
 * ARGUMENTS
 *
 * RETURNS: uChar
 *   0 => msg == null, 1 => msg != null
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
uChar myWarnNotEmpty ()
{
   return (uChar) ((warnBuff != NULL) ? 1 : 0);
}

/*****************************************************************************
 * myWarnLevel() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   This returns the status of the warnLevel.
 *
 * ARGUMENTS
 *
 * RETURNS: sChar
 *   -1 means no messages in msg (msg should be null)
 *    0 means up to notation msg in msg, but msg should not be null.
 *    1 means up to warning messages in msg, msg should not be null.
 *    2 means up to error messages in msg, msg should not be null.
 *
 * 12/2005 Arthur Taylor (MDL): Created.
 *
 * NOTES:
 *****************************************************************************
 */
sChar myWarnLevel ()
{
   return warnLevel;
}
#endif // unused_by_GDAL

#ifdef TEST_MYERROR
/*****************************************************************************
 * The following 2 procedures are included only to test myerror.c, and only
 * if TEST_MYERROR is defined.
 *****************************************************************************
 */

/*****************************************************************************
 * checkAns() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   To verify that a test gives the expected result.
 *
 * ARGUMENTS
 *  ptr = The results of the test. (Input)
 *  Ans = An array of correct answers. (Input)
 * test = Which test we are checking. (Input)
 *
 * RETURNS: void
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 *****************************************************************************
 */
static void checkAns (char *ptr, char **Ans, int test)
{
   if (ptr == NULL) {
      printf ("-----Check test (%d)--(ptr == NULL)-----\n", test);
      return;
   }
   if (strcmp (ptr, Ans[test]) != 0) {
      printf ("-----Failed test %d-------\n", test);
      printf ("%s %d =?= %s %d\n", ptr, strlen (ptr),
              Ans[test], strlen (Ans[test]));
   } else {
      printf ("passed test %d\n", test);
   }
}

/*****************************************************************************
 * main() -- Arthur Taylor / MDL (Review 12/2002)
 *
 * PURPOSE
 *   To test reallocSprint, mallocSprint, and errSprintf, to make sure that
 * they pass certain basic tests.  I will be adding more tests, as more bugs
 * are found, and features added.
 *
 * ARGUMENTS
 * argc = The number of arguments on the command line. (Input)
 * argv = The arguments on the command line. (Input)
 *
 * RETURNS: int
 *
 *  9/2002 Arthur Taylor (MDL/RSIS): Created.
 * 12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 *****************************************************************************
 */
int main (int argc, char **argv)
{
   char *ptr;
   uChar warn;
   static char *Cmd[] = { "configure", "inquire", "convert", NULL };
   sInt4 li_temp = 100000L;
   short int sect = 5;
   char varName[] = "Helium is a gas";
   sInt4 lival = 22;
   char unit[] = "km", sval[] = "ans";
   double dval = 2.71828;

   char *buffer = NULL;
   short int ssect = 0;
   char vvarName[] = "DataType";
   sInt4 llival = 0;
   char ssval[] = "Meteorological products";

   static char *Ans[] = { "S0 | DataType | 0 (Meteorological products)\n",
      "<testing>", "<05><3.1415><D><20>",
      "<configure,inquire,convert> ?options?",
      "100000", "25.123", "02s", "01234567890123456789012345",
      "25.123,05, hello world",
      "This is a test 5... Here I am\n",
      "Parse error Section 0\nErrorERROR: Problems opening c:--goober for "
            "write.Projection code requires Earth with Rad = 6367.47 not "
            "6400.010000",
      "ERROR IS1 not labeled correctly. 5000000\n"
            "Should be 1196575042 2 25\nERROR IS0 has unexpected values: "
            "100000 100000 100000\n",
      "S5 | Helium is a gas | 22 (ans)\nS5 | Helium is a gas | 22\n"
            "S5 | Helium is a gas | 22 (ans (km))\nS5 | Helium is a gas | ans\n"
            "S5 | Helium is a gas | 2.718280\nS5 | Helium is a gas | "
            "2.718280 (km)\n",
      "ERROR IS1 not labeled correctly. 5000000\n"
            "Should be 1196575042 2 25\nERROR IS0 has unexpected values: "
            "100000 100000 100000\n",
      "5.670000e+001"
   };

/* Test -2. (See if it can handle blank). */
   mallocSprintf (&ptr, "");
   free (ptr);
   ptr = NULL;

   mallocSprintf (&ptr, " ");
   free (ptr);
   ptr = NULL;


/* Test -1. (see if checkAns is ok) */
   ptr = errSprintf (NULL);
   checkAns (ptr, Ans, -1);

/* Test 0 */
   reallocSprintf (&buffer, "S%d | %s | %ld (%s)\n", ssect, vvarName,
                   llival, ssval);
   checkAns (buffer, Ans, 0);
   free (buffer);

/* Test 1. */
   ptr = NULL;
   reallocSprintf (&ptr, "<testing>");
   checkAns (ptr, Ans, 1);
   free (ptr);

/* Test 2. */
   ptr = NULL;
   reallocSprintf (&ptr, "<%02d><%.4f><%D><%ld>", 5, 3.1415, 20, 24);
   checkAns (ptr, Ans, 2);
   free (ptr);

/* Test 3. */
   ptr = NULL;
   reallocSprintf (&ptr, "<%S> ?options?", Cmd);
   checkAns (ptr, Ans, 3);
   free (ptr);

/* Test 4. */
   ptr = NULL;
   reallocSprintf (&ptr, "%ld", li_temp);
   checkAns (ptr, Ans, 4);
   free (ptr);

/* Test 5. */
   ptr = NULL;
   reallocSprintf (&ptr, "%.3f", 25.1234);
   checkAns (ptr, Ans, 5);
   free (ptr);

/* Test 6. */
   ptr = NULL;
   reallocSprintf (&ptr, "%02s", 25.1234);
   checkAns (ptr, Ans, 6);
   free (ptr);

/* Test 7. */
   ptr = NULL;
   reallocSprintf (&ptr, "%01234567890123456789012345");
   checkAns (ptr, Ans, 7);
   free (ptr);

/* Test 8. */
   mallocSprintf (&ptr, "%.3f", 25.1234);
   reallocSprintf (&ptr, ",%02d", 5);
   reallocSprintf (&ptr, ", %s", "hello world");
   checkAns (ptr, Ans, 8);
   free (ptr);
   ptr = NULL;

/* Test 9. */
   errSprintf ("This is a test %d... ", 5);
   errSprintf ("Here I am\n");
   ptr = errSprintf (NULL);
   checkAns (ptr, Ans, 9);
   free (ptr);

/* Test 10. */
   errSprintf ("Parse error Section 0\n%s", "Error");
   errSprintf ("ERROR: Problems opening %s for write.", "c:--goober");
   errSprintf ("Projection code requires Earth with Rad = 6367.47 not %f",
               6400.01);
   ptr = errSprintf (NULL);
   checkAns (ptr, Ans, 10);
   free (ptr);

/* Test 11. */
   errSprintf ("ERROR IS1 not labeled correctly. %ld\n", 5000000L);
   errSprintf ("Should be %ld %d %ld\n", 1196575042L, 2, 25);
   errSprintf ("ERROR IS0 has unexpected values: %ld %ld %ld\n", li_temp,
               li_temp, li_temp);
   ptr = errSprintf (NULL);
   checkAns (ptr, Ans, 11);
   free (ptr);

/* Test 12. */
   ptr = NULL;
   reallocSprintf (&ptr, "S%d | %s | %ld (%s)\n", sect, varName, lival, sval);
   reallocSprintf (&ptr, "S%d | %s | %ld\n", sect, varName, lival);
   reallocSprintf (&ptr, "S%d | %s | %ld (%s (%s))\n", sect, varName, lival,
                   sval, unit);
   reallocSprintf (&ptr, "S%d | %s | %s\n", sect, varName, sval);
   reallocSprintf (&ptr, "S%d | %s | %f\n", sect, varName, dval);
   reallocSprintf (&ptr, "S%d | %s | %f (%s)\n", sect, varName, dval, unit);
   checkAns (ptr, Ans, 12);
   free (ptr);

/* Test 13. */
   preErrSprintf ("Should be %ld %d %ld\n", 1196575042L, 2, 25);
   errSprintf ("ERROR IS0 has unexpected values: %ld %ld %ld\n", li_temp,
               li_temp, li_temp);
   preErrSprintf ("ERROR IS1 not labeled correctly. %ld\n", 5000000L);
   ptr = errSprintf (NULL);
   checkAns (ptr, Ans, 13);
   free (ptr);

/* Test 14. */
   ptr = NULL;
   reallocSprintf (&ptr, "%e", 56.7);
   checkAns (ptr, Ans, 14);
   free (ptr);

   myWarnSet (1, 0, 1, NULL);
   myWarnW2 (0, "This is a test of Warn\n");
   myWarnE2 (0, "This is a test of Err\n");
   myWarnQ2 (0, "This is a quiet note\n");
   myWarnPW2 (0, "This is a test2 of Error\n");
   myWarnW4 (0, "This is a test of WarnLnW3 %d %d\n", 10, 20);
   myWarnE3 (0, "This is a test of WarnLnE2 %d\n", 10);
   printf ("\tTest myWarnRet: %d\n", myWarnW1 (-1));
   printf ("\tTest myWarnRet: %d\n", myWarnE2 (-2, "Hello nurse\n"));
   if (myWarnNotEmpty ()) {
      ptr = NULL;
      warn = myWarnClear (&ptr, 0);
      printf ("WarnLevel=%d\n%s", warn, ptr);
      free (ptr);
   }

   return 0;
}
#endif
