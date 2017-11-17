/*****************************************************************************
 * fileendian.c
 *
 * DESCRIPTION
 *    This file contains all the utility functions that the Driver uses to
 * solve endian'ness related issues.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL / RSIS): Created.
 *  12/2002 Rici Yu, Fangyu Chi, Mark Armstrong, & Tim Boyer
 *          (RY,FC,MA,&TB): Code Review 2.
 *
 * NOTES
 *****************************************************************************
 */
#include <stdio.h>
#include <string.h>
#include "fileendian.h"

/*****************************************************************************
 * norfread() -- Review 12/2006
 *
 * Bas Retsios / ITC
 *
 * PURPOSE
 *   To map the #defined FREAD_BIG and FREAD_LIT to DataSource fread instead of "fread"
 *
 * ARGUMENTS
 *       Dst = The destination for the data. (Output)
 * elem_size = The size of a single element. (Input)
 *  num_elem = The number of elements in Src. (Input)
 *        fp = The file to read from. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct seek position.
 *
 * RETURNS: size_t
 *   Number of elements read.
 *
 * HISTORY
 *   12/2006 Bas Retsios (ITC): Created.
 *****************************************************************************
 */

size_t norfread (void *Dst, size_t elem_size, size_t num_elem, DataSource &fp)
{
	return fp.DataSourceFread(Dst, elem_size, num_elem);
}

/*****************************************************************************
 * revfread() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fread", but in a reverse manner.
 *
 * ARGUMENTS
 *       Dst = The destination for the data. (Output)
 * elem_size = The size of a single element. (Input)
 *  num_elem = The number of elements in Src. (Input)
 *        fp = The file to read from. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS: size_t
 *   Number of elements read.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 *   Decided to read it in and then swap.  The thought here being that it is
 * faster than a bunch of fgetc.  This is the exact opposite method as
 * revfwrite.
 *****************************************************************************
 */
size_t revfread (void *Dst, size_t elem_size, size_t num_elem, DataSource &fp)
{
   size_t ans;          /* The answer from fread. */
   size_t j;            /* Byte count. */
   char *dst;           /* Allows us to treat Dst as an array of char. */
   char temp;           /* A temporary holder of a byte when swapping. */
   char *ptr, *ptr2;    /* Pointers to the two bytes to swap. */

   ans = fp.DataSourceFread(Dst, elem_size, num_elem);
   if (elem_size == 1) {
      return ans;
   }
   if (ans == num_elem) {
      dst = (char *) Dst;
      for (j = 0; j < elem_size * num_elem; j += elem_size) {
         ptr = dst + j;
         ptr2 = ptr + elem_size - 1;
         while (ptr2 > ptr) {
            temp = *ptr;
            *(ptr++) = *ptr2;
            *(ptr2--) = temp;
         }
      }
   }
   return ans;
}

/*****************************************************************************
 * revfwrite() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fwrite", but in a reverse manner.
 *
 * ARGUMENTS
 *       Src = The source of the data. (Input)
 * elem_size = The size of a single element. (Input)
 *  num_elem = The number of elements in Src. (Input)
 *        fp = The file to write to. (Output)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS:
 *   Returns number of elements written, or EOF on error.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 Arthur Taylor (MDL/RSIS): Updated.
 *  12/2002 (RY,FC,MA,&TB): Code Review.
 *
 * NOTES
 *   Decided to write using a bunch of fput, since this is buffered.  The
 * thought here, is that it is faster than swapping memory and then writing.
 * This is the exact opposite method as revfread.
 *****************************************************************************
 */
size_t revfwrite (void *Src, size_t elem_size, size_t num_elem, FILE * fp)
{
   char *ptr;           /* Current byte to put to file. */
   size_t i;            /* Byte count */
   size_t j;            /* Element count */
   char *src;           /* Allows us to treat Src as an array of char. */

   if (elem_size == 1) {
      return fwrite (Src, elem_size, num_elem, fp);
   } else {
      src = (char *) Src;
      ptr = src - elem_size - 1;
      for (j = 0; j < num_elem; ++j) {
         ptr += 2 * elem_size;
         for (i = 0; i < elem_size; ++i) {
            if (fputc ((int) *(ptr--), fp) == EOF) {
               return 0;
            }
         }
      }
      return num_elem;
   }
}

