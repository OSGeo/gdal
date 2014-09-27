/*****************************************************************************
 * degrib2.c
 *
 * DESCRIPTION
 *    This file contains the main driver routines to call the unpack grib2
 * library functions.  It also contains the code needed to figure out the
 * dimensions of the arrays before calling the FORTRAN library.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL / RSIS): Created.
 *  12/2002 Tim Kempisty, Ana Canizares, Tim Boyer, & Marc Saccucci
 *          (TK,AC,TB,&MS): Code Review 1.
 *
 * NOTES
 *****************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "myassert.h"
#include "myerror.h"
#include "memendian.h"
#include "meta.h"
#include "metaname.h"
//#include "write.h"
#include "degrib2.h"
#include "degrib1.h"
#include "tdlpack.h"
#include "grib2api.h"
//#include "mymapf.h"
#include "clock.h"

#define GRIB_UNSIGN_INT3(a,b,c) ((a<<16)+(b<<8)+c)

/*****************************************************************************
 * ReadSect0() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Looks for the next GRIB message, by looking for the keyword "GRIB".  It
 * expects the message in "expect" bytes from the start, but could find the
 * message in "expect2" bytes or 0 bytes from the start.  Returns -1 if it
 * can't find "GRIB", 1 if "GRIB" is not 0, "expect", or "expect2" bytes from
 * the start.
 *   It stores the bytes it reads (a max of "expect") upto but not including
 * the 'G' in "GRIB" in wmo.
 *
 *   After it finds section 0, it then parses the 16 bytes that make up
 * section 0 so that it can return the length of the entire GRIB message.
 *
 *   When done, it sets fp to point to the end of Sect0.
 *
 *   The reason for this procedure is so that we can read in the size of the
 * grib message, and thus allocate enough memory to read the message in before
 * making it Big endian, and passing it to the library for unpacking.
 *
 * ARGUMENTS
 *       fp = A pointer to an opened file in which to read.
 *            When done, this points to the start of section 1. (Input/Output)
 *     buff = The data between messages. (Input/Output)
 *  buffLen = The length of buff (Output)
 *    limit = How many bytes to read before giving up and stating it is not
 *            a proper message.  (-1 means no limit). (Input)
 *    sect0 = The read in Section 0 (as seen on disk). (Output)
 *  gribLen = Length of this GRIB message. (Output)
 *  version = 1 if GRIB1 message, 2 if GRIB2 message, -1 if TDLP message.
 *            (Output)
 *   expect = The expected number of bytes to find "GRIB" in. (Input)
 *  expect2 = The second possible number of bytes to find "GRIB" in. (Input)
 *      wmo = Assumed allocated to be at least size "expect".
 *            Holds the bytes before the first "GRIB" message.
 *            expect should be > expect2, but is up to caller (Output)
 *   wmoLen = Length of wmo (total number of bytes read - SECT0LEN_WORD * 4).
 *            (Output)
 *
 * FILES/DATABASES:
 *   An already opened "GRIB2" File
 *
 * RETURNS: int (could use errSprintf())
 *  1 = Length of wmo was != 0 and was != expect
 *  0 = OK
 * -1 = Couldn't find "GRIB" part of message.
 * -2 = Ran out of file while reading this section.
 * -3 = Grib version was not 1 or 2.
 * -4 = Most significant sInt4 of GRIB length was not 0
 * -5 = Grib message length was <= 16 (can't be smaller than just sect 0)
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Combined with ReadWMOHeader
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   1/2003 AAT: Bug found. wmo access out of bounds of expect when setting
 *          the /0 element, if wmoLen > expect.
 *   4/2003 AAT: Added ability to handle GRIB version 1.
 *   5/2003 AAT: Added limit option.
 *   8/2003 AAT: Removed dependence on offset, and fileLen.
 *  10/2004 AAT: Modified to allow for TDLP files
 *
 * NOTES
 * 1a) 1196575042L == ASCII representation of "GRIB" (GRIB in MSB)
 * 1b) 1112101447L == ASCII representation of "BIRG" (GRIB in LSB)
 * 1c) 1413762128L == ASCII representation of "TDLP" (TDLP in MSB)
 * 1d) 1347175508L == ASCII representation of "PLDT" (TDLP in LSB)
 * 2) Takes advantage of the wordType to check that the edition is correct.
 * 3) May want to return prodType.
 * 4) WMO_HEADER_ORIG_LEN was added for backward compatibility... should be
 *    removed when we no longer use old format. (say in a year from 11/2002)
 *
 *****************************************************************************
 */
