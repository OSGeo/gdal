/*****************************************************************************
 * myutil.c
 *
 * DESCRIPTION
 *    This file contains some simple utility functions.
 *
 * HISTORY
 * 12/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */

/* For S_IFDIR */
#if defined(__sun__) && __STDC_VERSION__ >= 201112L
#if _XOPEN_SOURCE < 600
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 600
#endif
#else
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
//#include <direct.h>
//#include <dirent.h>
#include "myutil.h"
#include "myassert.h"

#include "cpl_port.h"

/* Android compat */
#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif

#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

#ifndef S_IEXEC
#define S_IEXEC S_IXUSR
#endif
/* End of Android compat */

#ifdef MEMWATCH
#include "memwatch.h"
#endif

/*****************************************************************************
 * reallocFGets() -- Arthur Taylor / MDL
 *
 * PURPOSE
 *   Read in data from file until a \n is read.  Reallocate memory as needed.
 * Similar to fgets, except we don't know ahead of time that the line is a
 * specific length.
 *   Assumes that Ptr is either NULL, or points to lenBuff memory.
 *   Responsibility of caller to free the memory.
 *
 * ARGUMENTS
 *     Ptr = An array of data that is of size LenBuff. (Input/Output)
 * LenBuff = The Allocated length of Ptr. (Input/Output)
 *      fp = Input file stream (Input)
 *
 * RETURNS: size_t
 *   strlen (buffer)
 *     0 = We read only EOF
 *     1 = We have "\nEOF" or "<char>EOF"
 *
 * 12/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *  1) Based on getline (see K&R C book (2nd edition) p 29) and on the
 *     behavior of Tcl's gets routine.
 *  2) Chose MIN_STEPSIZE = 80 because pages are usually 80 columns.
 *  3) Could switch lenBuff = i + 1 / lenBuff = i to always true.
 *     Rather not... Less allocs... This way code behaves almost the
 *     same as fgets except it can expand as needed.
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
#define MIN_STEPSIZE 80
size_t reallocFGets (char **Ptr, size_t *LenBuff, FILE *fp)
{
   char *buffer = *Ptr; /* Local copy of Ptr. */
   size_t lenBuff = *LenBuff; /* Local copy of LenBuff. */
   int c;               /* Current char read from stream. */
   size_t i;            /* Where to store c. */

   myAssert (sizeof (char) == 1);
   for (i = 0; ((c = getc (fp)) != EOF) && (c != '\n'); ++i) {
      if (i >= lenBuff) {
         lenBuff += MIN_STEPSIZE;
         buffer = (char *) realloc ((void *) buffer, lenBuff);
      }
      buffer[i] = (char) c;
   }
   if (c == '\n') {
      if (lenBuff <= i + 1) {
         lenBuff = i + 2; /* Make room for \n\0. */
         buffer = (char *) realloc ((void *) buffer, lenBuff);
      }
      buffer[i] = (char) c;
      ++i;
   } else {
      if (lenBuff <= i) {
         lenBuff = i + 1; /* Make room for \0. */
         buffer = (char *) realloc ((void *) buffer, lenBuff);
      }
   }
   buffer[i] = '\0';
   *Ptr = buffer;
   *LenBuff = lenBuff;
   return i;
}

#undef MIN_STEPSIZE
#endif

/*****************************************************************************
 * mySplit() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Split a character array according to a given symbol.
 *   Responsibility of caller to free the memory.
 *
 * ARGUMENTS
 *   data = character string to look through. (Input)
 * symbol = character to split based on. (Input)
 *   argc = number of groupings found. (Output)
 *   argv = characters in each grouping. (Output)
 * f_trim = True if we should white space trim each element in list. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *  5/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */

#if 0  // Unused with GDAL.
void mySplit (const char *data, char symbol, size_t *Argc, char ***Argv,
              char f_trim)
{
   const char *head;    /* The head of the current string */
   const char *ptr;     /* a pointer to walk over the data. */
   size_t argc = 0;     /* Local copy of Argc */
   char **argv = NULL;  /* Local copy of Argv */
   size_t len;          /* length of current string. */

   myAssert (*Argc == 0);
   myAssert (*Argv == NULL);
   myAssert (sizeof (char) == 1);

   head = data;
   while (head != NULL) {
      argv = (char **) realloc ((void *) argv, (argc + 1) * sizeof (char *));
      ptr = strchr (head, symbol);
      if (ptr != NULL) {
         len = ptr - head;
         argv[argc] = (char *) malloc (len + 1);
         strncpy (argv[argc], head, len);
         argv[argc][len] = '\0';
         if (f_trim) {
            strTrim (argv[argc]);
         }
         argc++;
         head = ptr + 1;
         /* The following head != NULL is in case data is not '\0' terminated
          */
         if ((head != NULL) && (*head == '\0')) {
            /* Handle a break character just before the \0 */
            /* This results in not adding a "" to end of list. */
            head = NULL;
         }
      } else {
         /* Handle from here to end of text. */
         len = strlen (head);
         argv[argc] = (char *) malloc (len + 1);
         strcpy (argv[argc], head);
         if (f_trim) {
            strTrim (argv[argc]);
         }
         argc++;
         head = NULL;
      }
   }
   *Argc = argc;
   *Argv = argv;
}
#endif

#if 0  // Unused with GDAL.
int myAtoI (const char *ptr, sInt4 *value)
{
   char *extra = NULL;         /* The data after the end of the double. */

   myAssert (ptr != NULL);
   *value = 0;
   while (*ptr != '\0') {
      if (isdigit (*ptr) || (*ptr == '+') || (*ptr == '-')) {
         *value = (int)strtol (ptr, &extra, 10);
         myAssert (extra != NULL);
         if (*extra == '\0') {
            return 1;
         }
         break;
      } else if (!isspace ((unsigned char)*ptr)) {
         return 0;
      }
      ptr++;
   }
   /* Check if all white space. */
   if (*ptr == '\0') {
      return 0;
   }
   myAssert (extra != NULL);
   /* Allow first trailing char for ',' */
   if (!isspace ((unsigned char)*extra)) {
      if (*extra != ',') {
         *value = 0;
         return 0;
      }
   }
   extra++;
   /* Make sure the rest is all white space. */
   while (*extra != '\0') {
      if (!isspace ((unsigned char)*extra)) {
         *value = 0;
         return 0;
      }
      extra++;
   }
   return 1;
}
#endif

/*****************************************************************************
 * myAtoF() -- used to be myIsReal()
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *    Returns true if all char are digits except a leading + or -, or a
 * trailing ','.  Ignores leading or trailing white space.  Value is set to
 * atof (ptr).
 *
 * ARGUMENTS
 *   ptr = character string to look at. (Input)
 * value = the converted value of ptr, if ptr is a number. (Output)
 *
 * RETURNS: int
 *   0 = Not a real number,
 *   1 = Real number.
 *
 * HISTORY
 *  7/2004 Arthur Taylor (MDL): Updated
 *  4/2005 AAT (MDL): Did a code walk through.
 *
 * NOTES
 *****************************************************************************
 */

#if 0  // Unused with GDAL.
int myAtoF (const char *ptr, double *value)
{
   char *extra = NULL;         /* The data after the end of the double. */

   myAssert (ptr != NULL);
   *value = 0;
   while (*ptr != '\0') {
      if (isdigit (*ptr) || (*ptr == '+') || (*ptr == '-') || (*ptr == '.')) {
         *value = strtod (ptr, &extra);
         myAssert (extra != NULL);
         if (*extra == '\0') {
            return 1;
         }
         break;
      } else if (!isspace ((unsigned char)*ptr)) {
         return 0;
      }
      ptr++;
   }
   /* Check if all white space. */
   if (*ptr == '\0') {
      return 0;
   }
   myAssert (extra != NULL);
   /* Allow first trailing char for ',' */
   if (!isspace ((unsigned char)*extra)) {
      if (*extra != ',') {
         *value = 0;
         return 0;
      }
   }
   extra++;
   /* Make sure the rest is all white space. */
   while (*extra != '\0') {
      if (!isspace ((unsigned char)*extra)) {
         *value = 0;
         return 0;
      }
      extra++;
   }
   return 1;
}
#endif