/*****************************************************************************
 * FREAD_ODDINT_BIG() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fread" into a sInt4, but in a reverse manner with not
 * necessarily all 4 bytes.  It reads big endian data from disk.
 *
 * ARGUMENTS
 * dst = Where to store the data. (Output)
 * len = The number of bytes to read. (<= 4) (Input)
 *  fp = The file to read from. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS:
 *   Returns number of elements read, or EOF on error.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
size_t FREAD_ODDINT_BIG (sInt4 * dst, uChar len, DataSource &fp)
{
   *dst = 0;
#ifdef LITTLE_ENDIAN
   return revfread (dst, len, 1, fp);
#else
   return norfread ((((char *) dst) + (4 - len)), len, 1, fp);
#endif
}

/*****************************************************************************
 * FREAD_ODDINT_LIT() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fread" into a sInt4, but in a reverse manner with not
 * necessarily all 4 bytes.  It reads little endian data from disk.
 *
 * ARGUMENTS
 * dst = Where to store the data. (Output)
 * len = The number of bytes to read. (<= 4) (Input)
 *  fp = The file to read from. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS:
 *   Returns number of elements read, or EOF on error.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
size_t FREAD_ODDINT_LIT (sInt4 * dst, uChar len, DataSource &fp)
{
   *dst = 0;
#ifdef LITTLE_ENDIAN
   return norfread (dst, len, 1, fp);
#else
   return revfread ((((char *) dst) + (4 - len)), len, 1, fp);
#endif
}

/*****************************************************************************
 * FWRITE_ODDINT_BIG() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fwrite" from a sInt4, but in a reverse manner with not
 * necessarily all 4 bytes.  It writes big endian data to disk.
 *
 * ARGUMENTS
 * src = Where to read the data from. (Output)
 * len = The number of bytes to read. (<= 4) (Input)
 *  fp = The file to write the data to. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS:
 *   Returns number of elements written, or EOF on error.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
size_t FWRITE_ODDINT_BIG (sInt4 * src, uChar len, FILE * fp)
{
#ifdef LITTLE_ENDIAN
   return revfwrite (src, len, 1, fp);
#else
   return fwrite ((((char *) src) + (4 - len)), len, 1, fp);
#endif
}

/*****************************************************************************
 * FWRITE_ODDINT_LIT() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To do an "fwrite" from a sInt4, but in a reverse manner with not
 * necessarily all 4 bytes.  It writes little endian data to disk.
 *
 * ARGUMENTS
 * src = Where to read the data from. (Output)
 * len = The number of bytes to read. (<= 4) (Input)
 *  fp = The file to write the data to. (Input)
 *
 * FILES/DATABASES:
 *   It is assumed that file is already opened and in the correct place.
 *
 * RETURNS:
 *   Returns number of elements written, or EOF on error.
 *
 * HISTORY
 *  12/2004 Arthur Taylor (MDL): Created.
 *
 * NOTES
 *****************************************************************************
 */
size_t FWRITE_ODDINT_LIT (sInt4 * src, uChar len, FILE * fp)
{
#ifdef LITTLE_ENDIAN
   return fwrite (src, len, 1, fp);
#else
   return revfwrite ((((char *) src) + (4 - len)), len, 1, fp);
#endif
}

/*****************************************************************************
 * fileBitRead() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To get bits from the file.  Stores the current byte, and passes the
 * bits that were requested to the user.  Leftover bits, are stored in a
 * gbuf, which should be passed in for future reads.
 *   If numBits == 0, then flush the gbuf.
 *
 * ARGUMENTS
 *      dst = The storage place for the data read from file. (Output)
 *   dstLen = The size of dst (in bytes) (Input)
 * num_bits = The number of bits to read from the file. (Input)
 *       fp = The open file to read from. (Input)
 *     gbuf = The current bit buffer (Input/Output)
 *  gbufLoc = Where we are in the current bit buffer. (Input/Output)
 *
 * RETURNS:
 *   EOF if EOF, 1 if error, 0 otherwise.
 *
 * NOTES
 *****************************************************************************
 */