int ReadSECT0 (DataSource &fp, char **buff, uInt4 *buffLen, sInt4 limit,
               sInt4 sect0[SECT0LEN_WORD], uInt4 *gribLen, int *version)
{
   typedef union {
      sInt4 li;
      unsigned char buffer[4];
   } wordType;

   uChar gribMatch = 0; /* Counts how many letters in GRIB we've matched. */
   uChar tdlpMatch = 0; /* Counts how many letters in TDLP we've matched. */
   wordType word;       /* Used to check that the edition is correct. */
   uInt4 curLen;        /* Where we currently are in buff. */
   uInt4 i;             /* Used to loop over the first few char's */
   uInt4 stillNeed;     /* Number of bytes still needed to get 1st 8 bytes of 
                         * message into memory. */

   /* Get first 8 bytes.  If GRIB we don't care.  If TDLP, this is the length 
    * of record.  Read at least 1 record (length + 2 * 8) + 8 (next record
    * length) + 8 bytes before giving up. */
   curLen = 8;
   if (*buffLen < curLen) {
      *buffLen = curLen;
      *buff = (char *) realloc ((void *) *buff, *buffLen * sizeof (char));
   }
   if (fp.DataSourceFread(*buff, sizeof (char), curLen) != curLen) {
      errSprintf ("ERROR: Couldn't find 'GRIB' or 'TDLP'\n");
      return -1;
   }
/*
   Can't do the following because we don't know if the file is a GRIB file or
   not, or if it was a FORTRAN file.
   if (limit > 0) {
      MEMCPY_BIG (&recLen, *buff, 4);
      limit = (limit > recLen + 32) ? limit : recLen + 32;
   }
*/
   while ((tdlpMatch != 4) && (gribMatch != 4)) {
      for (i = curLen - 8; i + 3 < curLen; i++) {
         if ((*buff)[i] == 'G') {
            if (((*buff)[i + 1] == 'R') && ((*buff)[i + 2] == 'I') &&
                ((*buff)[i + 3] == 'B')) {
               gribMatch = 4;
               break;
            }
         } else if ((*buff)[i] == 'T') {
            if (((*buff)[i + 1] == 'D') && ((*buff)[i + 2] == 'L') &&
                ((*buff)[i + 3] == 'P')) {
               tdlpMatch = 4;
               break;
            }
         }
      }
      stillNeed = i - (curLen - 8);
      /* Read enough of message to have the first 8 bytes (including ID). */
      if (stillNeed != 0) {
         curLen += stillNeed;
         if ((limit >= 0) && (curLen > (size_t) limit)) {
            errSprintf ("ERROR: Couldn't find type in %ld bytes\n", limit);
            return -1;
         }
         if (*buffLen < curLen) {
            *buffLen = curLen;
            *buff = (char *) realloc ((void *) *buff,
                                      *buffLen * sizeof (char));
         }
         if (fp.DataSourceFread((*buff) + (curLen - stillNeed), sizeof (char), stillNeed) != stillNeed) {
            errSprintf ("ERROR: Ran out of file reading SECT0\n");
            return -1;
         }
      }
   }

   /* curLen and (*buff) hold 8 bytes of section 0. */
   curLen -= 8;
   memcpy (&(sect0[0]), (*buff) + curLen, 4);
#ifdef DEBUG
#ifdef LITTLE_ENDIAN
   myAssert ((sect0[0] == 1112101447L) || (sect0[0] == 1347175508L));
#else
   myAssert ((sect0[0] == 1196575042L) || (sect0[0] == 1413762128L));
#endif
#endif
   memcpy (&(sect0[1]), *buff + curLen + 4, 4);
   /* Make sure we don't pass back part of "GRIB" in the buffer. */
   (*buff)[curLen] = '\0';
   *buffLen = curLen;

   word.li = sect0[1];
   if (tdlpMatch == 4) {
      if (word.buffer[3] != 0) {
         errSprintf ("ERROR: unexpected version of TDLP in SECT0\n");
         return -2;
      }
      *version = -1;
      /* Find out the GRIB Message Length */
      *gribLen = GRIB_UNSIGN_INT3 (word.buffer[0], word.buffer[1],
                                   word.buffer[2]);
      /* Min message size: GRIB1=52, TDLP=59, GRIB2=86. */
      if (*gribLen < 59) {
         errSprintf ("TDLP length %ld was < 59?\n", *gribLen);
         return -5;
      }
   } else if (word.buffer[3] == 1) {
      *version = 1;
      /* Find out the GRIB Message Length */
      *gribLen = GRIB_UNSIGN_INT3 (word.buffer[0], word.buffer[1],
                                   word.buffer[2]);
      /* Min message size: GRIB1=52, TDLP=59, GRIB2=86. */
      if (*gribLen < 52) {
         errSprintf ("GRIB1 length %ld was < 52?\n", *gribLen);
         return -5;
      }
   } else if (word.buffer[3] == 2) {
      *version = 2;
      /* Make sure we still have enough file for the rest of section 0. */
      if (fp.DataSourceFread(sect0 + 2, sizeof (sInt4), 2) != 2) {
         errSprintf ("ERROR: Ran out of file reading SECT0\n");
         return -2;
      }
      if (sect0[2] != 0) {
         errSprintf ("Most significant sInt4 of GRIB length was not 0?\n");
         errSprintf ("This is either an error, or we have a single GRIB "
                     "message which is larger than 2^31 = 2,147,283,648 "
                     "bytes.\n");
         return -4;
      }
#ifdef LITTLE_ENDIAN
      revmemcpy (gribLen, &(sect0[3]), sizeof (sInt4));
#else
      *gribLen = sect0[3];
#endif
   } else {
      errSprintf ("ERROR: Not TDLPack, and Grib edition is not 1 or 2\n");
      return -3;
   }
   return 0;
}

/*****************************************************************************
 * FindGRIBMsg() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Jumps through a GRIB2 file looking for a specific message.  Currently
 * that message is determined by msgNum which is in the range of 1..n.
 *   In the future we may be searching based on projection or date.
 *
 * ARGUMENTS
 *      fp = The current GRIB2 file to look through. (Input)
 *  msgNum = Which message to look for. (Input)
 *  offset = Where in the file the message starts (this is before the
 *           wmo ASCII part if there is one.) (Output)
 *  curMsg = The current # of messages we have looked through. (In/Out) 
 *
 * FILES/DATABASES:
 *   An already opened "GRIB2" File
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = Problems reading Section 0.
 * -2 = Ran out of file.
 *
 * HISTORY
 *  11/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   6/2003 Matthew T. Kallio (matt@wunderground.com):
 *          "wmo" dimension increased to WMO_HEADER_LEN + 1 (for '\0' char)
 *   8/2003 AAT: Removed dependence on offset and fileLen.
 *
 * NOTES
 *****************************************************************************
 */