#if 0  // Unused with GDAL.
/* Change of name was to deprecate usage... Switch to myAtoF */
int myIsReal_old (const char *ptr, double *value)
{
   size_t len, i;

   *value = 0;
   if ((!isdigit (*ptr)) && (*ptr != '.'))
      if (*ptr != '-')
         return 0;
   len = strlen (ptr);
   for (i = 1; i < len - 1; i++) {
      if ((!isdigit (ptr[i])) && (ptr[i] != '.'))
         return 0;
   }
   if ((!isdigit (ptr[len - 1])) && (ptr[len - 1] != '.')) {
      if (ptr[len - 1] != ',') {
         return 0;
      } else {
/*         ptr[len - 1] = '\0';*/
         *value = atof (ptr);
/*         ptr[len - 1] = ',';*/
         return 1;
      }
   }
   *value = atof (ptr);
   return 1;
}
#endif

/* Return:
 * 0 if 'can't stat the file' (most likely not a file)
 * 1 if it is a directory
 * 2 if it is a file
 * 3 if it doesn't understand the file
 */
/* mtime may behave oddly...
 * stat appeared correct if I was in EST and the file was in EST,
 * but was off by 1 hour if I was in EST and the file was in EDT.
 * rddirlst.c solved this through use of "clock".
 *
 * Could return mode: RDCF___rwxrwxrwx where R is 1/0 based on regular file
 * D is 1/0 based on directory, first rwx is user permissions...
 */
#if 0  // Unused with GDAL.
int myStat (char *filename, char *perm, sInt4 *size, double *mtime)
{
   struct stat stbuf;
   char f_cnt;
   char *ptr;
   int ans;

   myAssert (filename != NULL);

   /* Check for unmatched quotes (apparently stat on MS-Windows lets:
    * ./data/ndfd/geodata\" pass, which causes issues later. */
   f_cnt = 0;
   for (ptr = filename; *ptr != '\0'; ptr++) {
      if (*ptr == '"')
         f_cnt = !f_cnt;
   }
   if (f_cnt) {
      /* unmatched quotes. */
      if (size)
         *size = 0;
      if (mtime)
         *mtime = 0;
      if (perm)
         *perm = 0;
      return 0;
   }

   /* Try to stat file. */
   if ((ans = stat (filename, &stbuf)) == -1) {
      if ((filename[strlen (filename) - 1] == '/') ||
          (filename[strlen (filename) - 1] == '\\')) {
         filename[strlen (filename) - 1] = '\0';
         ans = stat (filename, &stbuf);
      }
   }
   /* Can't stat */
   if (ans == -1) {
      if (size)
         *size = 0;
      if (mtime)
         *mtime = 0;
      if (perm)
         *perm = 0;
      return 0;
   }

   if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
      /* Is a directory */
      if (size)
         *size = (sInt4)stbuf.st_size;
      if (mtime)
         *mtime = stbuf.st_mtime;
      if (perm) {
         *perm = (stbuf.st_mode & S_IREAD) ? 4 : 0;
         if (stbuf.st_mode & S_IWRITE)
            *perm += 2;
         if (stbuf.st_mode & S_IEXEC)
            *perm += 1;
      }
      return MYSTAT_ISDIR;
   } else if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
      /* Is a file */
      if (size)
         *size = (sInt4)stbuf.st_size;
      if (mtime)
         *mtime = stbuf.st_mtime;
      if (perm) {
         *perm = (stbuf.st_mode & S_IREAD) ? 4 : 0;
         if (stbuf.st_mode & S_IWRITE)
            *perm += 2;
         if (stbuf.st_mode & S_IEXEC)
            *perm += 1;
      }
      return MYSTAT_ISFILE;
   } else {
      /* unrecognized file type */
      if (size)
         *size = 0;
      if (mtime)
         *mtime = 0;
      if (perm)
         *perm = 0;
      return 3;
   }
}
#endif

/**
static int FileMatch (const char *filename, const char *filter)
{
   const char *ptr1;
   const char *ptr2;

   ptr2 = filename;
   for (ptr1 = filter; *ptr1 != '\0'; ptr1++) {
      if (*ptr1 == '*') {
         if (ptr1[1] == '\0') {
            return 1;
         } else {
            ptr2 = strchr (ptr2, ptr1[1]);
            if (ptr2 == NULL) {
               return 0;
            }
         }
      } else if (*ptr2 == '\0') {
         return 0;
      } else if (*ptr1 == '?') {
         ptr2++;
      } else {
         if (*ptr1 == *ptr2) {
            ptr2++;
         } else {
            return 0;
         }
      }
   }
   return (*ptr2 == '\0');
}
**/