int fileBitRead (void *Dst, size_t dstLen, uShort2 num_bits, FILE * fp,
                 uChar * gbuf, sChar * gbufLoc)
{
   static uChar BitRay[] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };
   uChar buf_loc, buf, *ptr;
   uChar *dst = (uChar*)Dst;
   size_t num_bytes;
   uChar dst_loc;
   int c;

   memset (Dst, 0, dstLen);

   if (num_bits == 0) {
      *gbuf = 0;
      *gbufLoc = 0;
      return 0;
   }

   /* Since num_bits is always used with -1, I might as well do --num_bits
    * here. */
   num_bytes = ((--num_bits) / 8) + 1; /* 1..8 bits = 1 byte, ... */
   /* Check if dst has enough room for num_bits. */
   if (dstLen < num_bytes) {
      return 1;
   }

   /* num_bits was modified earlier. */
   dst_loc = (uChar) ((num_bits % 8) + 1);
   buf_loc = *gbufLoc;
   buf = *gbuf;

#ifdef LITTLE_ENDIAN
   ptr = dst + (num_bytes - 1);
#else
   ptr = dst + (dstLen - num_bytes);
#endif

   /* Deal with initial "remainder" part (most significant byte) in dst. */
   if (buf_loc >= dst_loc) {
      /* can now deal with entire "remainder". */
#ifdef LITTLE_ENDIAN
      *(ptr--) |= (uChar) ((buf & BitRay[buf_loc]) >> (buf_loc - dst_loc));
#else
      *(ptr++) |= (uChar) ((buf & BitRay[buf_loc]) >> (buf_loc - dst_loc));
#endif
      buf_loc -= dst_loc;
   } else {
      /* need to do 2 calls to deal with entire "remainder". */
      if (buf_loc != 0) {
         *ptr |= (uChar) ((buf & BitRay[buf_loc]) << (dst_loc - buf_loc));
      }
      /* buf_loc is now 0. so we need more data. */
      /* dst_loc is now dst_loc - buf_loc. */
      if ((c = fgetc (fp)) == EOF) {
         *gbufLoc = buf_loc;
         *gbuf = buf;
         return EOF;
      }
      /* buf_loc should be 8 */
      buf = (uChar) c;
      /* 8 - (dst_loc - buf_loc) */
      buf_loc += (uChar) (8 - dst_loc);
      /* Need mask in case right shift with sign extension? Should be ok
       * since buf is a uChar, so it fills with 0s. */
#ifdef LITTLE_ENDIAN
      *(ptr--) |= (uChar) (buf >> buf_loc);
#else
      *(ptr++) |= (uChar) (buf >> buf_loc);
#endif
      /* buf_loc should now be 8 - (dst_loc - buf_loc) */
   }

   /* Note buf_loc < dst_loc from here on.  Either it is 0 or < 8. */
   /* Also dst_loc is always 8 from here out. */
#ifdef LITTLE_ENDIAN
   while (ptr >= dst) {
#else
   while (ptr < dst + dstLen) {
#endif
      if (buf_loc != 0) {
         *ptr |= (uChar) ((buf & BitRay[buf_loc]) << (8 - buf_loc));
      }
      /* buf_loc is now 0. so we need more data. */
      if ((c = fgetc (fp)) == EOF) {
         *gbufLoc = buf_loc;
         *gbuf = buf;
         return EOF;
      }
      buf = (uChar) c;
      /* Need mask in case right shift with sign extension? Should be ok
       * since buf is a uChar, so it fills with 0s. */
#ifdef LITTLE_ENDIAN
      *(ptr--) |= (uChar) (buf >> buf_loc);
#else
      *(ptr++) |= (uChar) (buf >> buf_loc);
#endif
   }

   *gbufLoc = buf_loc;
   *gbuf = buf;
   return 0;
}

/*****************************************************************************
 * fileBitWrite() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To write bits from src out to file.  First writes out any leftover bits
 * in pbuf, then bits from src.  Any leftover bits that aren't on a full byte
 * boundary, are stored in pbuf.
 *   If numBits == 0, then flush the pbuf.
 *
 * ARGUMENTS
 *     src = The data to put out to file. (Input)
 *  srcLen = Length in bytes of src. (Input)
 * numBits = The number of bits to write to file. (Input)
 *      fp = The opened file ptr to write to. (Input)
 *    pbuf = The extra bit buffer (Input/Output)
 * pBufLoc = The location in the bit buffer.
 *
 * FILES/DATABASES: None
 *
 * RETURNS:
 *   1 if error, 0 otherwise
 *
 * HISTORY
 *   8/2004 Arthur Taylor (MDL): Created
 *
 * NOTES
 *****************************************************************************
 */