int FindGRIBMsg (DataSource &fp, int msgNum, sInt4 *offset, int *curMsg)
{
   int cnt;             /* The current message we are looking at. */
   char *buff;          /* Holds the info between records. */
   uInt4 buffLen;       /* Length of info between records. */
   sInt4 sect0[SECT0LEN_WORD]; /* Holds the current Section 0. */
   uInt4 gribLen;       /* Length of the current GRIB message. */
   int version;         /* Which version of GRIB is in this message. */
   int c;               /* Determine if end of the file without fileLen. */
   sInt4 jump;          /* How far to jump to get to past GRIB message. */

   cnt = *curMsg + 1;
   buff = NULL;
   buffLen = 0;
   while ((c = fp.DataSourceFgetc()) != EOF) {
      fp.DataSourceUngetc(c);
      if (cnt >= msgNum) {
         /* 12/1/2004 version 1.63 forgot to free buff */
         free (buff);
         *curMsg = cnt;
         return 0;
      }
      /* Read section 0 to find gribLen and wmoLen. */
      if (ReadSECT0 (fp, &buff, &buffLen, GRIB_LIMIT, sect0, &gribLen,
                     &version) < 0) {
         preErrSprintf ("Inside FindGRIBMsg\n");
         free (buff);
         return -1;
      }
      myAssert ((version == 1) || (version == 2) || (version == -1));
      /* Continue on to the next grib message. */
      if ((version == 1) || (version == -1)) {
         jump = gribLen - 8;
      } else {
         jump = gribLen - 16;
      }
      fp.DataSourceFseek(jump, SEEK_CUR);
      *offset = *offset + gribLen + buffLen;
      cnt++;
   }
   free (buff);
   *curMsg = cnt - 1;
   /* Return -2 since we reached the end of file. This may not be an error
    * (multiple file option). */
   return -2;
/*
   errSprintf ("ERROR: Ran out of file looking for msgNum %d.\n", msgNum);
   errSprintf ("       Current msgNum %d\n", cnt);
*/
}

/*****************************************************************************
 * FindSectLen2to7() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Looks through a GRIB message and finds out the maximum size of each
 * section.  Simpler if there is only one grid in the message.
 *
 * ARGUMENTS
 * c_ipack = The current GRIB2 message. (Input)
 * gribLen = Length of c_ipack. (Input)
 *      ns = Array of section lengths. (Output)
 * sectNum = Which section to start with. (Input)
 *  curTot = on going total read from c_ipack. (Input)
 *   nd2x3 = Total number of grid points (Output)
 * table50 = Type of packing used. (See code table 5.0) (GS5_SIMPLE = 0,
 *           GS5_CMPLX = 2, GS5_CMPLXSEC = 3) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = Ran of data in a section
 * -2 = Section not properly labeled.
 *
 * HISTORY
 *   3/2003 AAT: Created
 *
 * NOTES
 * 1) Assumes that the pack method of multiple grids are the same.
 *****************************************************************************
 */
static int FindSectLen2to7 (char *c_ipack, sInt4 gribLen, sInt4 ns[8],
                            char sectNum, sInt4 *curTot, sInt4 *nd2x3,
                            short int *table50)
{
   sInt4 sectLen;       /* The length of the current section. */
   sInt4 li_temp;       /* A temporary holder of sInt4s. */

   if ((sectNum == 2) || (sectNum == 3)) {
      /* Figure out the size of section 2 and 3. */
      if (*curTot + 5 > gribLen) {
         errSprintf ("ERROR: Ran out of data in Section 2 or 3\n");
         return -1;
      }
      /* Handle optional section 2. */
      if (c_ipack[*curTot + 4] == 2) {
         MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
         *curTot = *curTot + sectLen;
         if (ns[2] < sectLen)
            ns[2] = sectLen;
         if (*curTot + 5 > gribLen) {
            errSprintf ("ERROR: Ran out of data in Section 3\n");
            return -1;
         }
      }
      /* Handle section 3. */
      if (c_ipack[*curTot + 4] != 3) {
         errSprintf ("ERROR: Section 3 labeled as %d\n",
                     c_ipack[*curTot + 4]);
         return -2;
      }
      MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
      if (ns[3] < sectLen)
         ns[3] = sectLen;
      /* While we are here, grab the total number of grid points nd2x3. */
      MEMCPY_BIG (&li_temp, c_ipack + *curTot + 6, 4);
      if (*nd2x3 < li_temp)
         *nd2x3 = li_temp;
      *curTot = *curTot + sectLen;
   }
/*
#ifdef DEBUG
   printf ("Section len (2=%ld) (3=%ld)\n", ns[2], ns[3]);
#endif
*/

   /* Figure out the size of section 4. */
   if (*curTot + 5 > gribLen) {
      errSprintf ("ERROR: Ran out of data in Section 4\n");
      return -1;
   }
   if (c_ipack[*curTot + 4] != 4) {
      errSprintf ("ERROR: Section 4 labeled as %d\n", c_ipack[*curTot + 4]);
      return -2;
   }
   MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
   if (ns[4] < sectLen)
      ns[4] = sectLen;
   *curTot = *curTot + sectLen;
/*
#ifdef DEBUG
   printf ("Section len (4=%ld < %ld)\n", sectLen, ns[4]);
#endif
*/

   /* Figure out the size of section 5. */
   if (*curTot + 5 > gribLen) {
      errSprintf ("ERROR: Ran out of data in Section 5\n");
      return -1;
   }
   if (c_ipack[*curTot + 4] != 5) {
      errSprintf ("ERROR: Section 5 labeled as %d\n", c_ipack[*curTot + 4]);
      return -2;
   }
   MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
   /* While we are here, grab the packing method. */
   MEMCPY_BIG (table50, c_ipack + *curTot + 9, 2);
   if (ns[5] < sectLen)
      ns[5] = sectLen;
   *curTot = *curTot + sectLen;
/*
#ifdef DEBUG
   printf ("Section len (5=%ld < %ld)\n", sectLen, ns[5]);
#endif
*/

   /* Figure out the size of section 6. */
   if (*curTot + 5 > gribLen) {
      errSprintf ("ERROR: Ran out of data in Section 6\n");
      return -1;
   }
   if (c_ipack[*curTot + 4] != 6) {
      errSprintf ("ERROR: Section 6 labeled as %d\n", c_ipack[*curTot + 4]);
      return -2;
   }
   MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
   if (ns[6] < sectLen)
      ns[6] = sectLen;
   *curTot = *curTot + sectLen;
/*
#ifdef DEBUG
   printf ("Section len (6=%ld < %ld)\n", sectLen, ns[6]);
#endif
*/

   /* Figure out the size of section 7. */
   if (*curTot + 5 > gribLen) {
      errSprintf ("ERROR: Ran out of data in Section 7\n");
      return -1;
   }
   if (c_ipack[*curTot + 4] != 7) {
      errSprintf ("ERROR: Section 7 labeled as %d\n", c_ipack[*curTot + 4]);
      return -2;
   }
   MEMCPY_BIG (&sectLen, c_ipack + *curTot, 4);
   if (ns[7] < sectLen)
      ns[7] = sectLen;
   *curTot = *curTot + sectLen;
/*
#ifdef DEBUG
   printf ("Section len (7=%ld < %ld)\n", sectLen, ns[7]);
#endif
*/
   return 0;
}