#if 0  // Unused with GDAL.
int myGlob (CPL_UNUSED const char *dirName,
            CPL_UNUSED const char *filter,
            CPL_UNUSED size_t *Argc,
            CPL_UNUSED char ***Argv)
{
return 0; // TODO: reimplement for Win32
#if 0
   size_t argc = 0;     // Local copy of Argc
   char **argv = NULL;  // Local copy of Argv
   struct dirent *dp;
   DIR *dir;

   myAssert (*Argc == 0);
   myAssert (*Argv == NULL);

   if ((dir = opendir (dirName)) == NULL)
      return -1;

   while ((dp = readdir (dir)) != NULL) {
      /* Skip self and parent. */
      if (strcmp (dp->d_name, ".") == 0 || strcmp (dp->d_name, "..") == 0)
         continue;
      if (FileMatch (dp->d_name, filter)) {
         argv = (char **) realloc (argv, (argc + 1) * sizeof (char *));
         argv[argc] = (char *) malloc ((strlen (dirName) + 1 +
                                        strlen (dp->d_name) +
                                        1) * sizeof (char));
         sprintf (argv[argc], "%s/%s", dirName, dp->d_name);
         argc++;
      }
   }
   *Argc = argc;
   *Argv = argv;
   return 0;
#endif
}
#endif

/*****************************************************************************
 * FileCopy() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Copy a file from one location to another.
 *
 * ARGUMENTS
 *  fileIn = source file to read from. (Input)
 *  fileOut = destination file to write to. (Input)
 *
 * RETURNS: int
 *   0 = success.
 *   1 = problems opening fileIn
 *   2 = problems opening fileOut
 *
 * HISTORY
 *  5/2004 Arthur Taylor (MDL/RSIS): Created.
 *  4/2005 AAT (MDL): Did a code walk through.
 *
 * NOTES
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
int FileCopy (const char *fileIn, const char *fileOut)
{
   FILE *ifp;           /* The file pointer to read from. */
   FILE *ofp;           /* The file pointer to write to. */
   int c;               /* temporary variable while reading / writing. */

   if ((ifp = fopen (fileIn, "rb")) == NULL) {
#ifdef DEBUG
      printf ("Couldn't open %s for read\n", fileIn);
#endif
      return 1;
   }
   if ((ofp = fopen (fileOut, "wb")) == NULL) {
#ifdef DEBUG
      printf ("Couldn't open %s for write\n", fileOut);
#endif
      fclose (ifp);
      return 2;
   }
   while ((c = getc (ifp)) != EOF) {
      putc (c, ofp);
   }
   fclose (ifp);
   fclose (ofp);
   return 0;
}
#endif

/*****************************************************************************
 * FileTail() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Returns the characters in a filename after the last directory separator.
 *   Responsibility of caller to free the memory.
 *
 * ARGUMENTS
 * fileName = fileName to look at. (Input)
 *     tail = Tail of the filename. (Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *  5/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */

#if 0  // Unused with GDAL.
void FileTail (const char *fileName, char **tail)
{
   const char *ptr;     /* A pointer to last \ or // in fileName. */

   myAssert (fileName != NULL);
   myAssert (sizeof (char) == 1);

   ptr = strrchr (fileName, '/');
   if (ptr == NULL) {
      ptr = strrchr (fileName, '\\');
      if (ptr == NULL) {
         ptr = fileName;
      } else {
         ptr++;
      }
   } else {
      ptr++;
   }
   *tail = (char *) malloc (strlen (ptr) + 1);
   strcpy (*tail, ptr);
}
#endif

/*****************************************************************************
 * myRound() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Round a number to a given number of decimal places.
 *
 * ARGUMENTS
 *  data = number to round (Input)
 * place = How many decimals to round to (Input)
 *
 * RETURNS: double (rounded value)
 *
 * HISTORY
 *  5/2003 Arthur Taylor (MDL/RSIS): Created.
 *  2/2006 AAT: Added the (double) (.5) cast, and the mult by POWERS_OVER_ONE
 *         instead of division.
 *
 * NOTES
 *  1) It is probably inadvisable to make a lot of calls to this routine,
 *     considering the fact that a context swap is made, so this is provided
 *     primarily as an example, but it can be used for some rounding.
 *****************************************************************************
 */