char fileBitWrite (void *Src, size_t srcLen, uShort2 numBits, FILE * fp,
                   uChar * pbuf, sChar * pbufLoc)
{
   uChar buf_loc, buf, *ptr;
   uChar *src = (uChar*)Src;
   size_t num_bytes;
   uChar src_loc;

   if (numBits == 0) {
      if (*pbufLoc != 8) {
         fputc ((int) *pbuf, fp);
         *pbuf = 0;
         *pbufLoc = 8;
         return 8;
      } else {
         *pbuf = 0;
         *pbufLoc = 8;
         return 0;
      }
   }
   /* Since numBits is always used with -1, I might as well do --numBits
    * here. */
   num_bytes = ((--numBits) / 8) + 1; /* 1..8 bits = 1 byte, ... */
   /* Check if src has enough bits for us to put out. */
   if (srcLen < num_bytes) {
      return 1;
   }

   /* num_bits was modified earlier. */
   src_loc = (uChar) ((numBits % 8) + 1);
   buf_loc = *pbufLoc;
   buf = *pbuf;

   /* Get to start of interesting part of src. */
#ifdef LITTLE_ENDIAN
   ptr = src + (num_bytes - 1);
#else
   ptr = src + (srcLen - num_bytes);
#endif

   /* Deal with most significant byte in src. */
   if (buf_loc >= src_loc) {
      /* can store entire MSB in buf. */
      /* Mask? ... Safer to do so... Particularly if user has a number where
       * she wants us to start saving half way through. */
#ifdef LITTLE_ENDIAN
      buf |= (uChar) ((*(ptr--) & ((1 << src_loc) - 1)) <<
                      (buf_loc - src_loc));
#else
      buf |= (uChar) ((*(ptr++) & ((1 << src_loc) - 1)) <<
                      (buf_loc - src_loc));
#endif
      buf_loc -= src_loc;
   } else {
      /* need to do 2 calls to store the MSB. */
      if (buf_loc != 0) {
         buf |= (uChar) ((*ptr & ((1 << src_loc) - 1)) >>
                         (src_loc - buf_loc));
      }
      /* buf_loc is now 0, so we write it out. */
      if (fputc ((int) buf, fp) == EOF) {
         *pbufLoc = buf_loc;
         *pbuf = buf;
         return 1;
      }
      buf = (uChar) 0;
      /* src_loc is now src_loc - buf_loc */
      /* store rest of ptr in buf. So left shift by 8 - (src_loc -buf_loc)
       * and set buf_loc to 8 - (src_loc - buf_loc) */
      buf_loc += (uChar) (8 - src_loc);
#ifdef LITTLE_ENDIAN
      buf |= (uChar) (*(ptr--) << buf_loc);
#else
      buf |= (uChar) (*(ptr++) << buf_loc);
#endif
   }
   /* src_loc should always be considered 8 from now on.. */

#ifdef LITTLE_ENDIAN
   while (ptr >= src) {
#else
   while (ptr < src + srcLen) {
#endif
      if (buf_loc == 0) {
         /* Simple case where buf and src line up.. */
         if (fputc ((int) buf, fp) == EOF) {
            *pbufLoc = buf_loc;
            *pbuf = buf;
            return 1;
         }
#ifdef LITTLE_ENDIAN
         buf = (uChar) * (ptr--);
#else
         buf = (uChar) * (ptr++);
#endif
      } else {
         /* No mask since src_loc is considered 8. */
         /* Need mask in case right shift with sign extension? Should be ok
          * since *ptr is a uChar so it fills with 0s. */
         buf |= (uChar) ((*ptr) >> (8 - buf_loc));
         /* buf_loc is now 0, so we write it out. */
         if (fputc ((int) buf, fp) == EOF) {
            *pbufLoc = buf_loc;
            *pbuf = buf;
            return 1;
         }
         buf = (uChar) 0;
         /* src_loc is 8-buf_loc... */
         /* need to left shift by 8 - (8-buf_loc) */
#ifdef LITTLE_ENDIAN
         buf |= (uChar) (*(ptr--) << buf_loc);
#else
         buf |= (uChar) (*(ptr++) << buf_loc);
#endif
      }
   }
   /* We would rather not keep a full bit buffer. */
   if (buf_loc == 0) {
      if (fputc ((int) buf, fp) == EOF) {
         *pbufLoc = buf_loc;
         *pbuf = buf;
         return 1;
      }
      buf_loc = 8;
      buf = (uChar) 0;
   }
   *pbufLoc = buf_loc;
   *pbuf = buf;
   return 0;
}