/*****************************************************************************
 * FindSectLen() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Looks through a GRIB message and finds out how big each section is.
 *
 * ARGUMENTS
 * c_ipack = The current GRIB2 message. (Input)
 * gribLen = Length of c_ipack. (Input)
 *      ns = Array of section lengths. (Output)
 *   nd2x3 = Total number of grid points (Output)
 * table50 = Type of packing used. (See code table 5.0) (GS5_SIMPLE = 0,
 *           GS5_CMPLX = 2, GS5_CMPLXSEC = 3) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = Ran of data in a section
 * -2 = Section not properly labeled.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Updated.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   3/2003 AAT: Made it handle multiple grids in the same GRIB2 message.
 *   5/2003 AAT: Bug: Initialized size of section 2..6 to -1, instead
 *               of 2..7.
 *
 * NOTES
 * 1) Assumes that the pack method of multiple grids are the same.
 *****************************************************************************
 */
static int FindSectLen (char *c_ipack, sInt4 gribLen, sInt4 ns[8],
                        sInt4 *nd2x3, short int *table50)
{
   sInt4 curTot;        /* Where we are in the current GRIB message. */
   char sectNum;        /* Which section we are working with. */
   int ans;             /* The return error code of FindSectLen2to7. */
   sInt4 sectLen;       /* The length of the current section. */
   int i;               /* counter as we init ns[]. */

   ns[0] = SECT0LEN_WORD * 4;
   curTot = ns[0];

   /* Figure out the size of section 1. */
   if (curTot + 5 > gribLen) {
      errSprintf ("ERROR: Ran out of data in Section 1\n");
      return -1;
   }
   if (c_ipack[curTot + 4] != 1) {
      errSprintf ("ERROR: Section 1 labeled as %d\n", c_ipack[curTot + 4]);
      return -2;
   }
   MEMCPY_BIG (&(ns[1]), c_ipack + curTot, 4);
   curTot += ns[1];
/*
#ifdef DEBUG
   printf ("Section len (0=%ld) (1=%ld)\n", ns[0], ns[1]);
#endif
*/
   sectNum = 2;
   for (i = 2; i < 8; i++) {
      ns[i] = -1;
   }
   *nd2x3 = -1;
   do {
      if ((ans = FindSectLen2to7 (c_ipack, gribLen, ns, sectNum, &curTot,
                                  nd2x3, table50)) != 0) {
         return ans;
      }
      /* Try to read section 8.  If it is "7777" == 926365495 regardless of
       * endian'ness then we have a simple message, otherwise it is complex,
       * and we need to read more. */
      memcpy (&sectLen, c_ipack + curTot, 4);
      if (sectLen == 926365495L) {
         sectNum = 8;
      } else {
         sectNum = c_ipack[curTot + 4];
         if ((sectNum < 2) || (sectNum > 7)) {
            errSprintf ("ERROR (FindSectLen): Couldn't find the end of the "
                        "message\n");
            errSprintf ("and it doesn't appear to repeat sections.\n");
            errSprintf ("so it is probably an ASCII / binary bug\n");
            errSprintf ("Max Sect Lengths: %ld %ld %ld %ld %ld %ld %ld"
                        " %ld\n", ns[0], ns[1], ns[2], ns[3], ns[4], ns[5],
                        ns[6], ns[7]);
            return -2;
         }
      }
   } while (sectNum != 8);
   return 0;
}

/*****************************************************************************
 * IS_Init() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Initialize the IS data structure.  The IS_dataType is used to organize
 * and allocate all the arrays that the unpack library uses.
 *   This makes an initial guess for the size of the arrays, and we use
 * realloc to increase the size if needed.  The write up: "UNPK_GRIB2
 * 3/15/02" by Bryon Lawrence, Bob Glahn, David Rudack suggested these numbers
 *
 * ARGUMENTS
 * is = The data structure to initialize. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT : Updated.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *
 * NOTES
 * 1) Numbers not found in document were discused with Bob Glahn on 8/29/2002
 * 2) Possible exceptions:
 *    template 3.120 could need ns[3] = 1600
 *    template 4.30 could need a different ns4.
 * 3) sizeof(float) == sizeof(sInt4), and unpacker does not use both ain
 *    and iain, so it is possible to have ain and iain point to the same
 *    memory.  Not sure if this is safe, so I haven't done it.
 *****************************************************************************
 */
void IS_Init (IS_dataType *is)
{
   int i;               /* A simple loop counter. */
   is->ns[0] = 16;
   is->ns[1] = 21;
   is->ns[2] = 7;
   is->ns[3] = 96;
   is->ns[4] = 130;     /* 60->130 in case there are some S4 time intervals */
   is->ns[5] = 49;
   is->ns[6] = 6;
   is->ns[7] = 8;
   for (i = 0; i < 8; i++) {
      is->is[i] = (sInt4 *) calloc (is->ns[i], sizeof (sInt4));
   }
   /* Allocate grid memory. */
   is->nd2x3 = 0;
   is->iain = NULL;
   is->ib = NULL;
   /* Allocate section 2 int memory. */
   is->nidat = 0;
   is->idat = NULL;
   /* Allocate section 2 float memory. */
   is->nrdat = 0;
   is->rdat = NULL;
   /* Allocate storage for ipack. */
   is->ipackLen = 0;
   is->ipack = NULL;
}