static const double POWERS_ONE[] = {
   1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
   1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17
};

double myRound (double data, uChar place)
{
   if (place > 17)
      place = 17;

   return (floor (data * POWERS_ONE[place] + 5e-1)) / POWERS_ONE[place];

   /* Tried some other options to see if I could fix test 40 on linux, but
    * changing it appears to make other tests fail on other OS's. */
/*
   return (((sInt4) (data * POWERS_ONE[place] + .5)) / POWERS_ONE[place]);
*/
/*
   return (floor (data * POWERS_ONE[place] + .5)) / POWERS_ONE[place];
*/
}

/*****************************************************************************
 * strTrim() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Trim the white space from both sides of a char string.
 *
 * ARGUMENTS
 * str = The string to trim (Input/Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *  10/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *   See K&R p106 for strcpy part.
 *****************************************************************************
 */
void strTrim (char *str)
{
   char *ptr;           /* Pointer to where first non-white space is. */
   char *ptr2;          /* Pointer to just past last non-white space. */

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   /* Trim the string to the left first. */
   for (ptr = str; isspace (*ptr); ptr++) {
   }
   /* Did we hit the end of an all space string? */
   if (*ptr == '\0') {
      *str = '\0';
      return;
   }

   /* now work on the right side. */
   for (ptr2 = ptr + (strlen (ptr) - 1); isspace (*ptr2); ptr2--) {
   }

   /* adjust the pointer to add the null byte. */
   ptr2++;
   *ptr2 = '\0';

   if (ptr != str) {
      /* Can't do a strcpy here since we don't know that they start at left
       * and go right. */
      while ((*str++ = *ptr++) != '\0') {
      }
      *str = '\0';
   }
}

/*****************************************************************************
 * strTrimRight() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Trim white space and a given char from the right.
 *
 * ARGUMENTS
 * str = The string to trim (Input/Output)
 *   c = The character to remove. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *  7/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
void strTrimRight (char *str, char c)
{
   int i;               /* loop counter for traversing str. */

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   for (i = (int)strlen (str) - 1;
        ((i >= 0) && ((isspace ((unsigned char)str[i])) || (str[i] == c))); i--) {
   }
   str[i + 1] = '\0';
}

/*****************************************************************************
 * strCompact() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Replace any multiple instances of 'c' in the string with 1 instance.
 *
 * ARGUMENTS
 * str = The string to compact (Input/Output)
 *   c = The character to look for. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 * 10/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
void strCompact (char *str, char c)
{
   char *ptr;           /* The next good value in str to keep. */

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   ptr = str;
   while ((*str = *(ptr++)) != '\0') {
      if (*(str++) == c) {
         while ((*ptr != '\0') && (*ptr == c)) {
            ptr++;
         }
      }
   }
}

/*****************************************************************************
 * strReplace() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Replace all instances of c1 in str with c2.
 *
 * ARGUMENTS
 * str = The string to trim (Input/Output)
 *  c1 = The character(s) in str to be replaced. (Input)
 *  c2 = The char to replace c1 with. (Input)
 *
 * RETURNS: void
 *
 * HISTORY
 *  7/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
void strReplace (char *str, char c1, char c2)
{
   char *ptr = str;

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   for (ptr = str; *ptr != '\0'; ptr++) {
      if (*ptr == c1) {
         *ptr = c2;
      }
   }
}
#endif

/*****************************************************************************
 * strToUpper() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Convert a string to all uppercase.
 *
 * ARGUMENTS
 * str = The string to adjust (Input/Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *  10/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
void strToUpper (char *str)
{
   char *ptr = str;     /* Used to traverse str. */

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   while ((*ptr++ = toupper (*str++)) != '\0') {
   }
}
#endif

/*****************************************************************************
 * strToLower() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Convert a string to all lowercase.
 *
 * ARGUMENTS
 * str = The string to adjust (Input/Output)
 *
 * RETURNS: void
 *
 * HISTORY
 *  5/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
void strToLower (char *str)
{
   char *ptr = str;     /* Used to traverse str. */

   /* str shouldn't be null, but if it is, we want to handle it. */
   myAssert (str != NULL);
   if (str == NULL) {
      return;
   }

   while ((*ptr++ = tolower (*str++)) != '\0') {
   }
}
#endif

/*
 * Returns: Length of the string.
 * History: 1/29/98 AAT Commented.
 *
int str2lw (char *s) {
  int i = 0, len = strlen (s);
  while (i < len) {
    s[i] = (char) tolower(s[i]);
    i++;
  }
  return len;
}
*/

/*****************************************************************************
 * strcmpNoCase() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Compare two strings without concern for case.
 *
 * ARGUMENTS
 * str1 = String1 to compare (Input)
 * str2 = String2 to compare (Input)
 *
 * RETURNS: int
 *   -1 = (str1 < str2)
 *    0 = (str1 == str2)
 *    1 = (str1 > str2)
 *
 * HISTORY
 *  5/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *   See K&R p 106
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
int strcmpNoCase (const char *str1, const char *str2)
{
   /* str1, str2 shouldn't be null, but if it is, we want to handle it. */
   myAssert (str1 != NULL);
   myAssert (str2 != NULL);
   if (str1 == NULL) {
      if (str2 == NULL) {
         return 0;
      } else {
         return -1;
      }
   }
   if (str2 == NULL) {
      return 1;
   }

   for (; tolower (*str1) == tolower (*str2); str1++, str2++) {
      if (*str1 == '\0')
         return 0;
   }
   return (tolower (*str1) - tolower (*str2) < 0) ? -1 : 1;
/*
   strlen1 = strlen (str1);
   strlen2 = strlen (str2);
   min = (strlen1 < strlen2) ? strlen1 : strlen2;
   for (i = 0; i < min; i++) {
      c1 = tolower (str1[i]);
      c2 = tolower (str2[i]);
      if (c1 < c2)
         return -1;
      if (c1 > c2)
         return 1;
   }
   if (strlen1 < strlen2) {
      return -1;
   }
   if (strlen1 > strlen2) {
      return 1;
   }
   return 0;
*/
}
#endif

 /*****************************************************************************
 * GetIndexFromStr() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Looks through a list of strings (with a NULL value at the end) for a
 * given string.  Returns the index where it found it.
 *
 * ARGUMENTS
 *   str = The string to look for. (Input)
 *   Opt = The list to look for arg in. (Input)
 * Index = The location of arg in Opt (or -1 if it couldn't find it) (Output)
 *
 * RETURNS: int
 *  # = Where it found it.
 * -1 = Couldn't find it.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *
 * NOTES
 *   Why not const char **Opt?
 *****************************************************************************
 */