/*****************************************************************************
 * IS_Free() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Free the memory allocated in the IS data structure.
 * The IS_dataType is used to organize and allocate all the arrays that the
 * unpack library uses.
 *
 * ARGUMENTS
 * is = The data structure to Free. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT : Updated.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *
 * NOTES
 *****************************************************************************
 */
void IS_Free (IS_dataType *is)
{
   int i;               /* A simple loop counter. */
   for (i = 0; i < 8; i++) {
      free (is->is[i]);
      is->is[i] = NULL;
      is->ns[i] = 0;
   }
   /* Free grid memory. */
   free (is->iain);
   is->iain = NULL;
   free (is->ib);
   is->ib = NULL;
   is->nd2x3 = 0;
   /* Free section 2 int memory. */
   free (is->idat);
   is->idat = NULL;
   is->nidat = 0;
   /* Free section 2 float memory. */
   free (is->rdat);
   is->rdat = NULL;
   is->nrdat = 0;
   /* Free storage for ipack. */
   free (is->ipack);
   is->ipack = NULL;
   is->ipackLen = 0;
}

/*****************************************************************************
 * ReadGrib2Record() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Reads a GRIB2 message from a file which is already opened and is pointing
 * at the correct message.  It reads in the message storing the results in
 * Grib_Data which is of size grib_DataLen.  If needed, it increases
 * grib_DataLen enough to fit the current message's grid.  It converts (if
 * appropriate) the data in Grib_Data to the units specified in f_unit.
 *
 *   In addition it updates offset, and stores the meta data returned by the
 * unpacker library in both IS, and (after parsing it) in meta.
 *
 *   Note: It expects meta and IS to already be initialized through calls to
 * MetaInit(&meta) and IS_Init(&is) respectively.
 *
 * ARGUMENTS
 *           fp = An opened GRIB2 file already at the correct message. (Input)
 *      fileLen = Length of the opened file. (Input)
 *       f_unit = 0 use GRIB2 units, 1 use English, 2 use metric. (Input)
 *    Grib_Data = The read in GRIB2 grid. (Output)
 * grib_DataLen = Size of Grib_Data. (Output)
 *         meta = A filled in meta structure (Output)
 *           IS = The structure containing all the arrays that the
 *                unpacker uses (Output)
 *      subgNum = Which subgrid in the GRIB2 message is of interest.
 *                (0 = first grid), if it can't find message subgNum,
 *                returns -5, and an error message (Input)
 *     majEarth = Used to override the GRIB major axis of earth. (Input)
 *     minEarth = Used to override the GRIB minor axis of earth. (Input)
 *      simpVer = The version of the simple weather code to use when parsing
 *                the WxString. (Input)
 *     f_endMsg = 1 means we finished reading the previous GRIB message, or
 *                there was no previous GRIB message.  0 means that we need
 *                to read a subgrid of the previous message. (Input/Output)
 *   lwlt, uprt = If the lat is not -100, then lwlt, and uprt define a
 *                subgrid that the user is interested in.  Get the map
 *                projection out of the GRIB2 message, and do everything
 *                on the subgrid. (if lwlt, and uprt are not "correct", the
 *                lat/lons may get swapped) (Input/Output)
 *
 * FILES/DATABASES:
 *   An already opened "GRIB2" File
 *
 * RETURNS: int (could use errSprintf())
 *  0 = OK
 * -1 = Problems in section 0
 * -2 = Problems figuring out the Section Lengths.
 * -3 = Error returned by unpack library.
 * -4 = Problems parsing the Meta Data.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Updated.
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   1/2003 AAT: It wasn't error coded 208, but rather 202 to look for.
 *   3/2003 AAT: Modified handling of section 2 stuff (no loop)
 *   3/2003 AAT: Added ability to handle multiple grids in same message.
 *   4/2003 AAT: Added ability to call GRIB1 decoder for GRIB1 messages.
 *   5/2003 AAT: Update the offset for ReadGrib1.
 *   6/2003 Matthew T. Kallio (matt@wunderground.com):
 *          "wmo" dimension increased to WMO_HEADER_LEN + 1 (for '\0' char)
 *   7/2003 AAT: switched to checking against element name for Wx instead
 *          of pds2.sect2.ptrType == GS2_WXTYPE
 *   7/2003 AAT: Allowed user to override the radius of earth.
 *   8/2003 AAT: Removed dependence on fileLen and offset.
 *   2/2004 AAT: Added "f_endMsg" logic.
 *   2/2004 AAT: Added subgrid potential.
 *   2/2004 AAT: Added maj/min Earth override.
 *
 * NOTES
 * 1) Reason ns[7] is not MAX (IS.ns[], local_ns[]) is because local_ns[7]
 *    is size of the packed message, but ns[7] refers to the returned meta
 *    data which unpacker library found in section 7, which is a lot smaller.
 * 2) Problem: MDL's sect2 is packed and we have no idea how large it is
 *    when unpacked.  So we allocate room for 4000 sInt4s and 500 floats.
 *    We then check 'jer' for error "202", if we find it we double the size
 *    and call the unpacker again.
 *    3/26/2003: Changed this to be: try once with size
 *       = max (32 * packed size, 4000)
 *    Should be fewer calls (more memory intensive) same result, since we had
 *    been doubling it 5 times.
 * 3) For Complex second order packing (GS5_CMPLXSEC) the unpacker needs nd5
 *    (which is size of message) to be >= nd2x3 (size of grid).
 * 3a) Appears to also need this if simple packing, and has a bitmap.
 * 4) inew = 1:  Currently we only expect 1 grid in 1 GRIB message, although
 *    the library does allow for multiple grids in a GRIB message.
 * 5) iclean = 1:  This only maters if there is bitmap data, otherwise it is
 *    ignored.  For bitmap data, if == 0, it embeds the given values for
 *    xmissp, and xmisss.  We don't embed because we don't know what to set
 *    xmissp or xmisss to.  Instead after we know the range, we choose a value
 *    and walk through the bitmap setting grib_Data appropriately.
 * 5a) iclean = 0;  This is because we do want the missing values embeded.
 *    that is we want the missing values to be place holders.
 * 6) f_endMsg is true if in the past we either completed reading a message,
 *    or we haven't read any messages.  In either case we need to read the
 *    next message from file. If f_endMsg is false, then there is more to read
 *    from IS->ipack, so we don't want to throw it out, nor have to re-read
 *    ipack from disk.
 *
 * Question: Should we double ns[2] when we double nrdat, and nidat?
 *****************************************************************************
 */