int GetIndexFromStr (const char *str, const char * const *Opt, int *Index)
{
   int cnt = 0;         /* Current Count in Opt. */

   myAssert (str != NULL);
   if (str == NULL) {
      *Index = -1;
      return -1;
   }

   for (; *Opt != NULL; Opt++, cnt++) {
      if (strcmp (str, *Opt) == 0) {
         *Index = cnt;
         return cnt;
      }
   }
   *Index = -1;
   return -1;
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
 * RETURNS: sInt2
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
#if 0  // Unused with GDAL.
static sChar Clock_GetTimeZone ()
{
   struct tm l_time;
   time_t ansTime;
   struct tm *gmTime;
   static sChar timeZone = 127;

   if (timeZone == 127) {
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
#endif

/*****************************************************************************
 * myParseTime() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parse a string such as "19730724000000" and return time since the
 * beginning of the epoch.
 *
 * ARGUMENTS
 *      is = String to read the date from (Input)
 * AnsTime = Time to String2 to compare (Input)
 *
 * RETURNS: int
 *    0 = success
 *    1 = error
 *
 * HISTORY
 *  4/2005 Arthur Taylor (MDL): Commented
 *
 * NOTES
 *   Rename (myParseTime -> myParseTime2) because changed error return from
 *      -1 to 1
 *   Rename (myParseTime2 -> myParseTime3) because I'm trying to phase it out.
 *   Use: int Clock_ScanDateNumber (double *clock, char *buffer) instead.
 *****************************************************************************
 */
#if 0  // Unused with GDAL.
int myParseTime3 (const char *is, time_t * AnsTime)
{
   char buffer[5];      /* A temporary variable for parsing "is". */
   sShort2 year;        /* The year. */
   uChar mon;           /* The month. */
   uChar day;           /* The day. */
   uChar hour;          /* The hour. */
   uChar min;           /* The minute. */
   uChar sec;           /* The second. */
   struct tm l_time;      /* A temporary variable to put the time info into. */

   memset (&l_time, 0, sizeof (struct tm));
   myAssert (strlen (is) == 14);
   if (strlen (is) != 14) {
      printf ("%s is not formatted correctly\n", is);
      return 1;
   }
   strncpy (buffer, is, 4);
   buffer[4] = '\0';
   year = atoi (buffer);
   strncpy (buffer, is + 4, 2);
   buffer[2] = '\0';
   mon = atoi (buffer);
   strncpy (buffer, is + 6, 2);
   day = atoi (buffer);
   strncpy (buffer, is + 8, 2);
   hour = atoi (buffer);
   strncpy (buffer, is + 10, 2);
   min = atoi (buffer);
   strncpy (buffer, is + 12, 2);
   sec = atoi (buffer);
   if ((year > 2001) || (year < 1900) || (mon > 12) || (mon < 1) ||
       (day > 31) || (day < 1) || (hour > 23) || (min > 59) || (sec > 60)) {
      printf ("date %s is invalid\n", is);
      printf ("%d %d %d %d %d %d\n", year, mon, day, hour, min, sec);
      return 1;
   }
   l_time.tm_year = year - 1900;
   l_time.tm_mon = mon - 1;
   l_time.tm_mday = day;
   l_time.tm_hour = hour;
   l_time.tm_min = min;
   l_time.tm_sec = sec;
   *AnsTime = mktime (&l_time) - (Clock_GetTimeZone () * 3600);
   return 0;
}
#endif

#ifdef MYUTIL_TEST
int main (int argc, char **argv)
{
   char buffer[] = "Hello , World, This, is, a , test\n";
   char buffer2[] = "";
   size_t listLen = 0;
   char **List = NULL;
   size_t i;
   size_t j;
   char ans;
   double value;
   char *tail;

/*
   printf ("1 :: %f\n", clock() / (double) (CLOCKS_PER_SEC));
   for (j = 0; j < 25000; j++) {
      mySplit (buffer, ',', &listLen, &List, 1);
      for (i = 0; i < listLen; i++) {
         free (List[i]);
      }
      free (List);
      List = NULL;
      listLen = 0;
   }
   printf ("1 :: %f\n", clock() / (double) (CLOCKS_PER_SEC));
*/
   mySplit (buffer, ',', &listLen, &List, 1);
   for (i = 0; i < listLen; i++) {
      printf ("%d:'%s'\n", i, List[i]);
      free (List[i]);
   }
   free (List);
   List = NULL;
   listLen = 0;

   mySplit (buffer2, ',', &listLen, &List, 1);
   for (i = 0; i < listLen; i++) {
      printf ("%d:'%s'\n", i, List[i]);
      free (List[i]);
   }
   free (List);
   List = NULL;
   listLen = 0;

   strcpy (buffer, "  0.95");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "0.95");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "+0.95");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "0.95,  ");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "0.95,");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "0.9.5");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "  alph 0.9.5");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "  ");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   strcpy (buffer, "");
   ans = myAtoF (buffer, &value);
   printf ("%d %f : ", ans, value);
   ans = myIsReal_old (buffer, &value);
   printf ("%d %f : '%s'\n", ans, value, buffer);

   tail = NULL;
   FileTail ("test\\me/now", &tail);
   printf ("%s \n", tail);
   free (tail);
   tail = NULL;
   FileTail ("test/me\\now", &tail);
   printf ("%s \n", tail);
   free (tail);

   strcpy (buffer, "  here  ");
   strTrim (buffer);
   printf ("%s\n", buffer);

   strcpy (buffer, "  here  ");
   strCompact (buffer, ' ');
   printf ("'%s'\n", buffer);
   return 0;
}
#endif