#define SECT2_INIT_SIZE 4000
#define UNPK_NUM_ERRORS 22
int ReadGrib2Record (DataSource &fp, sChar f_unit, double **Grib_Data,
                     uInt4 *grib_DataLen, grib_MetaData *meta,
                     IS_dataType *IS, int subgNum, double majEarth,
                     double minEarth, int simpVer, sInt4 *f_endMsg,
                     CPL_UNUSED LatLon *lwlf,
                     CPL_UNUSED LatLon *uprt)
{
   sInt4 l3264b;        /* Number of bits in a sInt4.  Needed by FORTRAN
                         * unpack library to determine if system has a 4
                         * byte_ sInt4 or an 8 byte sInt4. */
   char *buff;          /* Holds the info between records. */
   uInt4 buffLen;       /* Length of info between records. */
   sInt4 sect0[SECT0LEN_WORD]; /* Holds the current Section 0. */
   uInt4 gribLen;       /* Length of the current GRIB message. */
   sInt4 nd5;           /* Size of grib message rounded up to the nearest
                         * sInt4. */
   char *c_ipack;       /* A char ptr to the message stored in IS->ipack */
   sInt4 local_ns[8];   /* Local copy of section lengths. */
   sInt4 nd2x3;         /* Total number of grid points. */
   short int table50;   /* Type of packing used. (See code table 5.0)
                         * (GS5_SIMPLE==0, GS5_CMPLX==2, GS5_CMPLXSEC==3) */
   sInt4 nidat;         /* Size of section 2 if it contains integer data. */
   sInt4 nrdat;         /* Size of section 2 if it contains float data. */
   sInt4 inew;          /* 1 if this is the first grid we are reading. 0 if
                         * this is the second or later grid from the same
                         * GRIB message. */
   sInt4 iclean = 0;    /* 0 embed the missing values, 1 don't. */
   int j;               /* Counter used to find the desired subgrid. */
   sInt4 kfildo = 5;    /* FORTRAN Unit number for diagnostic info. Ignored,
                         * unless library is compiled a particular way. */
   sInt4 ibitmap;       /* 0 means no bitmap returned, otherwise 1. */
   float xmissp;        /* The primary missing value.  If iclean = 0, this
                         * value is embeded in grid, otherwise it is the
                         * value returned from the GRIB message. */
   float xmisss;        /* The secondary missing value.  If iclean = 0, this
                         * value is embeded in grid, otherwise it is the
                         * value returned from the GRIB message. */
   sInt4 jer[UNPK_NUM_ERRORS * 2]; /* Any Error codes along with their *
                                    * severity levels generated using the *
                                    * unpack GRIB2 library. */
   sInt4 ndjer = UNPK_NUM_ERRORS; /* The number of rows in JER( ). */
   sInt4 kjer;          /* The actual number of errors returned in JER. */
   size_t i;            /* counter as we loop through jer. */
   double unitM, unitB; /* values in y = m x + b used for unit conversion. */
   char unitName[15];   /* Holds the string name of the current unit. */
   int unitLen;         /* String length of string name of current unit. */
   int version;         /* Which version of GRIB is in this message. */
   sInt4 cnt;           /* Used to help compact the weather table. */
   int x1, y1;          /* The original grid coordinates of the lower left
                         * corner of the subgrid. */
   int x2, y2;          /* The original grid coordinates of the upper right
                         * corner of the subgrid. */
   uChar f_subGrid;     /* True if we have a subgrid. */
   sInt4 Nx, Ny;        /* original size of the data. */

   /*
    * f_endMsg is 1 if in the past we either completed reading a message,
    * or we haven't read any messages.  In either case we need to read the
    * next message from file.
    * If f_endMsg is false, then there is more to read from IS->ipack, so we
    * don't want to throw it out, nor have to re-read ipack from disk.
    */
   l3264b = sizeof (sInt4) * 8;
   buff = NULL;
   buffLen = 0;
   if (*f_endMsg == 1) {
      if (ReadSECT0 (fp, &buff, &buffLen, -1, sect0, &gribLen, &version) < 0) {
         preErrSprintf ("Inside ReadGrib2Record\n");
         free (buff);
         return -1;
      }
      meta->GribVersion = version;
      if (version == 1) {
         if (ReadGrib1Record (fp, f_unit, Grib_Data, grib_DataLen, meta, IS,
                              sect0, gribLen, majEarth, minEarth) != 0) {
            preErrSprintf ("Problems with ReadGrib1Record called by "
                           "ReadGrib2Record\n");
            free (buff);
            return -1;
         }
         *f_endMsg = 1;
         free (buff);
         return 0;
      } else if (version == -1) {
         if (ReadTDLPRecord (fp, Grib_Data, grib_DataLen, meta, IS,
                             sect0, gribLen, majEarth, minEarth) != 0) {
            preErrSprintf ("Problems with ReadGrib1Record called by "
                           "ReadGrib2Record\n");
            free (buff);
            return -1;
         }
         free (buff);
         return 0;
      }

      /*
       * Make room for entire message, and read it in.
       */
      /* nd5 needs to be gribLen in (sInt4) units rounded up. */
      nd5 = (gribLen + 3) / 4;
      if (nd5 > IS->ipackLen) {
         IS->ipackLen = nd5;
         IS->ipack = (sInt4 *) realloc ((void *) (IS->ipack),
                                        (IS->ipackLen) * sizeof (sInt4));
      }
      c_ipack = (char *) IS->ipack;
      /* Init last sInt4 to 0, to make sure that the padded bytes are 0. */
      IS->ipack[nd5 - 1] = 0;
      /* Init first 4 sInt4 to sect0. */
      memcpy (c_ipack, sect0, SECT0LEN_WORD * 4);
      /* Read in the rest of the message. */
      if (fp.DataSourceFread (c_ipack + SECT0LEN_WORD * 4, sizeof (char),
                 (gribLen - SECT0LEN_WORD * 4)) != (gribLen - SECT0LEN_WORD * 4)) {
         errSprintf ("GribLen = %ld, SECT0Len_WORD = %d\n", gribLen,
                     SECT0LEN_WORD);
         errSprintf ("Ran out of file\n");
         free (buff);
         return -1;
      }

      /*
       * Make sure the arrays are large enough for call to unpacker library.
       */
      /* FindSectLen Does not want (ipack / c_ipack) word swapped, because
       * that would make it much more confusing to find bytes in c_ipack. */
      if (FindSectLen (c_ipack, gribLen, local_ns, &nd2x3, &table50) < 0) {
         preErrSprintf ("Inside ReadGrib2Record.. Calling FindSectLen\n");
         free (buff);
         return -2;
      }

      /* Make sure all 'is' arrays except ns[7] are MAX (IS.ns[] ,
       * local_ns[]). See note 1 for reason to exclude ns[7] from MAX (). */
      for (i = 0; i < 7; i++) {
         if (local_ns[i] > IS->ns[i]) {
            IS->ns[i] = local_ns[i];
            IS->is[i] = (sInt4 *) realloc ((void *) (IS->is[i]),
                                           IS->ns[i] * sizeof (sInt4));
         }
      }

      /* Allocate room for sect 2. If local_ns[2] = -1 there is no sect 2. */
      if (local_ns[2] == -1) {
         nidat = 10;
         nrdat = 10;
      } else {
         /*
          * See note 2) We have a section 2, so use:
          *     MAX (32 * local_ns[2],SECT2_INTSIZE)
          * and MAX (32 * local_ns[2],SECT2_FLOATSIZE)
          * for size of section 2 unpacked.
          */
         nidat = (32 * local_ns[2] < SECT2_INIT_SIZE) ? SECT2_INIT_SIZE :
               32 * local_ns[2];
         nrdat = nidat;
      }
      if (nidat > IS->nidat) {
         IS->nidat = nidat;
         IS->idat = (sInt4 *) realloc ((void *) IS->idat,
                                       IS->nidat * sizeof (sInt4));
      }
      if (nrdat > IS->nrdat) {
         IS->nrdat = nrdat;
         IS->rdat = (float *) realloc ((void *) IS->rdat,
                                       IS->nrdat * sizeof (float));
      }
      /* Make sure we have room for the GRID part of the output. */
      if (nd2x3 > IS->nd2x3) {
         IS->nd2x3 = nd2x3;
         IS->iain = (sInt4 *) realloc ((void *) IS->iain,
                                       IS->nd2x3 * sizeof (sInt4));
         IS->ib = (sInt4 *) realloc ((void *) IS->ib,
                                     IS->nd2x3 * sizeof (sInt4));
      }
      /* See note 3) If table50 == 3, unpacker library needs nd5 >= nd2x3. */
      if ((table50 == 3) || (table50 == 0)) {
         if (nd5 < nd2x3) {
            nd5 = nd2x3;
            if (nd5 > IS->ipackLen) {
               IS->ipackLen = nd5;
               IS->ipack = (sInt4 *) realloc ((void *) (IS->ipack),
                                              IS->ipackLen * sizeof (sInt4));
            }
            /* Don't need to do the following, but we do in case code
             * changes. */
            c_ipack = (char *) IS->ipack;
         }
      }
      IS->nd5 = nd5;
      /* Unpacker library requires ipack to be MSB. */
/*
#ifdef DEBUG
      if (1==1) {
         FILE *fp = fopen ("test.bin", "wb");
         fwrite (IS->ipack, sizeof (sInt4), IS->nd5, fp);
         fclose (fp);
      }
#endif
*/
#ifdef LITTLE_ENDIAN
      memswp (IS->ipack, sizeof (sInt4), IS->nd5);
#endif
   } else {
      gribLen = IS->ipack[3];
   }
   free (buff);

   /* Loop through the grib message looking for the subgNum grid.  subgNum
    * goes from 0 to n-1. */
   for (j = 0; j <= subgNum; j++) {
      if (j == 0) {
         inew = 1;
      } else {
         inew = 0;
      }

      /* Note we are getting data back either as a float or an int, but not
       * both, so we don't need to allocated room for both. */
      unpk_grib2 (&kfildo, (float *) (IS->iain), IS->iain, &(IS->nd2x3),
                  IS->idat, &(IS->nidat), IS->rdat, &(IS->nrdat), IS->is[0],
                  &(IS->ns[0]), IS->is[1], &(IS->ns[1]), IS->is[2],
                  &(IS->ns[2]), IS->is[3], &(IS->ns[3]), IS->is[4],
                  &(IS->ns[4]), IS->is[5], &(IS->ns[5]), IS->is[6],
                  &(IS->ns[6]), IS->is[7], &(IS->ns[7]), IS->ib, &ibitmap,
                  IS->ipack, &(IS->nd5), &xmissp, &xmisss, &inew, &iclean,
                  &l3264b, f_endMsg, jer, &ndjer, &kjer);
      /*
       * Check for error messages...
       *   If we get an error message, print it, and return.
       */
      for (i = 0; i < (uInt4) kjer; i++) {
         if (jer[ndjer + i] == 0) {
            /* no error. */
         } else if (jer[ndjer + i] == 1) {
            /* Warning. */
#ifdef DEBUG
            printf ("Warning: Unpack library warning code (%d %d)\n",
                    jer[i], jer[ndjer + i]);
#endif
         } else {
            /* BAD Error. */
            errSprintf ("ERROR: Unpack library error code (%ld %ld)\n",
                        jer[i], jer[ndjer + i]);
            return -3;
         }
      }
   }

   /* Parse the meta data out. */
   if (MetaParse (meta, IS->is[0], IS->ns[0], IS->is[1], IS->ns[1],
                  IS->is[2], IS->ns[2], IS->rdat, IS->nrdat, IS->idat,
                  IS->nidat, IS->is[3], IS->ns[3], IS->is[4], IS->ns[4],
                  IS->is[5], IS->ns[5], gribLen, xmissp, xmisss, simpVer)
       != 0) {
#ifdef DEBUG
      FILE *fp;
      if ((fp = fopen ("dump.is0", "wt")) != NULL) {
         for (i = 0; i < 8; i++) {
            fprintf (fp, "---Section %d---\n", (int) i);
            for (j = 1; j <= IS->ns[i]; j++) {
               fprintf (fp, "IS%d Item %d = %d\n", (int) i, (int) j, IS->is[i][j - 1]);
            }
         }
         fclose (fp);
      }
#endif
      preErrSprintf ("Inside ReadGrib2Record.. Problems in MetaParse\n");
      return -4;
   }

   if ((majEarth > 6000) && (majEarth < 7000)) {
      if ((minEarth > 6000) && (minEarth < 7000)) {
         meta->gds.f_sphere = 0;
         meta->gds.majEarth = majEarth;
         meta->gds.minEarth = minEarth;
      } else {
         meta->gds.f_sphere = 1;
         meta->gds.majEarth = majEarth;
         meta->gds.minEarth = majEarth;
      }
   }

   /* Figure out an equation to pass to ParseGrid to convert the units for
    * this grid. */
/*
   if (ComputeUnit (meta->pds2.prodType, meta->pds2.sect4.templat,
                    meta->pds2.sect4.cat, meta->pds2.sect4.subcat, f_unit,
                    &unitM, &unitB, unitName) == 0) {
*/
   if (ComputeUnit (meta->convert, meta->unitName, f_unit, &unitM, &unitB,
                    unitName) == 0) {
      unitLen = strlen (unitName);
      meta->unitName = (char *) realloc ((void *) (meta->unitName),
                                         1 + unitLen * sizeof (char));
      strncpy (meta->unitName, unitName, unitLen);
      meta->unitName[unitLen] = '\0';
   }

   /* compute the subgrid. */
   /*
   if ((lwlf->lat != -100) && (uprt->lat != -100)) {
      Nx = meta->gds.Nx;
      Ny = meta->gds.Ny;
      if (computeSubGrid (lwlf, &x1, &y1, uprt, &x2, &y2, &(meta->gds),
                          &newGds) != 0) {
         preErrSprintf ("ERROR: In compute subgrid.\n");
         return 1;
      }
      // I couldn't decide if I should "permanently" change the GDS or not.
      // when I wrote computeSubGrid.  If next line stays, really should
      // rewrite computeSubGrid.
      memcpy (&(meta->gds), &newGds, sizeof (gdsType));
      f_subGrid = 1;
   } else {
      Nx = meta->gds.Nx;
      Ny = meta->gds.Ny;
      x1 = 1;
      x2 = Nx;
      y1 = 1;
      y2 = Ny;
      f_subGrid = 0;
   }
   */
   Nx = meta->gds.Nx;
   Ny = meta->gds.Ny;
   x1 = 1;
   x2 = Nx;
   y1 = 1;
   y2 = Ny;
   f_subGrid = 0;

   /* Figure out if we need iain or ain, and set it to Grib_Data.  At the
    * same time handle any bitmaps, and compute some statistics. */
   if ((f_subGrid) && (meta->gds.scan != 64)) {
      errSprintf ("Can not do a subgrid of non scanmode 64 grid yet.\n");
      return -3;
   }

   if (strcmp (meta->element, "Wx") != 0) {
      ParseGrid (&(meta->gridAttrib), Grib_Data, grib_DataLen, Nx, Ny,
                 meta->gds.scan, IS->iain, ibitmap, IS->ib, unitM, unitB, 0,
                 NULL, f_subGrid, x1, y1, x2, y2);
   } else {
      /* Handle weather grid.  ParseGrid looks up the values... If they are
       * "<Invalid>" it sets it to missing (or creates one).  If the table
       * entry is used it sets f_valid to 2. */
      ParseGrid (&(meta->gridAttrib), Grib_Data, grib_DataLen, Nx, Ny,
                 meta->gds.scan, IS->iain, ibitmap, IS->ib, unitM, unitB, 1,
                 (sect2_WxType *) &(meta->pds2.sect2.wx), f_subGrid, x1, y1,
                 x2, y2);

      /* compact the table to only those which are actually used. */
      cnt = 0;
      for (i = 0; i < meta->pds2.sect2.wx.dataLen; i++) {
         if (meta->pds2.sect2.wx.ugly[i].f_valid == 2) {
            meta->pds2.sect2.wx.ugly[i].validIndex = cnt;
            cnt++;
         } else if (meta->pds2.sect2.wx.ugly[i].f_valid == 3) {
            meta->pds2.sect2.wx.ugly[i].f_valid = 0;
            meta->pds2.sect2.wx.ugly[i].validIndex = cnt;
            cnt++;
         } else {
            meta->pds2.sect2.wx.ugly[i].validIndex = -1;
         }
      }
   }

   /* Figure out some other non-section oriented meta data. */
/*   strftime (meta->refTime, 20, "%Y%m%d%H%M",
             gmtime (&(meta->pds2.refTime)));
*/
   Clock_Print (meta->refTime, 20, meta->pds2.refTime, "%Y%m%d%H%M", 0);
/*
   strftime (meta->validTime, 20, "%Y%m%d%H%M",
             gmtime (&(meta->pds2.sect4.validTime)));
*/
   Clock_Print (meta->validTime, 20, meta->pds2.sect4.validTime,
                "%Y%m%d%H%M", 0);

   meta->deltTime = (sInt4) (meta->pds2.sect4.validTime - meta->pds2.refTime);

   return 0;
}
