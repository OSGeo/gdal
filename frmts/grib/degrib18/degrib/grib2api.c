/*****************************************************************************
 * grib2api.c
 *
 * DESCRIPTION
 *    This file contains the API to the GRIB2 libraries which is as close as
 * possible to the "official" NWS GRIB2 Library's API.  The reason for this
 * is so we can use NCEP's unofficial GRIB2 library while having minimal
 * impact on the already written drivers.
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "grib2api.h"
#include "grib2.h"
#include "scan.h"
#include "memendian.h"
#include "myassert.h"
#include "gridtemplates.h"
#include "pdstemplates.h"
#include "drstemplates.h"

//#include "config.h" /*config.h created by configure - ADT mod*/

/* Declare the external FORTRAN routine
 * gcc has two __ if there is one _ in the procedure name. */
#ifdef _FORTRAN
   extern void UNPK_G2MDL
   (sInt4 * kfildo, sInt4 * jmin, sInt4 * lbit, sInt4 * nov, sInt4 * iwork,
   float * ain, sInt4 * iain, sInt4 * nd2x3, sInt4 * idat, sInt4 * nidat,
   float * rdat, sInt4 * nrdat, sInt4 * is0, sInt4 * ns0, sInt4 * is1,
   sInt4 * ns1, sInt4 * is2, sInt4 * ns2, sInt4 * is3, sInt4 * ns3,
   sInt4 * is4, sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6,
   sInt4 * ns6, sInt4 * is7, sInt4 * ns7, sInt4 * ib, sInt4 * ibitmap,
   sInt4 * ipack, sInt4 * nd5, float * xmissp, float * xmisss,
   sInt4 * inew, sInt4 * iclean, sInt4 * l3264b, sInt4 * iendpk, sInt4 * jer,
   sInt4 * ndjer, sInt4 * kjer);

   extern void PK_G2MDL
   (sInt4 * kfildo, sInt4 * jmax, sInt4 * jmin, sInt4 * lbit, sInt4 * nov,
   sInt4 * misslx, float * a, sInt4 * ia, sInt4 * newbox, sInt4 * newboxp,
   float * ain, sInt4 * iain, sInt4 * nx, sInt4 * ny, sInt4 * idat,
   sInt4 * nidat, float * rdat, sInt4 * nrdat, sInt4 * is0, sInt4 * ns0,
   sInt4 * is1, sInt4 * ns1, sInt4 * is3, sInt4 * ns3, sInt4 * is4,
   sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6, sInt4 * ns6,
   sInt4 * is7, sInt4 * ns7, sInt4 * ib, sInt4 * ibitmap, sInt4 * ipack,
   sInt4 * nd5, sInt4 * missp, float * xmissp, sInt4 * misss,
   float * xmisss, sInt4 * inew, sInt4 * minpk, sInt4 * iclean,
   sInt4 * l3264b, sInt4 * jer, sInt4 * ndjer, sInt4 * kjer);
#endif

/*****************************************************************************
 * mdl_LocalUnpack() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Unpack the local use data assuming that it was packed using the MDL
 * encoder.  This assumes "local" starts at octet 6 (ie skipping over the
 * length and section ID octets)
 *
 *   In Section 2, GRIB2 provides for local use data.  The MDL encoder packs
 * that data, and signifies that it has done so by setting octet 6 to 1.
 * This may have been inadvisable, since the GRIB2 specs did not state that
 * octet 6 was special.
 *
 * ARGUMENTS
 *    local = The section 2 data prior to being unpacked. (Input)
 * locallen = The length of "local" (Input)
 *     idat = Unpacked MDL local data (if it was an integer) (Output)
 *    nidat = length of idat. (Input)
 *     rdat = Unpacked MDL local data (if it was a float) (Output)
 *    nrdat = length of rdat. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 *  1 = Mixed local data types?
 *  2 = nrdat is not large enough.
 *  3 = nidat is not large enough.
 *  4 = Too many bits to unpack, should be a max of 32 bits.
 *  5 = Locallen is too small
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#define GRIB_UNSIGN_INT2(a,b) ((a<<8)+b)
static int mdl_LocalUnpack (unsigned char *local, sInt4 locallen,
                            sInt4 * idat, sInt4 * nidat, float * rdat,
                            sInt4 * nrdat)
{
   int BytesUsed = 0;   /* How many bytes have been used. */
   unsigned int numGroup; /* Number of groups */
   int i;               /* Counter over the groups. */
   sInt4 numVal;        /* Number of values in a given group. */
   float refVal;       /* The reference value in a given group. */
   unsigned int scale;  /* The power of 10 scale value in the group. */
   sInt4 recScale10;    /* 1 / 10**scale.. For faster computations. */
   unsigned char numBits; /* # of bits for a single element in the group. */
   char f_dataType;     /* If the local data is a float or integer. */
   char f_firstType = 0; /* The type of the first group of local data. */
   int curIndex = 0;    /* Where to store the current data. */
   int j;               /* Counter over the number of values in a group. */
   size_t numUsed;      /* Number of bytes used in call to memBitRead. */
   uChar bufLoc;   /* Where to read for more bits in the data stream. */
   uInt4 uli_temp; /* Temporary storage to hold unpacked data. */

   if (locallen < BytesUsed + 3) {
#ifdef DEBUG
      printf ("Locallen is too small.\n");
#endif
      return 5;
   }
   /* The calling routine should check octet 6, which is local[0], to be 1,
    * so we just assert it is 1. */
   myAssert (local[0] == 1);
   numGroup = GRIB_UNSIGN_INT2 (local[1], local[2]);
   local += 3;
   BytesUsed += 3;
   myAssert (*nrdat > 1);
   myAssert (*nidat > 1);
   idat[0] = 0;
   rdat[0] = 0;

   for (i = 0; i < numGroup; i++) {
      if (locallen < BytesUsed + 12) {
#ifdef DEBUG
         printf ("Locallen is too small.\n");
#endif
         return 5;
      }
      MEMCPY_BIG (&numVal, local, sizeof (sInt4));
      MEMCPY_BIG (&refVal, local + 4, sizeof (float));
      scale = GRIB_UNSIGN_INT2 (local[8], local[9]);
      recScale10 = 1 / pow (10.0, scale);
      numBits = local[10];
      if (numBits >= 32) {
#ifdef DEBUG
         printf ("Too many bits too unpack.\n");
#endif
         return 4;
      }
      f_dataType = local[11];
      local += 12;
      BytesUsed += 12;
      if (locallen < BytesUsed + ((numBits * numVal) + 7) / 8) {
#ifdef DEBUG
         printf ("Locallen is too small.\n");
#endif
         return 5;
      }
      if (i == 0) {
         f_firstType = f_dataType;
      } else if (f_firstType != f_dataType) {
#ifdef DEBUG
         printf ("Local use data has mixed float/integer type?\n");
#endif
         return 1;
      }
      bufLoc = 8;
      if (f_dataType == 0) {
         /* Floating point data. */
         if (*nrdat < (curIndex + numVal + 3)) {
#ifdef DEBUG
            printf ("nrdat is not large enough.\n");
#endif
            return 2;
         }
         rdat[curIndex] = numVal;
         curIndex++;
         rdat[curIndex] = scale;
         curIndex++;
         for (j = 0; j < numVal; j++) {
            memBitRead (&uli_temp, sizeof (sInt4), local, numBits,
                        &bufLoc, &numUsed);
            local += numUsed;
            BytesUsed += numUsed;
            rdat[curIndex] = (refVal + uli_temp) * recScale10;
            curIndex++;
         }
         rdat[curIndex] = 0;
      } else {
         /* Integer point data. */
         if (*nidat < (curIndex + numVal + 3)) {
#ifdef DEBUG
            printf ("nidat is not large enough.\n");
#endif
            return 3;
         }
         idat[curIndex] = numVal;
         curIndex++;
         idat[curIndex] = scale;
         curIndex++;
         for (j = 0; j < numVal; j++) {
            memBitRead (&uli_temp, sizeof (sInt4), local, numBits,
                        &bufLoc, &numUsed);
            local += numUsed;
            BytesUsed += numUsed;
            idat[curIndex] = (refVal + uli_temp) * recScale10;
            curIndex++;
         }
         idat[curIndex] = 0;
      }
   }
   return 0;
}

/*****************************************************************************
 * fillOutSectLen() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   The GRIB2 API returns the lengths of each GRIB2 section along with the
 * meta data.  NCEP's routines did not, so this routine fills in that part.
 * This routine has to consider which grid is being unpacked.
 *
 *   c_ipack is passed in after section 1 (since section 1 is not repeated)
 *
 * ARGUMENTS
 *  c_ipack = Complete GRIB2 message to look for the section lengths in. (In)
 * lenCpack = Length of c_ipack (Input)
 *  subgNum = Which sub rid to find the section lengths of. (Input)
 *      is2 = Section 2 data (Output)
 *      is3 = Section 3 data (Output)
 *      is4 = Section 4 data (Output)
 *      is5 = Section 5 data (Output)
 *      is6 = Section 6 data (Output)
 *      is7 = Section 7 data (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 *  1 = c_ipack is not large enough
 *  2 = invalid section number
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int fillOutSectLen (unsigned char *c_ipack, int lenCpack,
                           int subgNum, sInt4 * is2, sInt4 * is3,
                           sInt4 * is4, sInt4 * is5, sInt4 * is6,
                           sInt4 * is7)
{
   sInt4 offset = 0;    /* How far in c_ipack we have read. */
   sInt4 sectLen;       /* The length of the current section. */
   int sectId;          /* The current section number. */
   unsigned int gNum = 0; /* Which sub group we are currently working with. */

   if (lenCpack < 5) {
#ifdef DEBUG
      printf ("Cpack is not large enough.\n");
#endif
      return 1;
   }
   /* assert that we start with data in either section 2 or 3. */
   myAssert ((c_ipack[4] == 2) || (c_ipack[4] == 3));
   while (gNum <= subgNum) {
      if (lenCpack < offset + 5) {
#ifdef DEBUG
         printf ("Cpack is not large enough.\n");
#endif
         return 1;
      }
      MEMCPY_BIG (&sectLen, c_ipack + offset, sizeof (sInt4));
      /* Check if we just read section 8.  If so, then it is "7777" =
       * 926365495 regardless of endian'ness. */
      if (sectLen == 926365495L) {
#ifdef DEBUG
         printf ("Shouldn't see sect 8. Should stop after correct sect 7\n");
#endif
         return 2;
      }
      sectId = c_ipack[offset + 4];
      switch (sectId) {
         case 2:
            is2[0] = sectLen;
            break;
         case 3:
            is3[0] = sectLen;
            break;
         case 4:
            is4[0] = sectLen;
            break;
         case 5:
            is5[0] = sectLen;
            break;
         case 6:
            is6[0] = sectLen;
            break;
         case 7:
            is7[0] = sectLen;
            gNum++;
            break;
         default:
#ifdef DEBUG
            printf ("Invalid section id %d.\n", sectId);
#endif
            return 2;
      }
      offset += sectLen;
   }
   return 0;
}

/*****************************************************************************
 * TransferInt() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To transfer the data from "fld" and "bmap" to "iain" and "ib".  The API
 * attempts to rearrange the order, so that anything returned from it has
 * scan mode 0100????
 *
 * ARGUMENTS
 *          fld = The expanded grid from NCEPs routines (Input)
 *      ngrdpts = Length of fld (Input)
 *      ibitmap = flag if we have a bitmap or not. (Input)
 *         bmap = bitmap from NCEPs routines. (Input)
 * f_ignoreScan = Flag to ignore the attempt at changing the scan (Input)
 *         scan = The scan orientation of fld/bmap/iain/ib (Input/Output)
 *       nx, ny = The dimmensions of the grid. (Input)
 *       iclean = 1 means the user wants the unpacked data returned without
 *                missing values in it. 0 means embed the missing values. (In)
 *       xmissp = The primary missing value to use if iclean = 0. (Input).
 *         iain = The grid to return. (Output)
 *        nd2x3 = The length of iain. (Input)
 *           ib = Bitmap if it was packed (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 *  1 = nd2x3 is too small.
 *  2 = nx*ny != ngrdpts
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *   May want to disable the scan adjustment in the future.
 *****************************************************************************
 */
static int TransferInt (float * fld, sInt4 ngrdpts, sInt4 ibitmap,
                        sInt4 * bmap, char f_ignoreScan, sInt4 * scan,
                        sInt4 nx, sInt4 ny, sInt4 iclean, float xmissp,
                        sInt4 * iain, sInt4 nd2x3, sInt4 * ib)
{
   int i;               /* loop counter over all grid points. */
   sInt4 x, y;       /* Where we are in a grid of scan value 0100???? */
   int curIndex;        /* Where in iain to store the current data. */

   if (nd2x3 < ngrdpts) {
#ifdef DEBUG
      printf ("nd2x3(%d) is < ngrdpts(%d)\n", nd2x3, ngrdpts);
#endif
      return 1;
   }
   if (f_ignoreScan || ((*scan & 0xf0) == 64)) {
      if (ibitmap) {
         for (i = 0; i < ngrdpts; i++) {
            ib[i] = bmap[i];
            /* Check if we are supposed to insert xmissp into the field */
            if ((iclean != 0) && (ib[i] == 0)) {
               iain[i] = xmissp;
            } else {
               iain[i] = fld[i];
            }
         }
      } else {
         for (i = 0; i < ngrdpts; i++) {
            iain[i] = fld[i];
         }
      }
   } else {
      if (nx * ny != ngrdpts) {
#ifdef DEBUG
         printf ("nx * ny (%d) != ngrdpts(%d)\n", nx * ny, ngrdpts);
#endif
         return 2;
      }
      if (ibitmap) {
         for (i = 0; i < ngrdpts; i++) {
            ScanIndex2XY (i, &x, &y, *scan, nx, ny);
            /* ScanIndex returns value as if scan was 0100(0000) */
            curIndex = (x - 1) + (y - 1) * nx;
            myAssert (curIndex < nd2x3);
            ib[curIndex] = bmap[i];
            /* Check if we are supposed to insert xmissp into the field */
            if ((iclean != 0) && (ib[curIndex] == 0)) {
               iain[i] = xmissp;
            } else {
               iain[curIndex] = fld[i];
            }
         }
      } else {
         for (i = 0; i < ngrdpts; i++) {
            ScanIndex2XY (i, &x, &y, *scan, nx, ny);
            /* ScanIndex returns value as if scan was 0100(0000) */
            curIndex = (x - 1) + (y - 1) * nx;
            myAssert (curIndex < nd2x3);
            iain[curIndex] = fld[i];
         }
      }
      *scan = 64 + (*scan & 0x0f);
   }
   return 0;
}

/*****************************************************************************
 * TransferFloat() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To transfer the data from "fld" and "bmap" to "ain" and "ib".  The API
 * attempts to rearrange the order, so that anything returned from it has
 * scan mode 0100????
 *
 * ARGUMENTS
 *          fld = The expanded grid from NCEPs routines (Input)
 *      ngrdpts = Length of fld (Input)
 *      ibitmap = flag if we have a bitmap or not. (Input)
 *         bmap = bitmap from NCEPs routines. (Input)
 *         scan = The scan orientation of fld/bmap/iain/ib (Input/Output)
 * f_ignoreScan = Flag to ignore the attempt at changing the scan (Input)
 *       nx, ny = The dimmensions of the grid. (Input)
 *       iclean = 1 means the user wants the unpacked data returned without
 *                missing values in it. 0 means embed the missing values. (In)
 *       xmissp = The primary missing value to use if iclean = 0. (Input).
 *          ain = The grid to return. (Output)
 *        nd2x3 = The length of iain. (Input)
 *           ib = Bitmap if it was packed (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 *  1 = nd2x3 is too small.
 *  2 = nx*ny != ngrdpts
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *   May want to disable the scan adjustment in the future.
 *****************************************************************************
 */
static int TransferFloat (float * fld, sInt4 ngrdpts, sInt4 ibitmap,
                          sInt4 * bmap, char f_ignoreScan, sInt4 * scan,
                          sInt4 nx, sInt4 ny, sInt4 iclean, float xmissp,
                          float * ain, sInt4 nd2x3, sInt4 * ib)
{
   int i;               /* loop counter over all grid points. */
   sInt4 x, y;       /* Where we are in a grid of scan value 0100???? */
   int curIndex;        /* Where in ain to store the current data. */

   if (nd2x3 < ngrdpts) {
#ifdef DEBUG
      printf ("nd2x3(%d) is < ngrdpts(%d)\n", nd2x3, ngrdpts);
#endif
      return 1;
   }
   if (f_ignoreScan || ((*scan & 0xf0) == 64)) {
      if (ibitmap) {
         for (i = 0; i < ngrdpts; i++) {
            ib[i] = bmap[i];
            /* Check if we are supposed to insert xmissp into the field */
            if ((iclean != 0) && (ib[i] == 0)) {
               ain[i] = xmissp;
            } else {
               ain[i] = fld[i];
            }
         }
      } else {
         for (i = 0; i < ngrdpts; i++) {
            ain[i] = fld[i];
         }
      }
   } else {
      if (nx * ny != ngrdpts) {
#ifdef DEBUG
         printf ("nx * ny (%d) != ngrdpts(%d)\n", nx * ny, ngrdpts);
#endif
         return 2;
      }
      if (ibitmap) {
         for (i = 0; i < ngrdpts; i++) {
            ScanIndex2XY (i, &x, &y, *scan, nx, ny);
            /* ScanIndex returns value as if scan was 0100(0000) */
            curIndex = (x - 1) + (y - 1) * nx;
            myAssert (curIndex < nd2x3);
            ib[curIndex] = bmap[i];
            /* Check if we are supposed to insert xmissp into the field */
            if ((iclean != 0) && (ib[curIndex] == 0)) {
               ain[i] = xmissp;
            } else {
               ain[curIndex] = fld[i];
            }
         }
      } else {
         for (i = 0; i < ngrdpts; i++) {
            ScanIndex2XY (i, &x, &y, *scan, nx, ny);
            /* ScanIndex returns value as if scan was 0100(0000) */
            curIndex = (x - 1) + (y - 1) * nx;
            myAssert (curIndex < nd2x3);
            ain[curIndex] = fld[i];
         }
      }
      *scan = 64 + (*scan & 0x0f);
   }
/*
   if (1==1) {
      int i;
      for (i=0; i < ngrdpts; i++) {
         if (ain[i] != 9999) {
            fprintf (stderr, "grib2api.c: ain[%d] %f, fld[%d] %f\n", i, ain[i],
                     i, fld[i]);
         }
      }
   }
*/
   return 0;
}

#ifdef PKNCEP
int pk_g2ncep (sInt4 * kfildo, float * ain, sInt4 * iain, sInt4 * nx,
               sInt4 * ny, sInt4 * idat, sInt4 * nidat, float * rdat,
               sInt4 * nrdat, sInt4 * is0, sInt4 * ns0, sInt4 * is1,
               sInt4 * ns1, sInt4 * is3, sInt4 * ns3, sInt4 * is4,
               sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6,
               sInt4 * ns6, sInt4 * is7, sInt4 * ns7, sInt4 * ib,
               sInt4 * ibitmap, unsigned char *cgrib, sInt4 * nd5,
               sInt4 * missp, float * xmissp, sInt4 * misss,
               float * xmisss, sInt4 * inew, sInt4 * minpk, sInt4 * iclean,
               sInt4 * l3264b, sInt4 * jer, sInt4 * ndjer, sInt4 * kjer)
{
   g2int listsec0[2];
   g2int listsec1[13];
   g2int igds[5];
   g2int igdstmpl[];
   int ierr;            /* Holds the error code from a called routine. */
   int i;

   listsec0[0] = is0[6];
   listsec0[1] = is0[7];

   listsec1[0] = is1[5];
   listsec1[1] = is1[7];
   listsec1[2] = is1[9];
   listsec1[3] = is1[10];
   listsec1[4] = is1[11];
   listsec1[5] = is1[12];
   listsec1[6] = is1[14];
   listsec1[7] = is1[15];
   listsec1[8] = is1[16];
   listsec1[9] = is1[17];
   listsec1[10] = is1[18];
   listsec1[11] = is1[19];
   listsec1[12] = is1[20];

   ierr = g2_create (cgrib, listsec0, listsec1);
   printf ("Length = %d\n", ierr);

   if ((idat[0] != 0) || (rdat[0] != 0)) {
      printf ("Don't handle this yet.\n");
/*
      ierr = g2_addlocal (cgrib, unsigned char *csec2, g2int lcsec2);
*/
   }

   igds[0] = is3[5];
   igds[1] = is3[6];
   igds[2] = is3[10];
   igds[3] = is3[11];
   igds[4] = is3[12];

IS3(15) -  IS3(nn) = Grid Definition Template, stored in bytes 15-nn (*)

   ierr = g2_addgrid (cgrib, igds, g2int *igdstmpl, g2int *ideflist,
                      g2int idefnum)




   return 0;
/*

To start a new GRIB2 message, call function g2_create.  G2_create
encodes Sections 0 and 1 at the beginning of the message.  This routine
must be used to create each message.

Routine g2_addlocal can be used to add a Local Use Section ( Section 2 ).
Note that this section is optional and need not appear in a GRIB2 message.

Function g2_addgrid is used to encode a grid definition into Section 3.
This grid definition defines the geometry of the the data values in the
fields that follow it.  g2_addgrid can be called again to change the grid
definition describing subsequent data fields.

Each data field is added to the GRIB2 message using routine g2_addfield,
which adds Sections 4, 5, 6, and 7 to the message.

After all desired data fields have been added to the GRIB2 message, a
call to function g2_gribend is needed to add the final section 8 to the
message and to update the length of the message.  A call to g2_gribend
is required for each GRIB2 message.
*/

}
#endif

/*****************************************************************************
 * unpk_g2ncep() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This procedure is a wrapper around the NCEP GRIB2 routines to interface
 * their routines with the "official NWS" GRIB2 API.  The reason for this is
 * so drivers that have been written to use the "official NWS" GRIB2 API, can
 * use the NCEP library with minimal disruption.
 *
 * ARGUMENTS
 *  kfildo = Unit number for output diagnostics (C ignores this). (Input)
 *     ain = contains the data if the original data was float data.
 *           (size = nd2x3) (Output)
 *    iain = contains the data if the original data was integer data.
 *           (size = nd2x3) (Output)
 *   nd2x3 = length of ain, iain, ib (Input)
 *           (at least the size of num grid points)
 *    idat = local use data if any that were unpacked from Section 2. (Output)
 *   nidat = length of idat. (Input)
 *    rdat = floating point local use data (Output)
 *   nrdat = length of rdat. (Input)
 *     is0 = Section 0 data (Output)
 *     ns0 = length of is0 (16 is fine) (Input)
 *     is1 = Section 1 data (Output)
 *     ns1 = length of is1 (21 is fine) (Input)
 *     is2 = Section 2 data (Output)
 *     ns2 = length of is2 () (Input)
 *     is3 = Section 3 data (Output)
 *     ns3 = length of is3 (96 or 1600) (Input)
 *     is4 = Section 4 data (Output)
 *     ns4 = length of is4 (60) (Input)
 *     is5 = Section 5 data (Output)
 *     ns5 = length of is5 (49 is fine) (Input)
 *     is6 = Section 6 data (Output)
 *     ns6 = length of is6 (6 is fine) (Input)
 *     is7 = Section 7 data (Output)
 *     ns7 = length of is7 (8 is fine) (Input)
 *      ib = Bitmap if user requested it, and it was packed (Output)
 * ibitmap = 0 means ib is invalid, 1 means ib is valid. (Output)
 * c_ipack = The message to unpack (Input)
 *     nd5 = 1/4 the size of c_ipack (Input)
 *  xmissp = The floating point representation for the primary missing value.
 *           (Input/Output)
 *  xmisss = The floating point representation for the secondary missing
 *           value (Input/Output)
 *     new = 1 if this is the first grid to be unpacked, else 0. (Input)
 *  iclean = 1 means the user wants the unpacked data returned without
 *           missing values in it. 0 means embed the missing values. (Input)
 *  l3264b = Integer word length in bits (32 or 64) (Input)
 *  iendpk = A 1 means no more grids in this message, a 0 means more grids.
 *           (Output)
 * jer(ndjer,2) = error codes along with severity. (Output)
 *   ndjer = 1/2 length of jer. (>= 15) (Input)
 *    kjer = number of error messages stored in jer.
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *   "jer" is used to store error messages, and kjer is used to denote how
 * many there were.  Jer is set up as a 2 column array, with the first
 * column in the first half of the array, and denoting the GRIB2 section
 * an error occurred in.  The second column denoting the severity.
 *
 *    The possibilities from unpk_g2ncep() are as follows:
 * ker=1 jer[0,0]=0    jer[0,1]=2: Message is not formed correctly
 *                                 Request for an invalid subgrid
 *                                 Problems unpacking the data.
 *                                 problems expanding the data.
 *                                 Calling dimmensions were too small.
 * ker=2 jer[1,0]=100  jer[1,1]=2: Error unpacking section 1.
 * ker=3 jer[2,0]=200  jer[2,1]=2: Error unpacking section 2.
 * ker=4 jer[3,0]=300  jer[3,1]=2: Error unpacking section 3.
 * ker=5 jer[4,0]=400  jer[4,1]=2: Error unpacking section 4.
 * ker=6 jer[5,0]=500  jer[5,1]=2: Error unpacking section 5.
 *                                 Data Template not implemented.
 *                                 Durring Transfer, nx * ny != ngrdpts.
 * ker=7 jer[6,0]=600  jer[6,1]=2: Error unpacking section 6.
 * ker=8 jer[7,0]=700  jer[7,1]=2: Error unpacking section 7.
 * ker=9 jer[8,0]=2001 jer[8,1]=2: nd2x3 is not large enough.
 * ker=9 jer[8,0]=2003 jer[8,1]=2: undefined sect 3 template
 *                                 (see gridtemplates.h).
 * ker=9 jer[8,0]=2004 jer[8,1]=2: undefined sect 4 template
 *                                 (see pdstemplates.h).
 * ker=9 jer[8,0]=2005 jer[8,1]=2: undefined sect 5 template
 *                                 (see drstemplates.h).
 * ker=9 jer[8,0]=9999 jer[8,1]=2: NCEP returns an unrecognized error.
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * MDL handles is5[12], is5[23], and is5[27] in an "interesting" manner.
 * MDL attempts to always return grids in scan mode 0100????
 *
 * ToDo: Check length of parameters better.
 * ToDo: Probably shouldn't abort if they have problems expanding the data.
 * ToDo: Better handling of:
 * gfld->list_opt  = (Used if gfld->numoct_opt .ne. 0)  This array contains
 *      the number of grid points contained in each row (or column). (part of
 *      Section 3)  This element is a pointer to an array that holds the data.
 *      This pointer is nullified if gfld->numoct_opt=0.
 * gfld->num_opt = (Used if gfld->numoct_opt .ne. 0)  The number of entries in
 *      array ideflist.  i.e. number of rows (or columns) for which optional
 *      grid points are defined.  This value is set to zero, if
 *      gfld->numoct_opt=0.
 * gfld->coord_list  = Real array containing floating point values intended to
 *      document the vertical discretisation associated to model data on
 *      hybrid coordinate vertical levels.  (part of Section 4) This element
 *      is a pointer to an array that holds the data.
 * gfld->num_coord = number of values in array gfld->coord_list[].
 *****************************************************************************
 */
void unpk_g2ncep (sInt4 * kfildo, float * ain, sInt4 * iain, sInt4 * nd2x3,
                  sInt4 * idat, sInt4 * nidat, float * rdat, sInt4 * nrdat,
                  sInt4 * is0, sInt4 * ns0, sInt4 * is1, sInt4 * ns1,
                  sInt4 * is2, sInt4 * ns2, sInt4 * is3, sInt4 * ns3,
                  sInt4 * is4, sInt4 * ns4, sInt4 * is5, sInt4 * ns5,
                  sInt4 * is6, sInt4 * ns6, sInt4 * is7, sInt4 * ns7,
                  sInt4 * ib, sInt4 * ibitmap, unsigned char *c_ipack,
                  sInt4 * nd5, float * xmissp, float * xmisss,
                  sInt4 * inew, sInt4 * iclean, sInt4 * l3264b,
                  sInt4 * iendpk, sInt4 * jer, sInt4 * ndjer, sInt4 * kjer)
{
   int i;               /* A counter used for a number of purposes. */
   static unsigned int subgNum = 0; /* The sub grid we read most recently.
                                     * This is primarily to help with the
                                     * inew option. */
   int ierr;            /* Holds the error code from a called routine. */
   sInt4 listsec0[3];
   sInt4 listsec1[13];
   static sInt4 numfields = 1; /* Number of sub Grids in this message */
   sInt4 numlocal;      /* Number of local sections in this message. */
   int unpack;          /* Tell g2_getfld to unpack the message. */
   int expand;          /* Tell g2_getflt to attempt to expand the bitmap. */
   gribfield *gfld;     /* Holds the data after g2_getfld unpacks it. */
   sInt4 gridIndex;     /* index in templatesgrid[] for this sect 3 templat */
   sInt4 pdsIndex;      /* index in templatespds[] for this sect 4 template */
   sInt4 drsIndex;      /* index in templatesdrs[] for this sect 5 template */
   int curIndex;        /* Where in is3, is4, or is5 to store meta data */
   int scanIndex;       /* Where in is3 to find the scan mode. */
   int nxIndex;         /* Where in is3 to find the number of x values. */
   int nyIndex;         /* Where in is3 to find the number of y values. */
   float f_temp;       /* Assist with handling weird MDL behavior in is5[] */
   char f_ignoreScan;   /* Flag to ignore the attempt at changing the scan */
   sInt4 dummyScan;     /* Dummy place holder for call to Transfer routines
                         * if ignoring scan. */

   myAssert (*ndjer >= 8);
   /* Init the error handling array. */
   memset ((void *) jer, 0, 2 * *ndjer * sizeof (sInt4));
   for (i = 0; i < 8; i++) {
      jer[i] = i * 100;
   }
   *kjer = 8;

   /* The first time in, figure out how many grids there are, and store it
    * in numfields for subsequent calls with inew != 1. */
   if (*inew == 1) {
      subgNum = 0;
      ierr = g2_info (c_ipack, listsec0, listsec1, &numfields, &numlocal);
      if (ierr != 0) {
         switch (ierr) {
            case 1:    /* Beginning characters "GRIB" not found. */
            case 2:    /* GRIB message is not Edition 2. */
            case 3:    /* Could not find Section 1, where expected. */
            case 4:    /* End string "7777" found, but not where expected. */
            case 5:    /* End string "7777" not found at end of message. */
            case 6:    /* Invalid section number found. */
               jer[0 + *ndjer] = 2;
               *kjer = 1;
               break;
            default:
               jer[8 + *ndjer] = 2;
               jer[8] = 9999; /* Unknown error message. */
               *kjer = 9;
         }
         return;
      }
   } else {
      if (subgNum + 1 >= numfields) {
         /* Field request error. */
         jer[0 + *ndjer] = 2;
         *kjer = 1;
         return;
      }
      subgNum++;
   }

   /* Expand the desired subgrid. */
   unpack = 1;
   expand = 1;
   ierr = g2_getfld (c_ipack, subgNum + 1, unpack, expand, &gfld);
   if (ierr != 0) {
      switch (ierr) {
         case 1:       /* Beginning characters "GRIB" not found. */
         case 2:       /* GRIB message is not Edition 2. */
         case 3:       /* The data field request number was not positive. */
         case 4:       /* End string "7777" found, but not where expected. */
         case 6:       /* message did not contain requested # of fields */
         case 7:       /* End string "7777" not found at end of message. */
         case 8:       /* Unrecognized Section encountered. */
            jer[0 + *ndjer] = 2;
            *kjer = 1;
            break;
         case 15:      /* Error unpacking Section 1. */
            jer[1 + *ndjer] = 2;
            *kjer = 2;
            break;
         case 16:      /* Error unpacking Section 2. */
            jer[2 + *ndjer] = 2;
            *kjer = 3;
            break;
         case 10:      /* Error unpacking Section 3. */
            jer[3 + *ndjer] = 2;
            *kjer = 4;
            break;
         case 11:      /* Error unpacking Section 4. */
            jer[4 + *ndjer] = 2;
            *kjer = 5;
            break;
         case 9:       /* Data Template 5.NN not implemented. */
         case 12:      /* Error unpacking Section 5. */
            jer[5 + *ndjer] = 2;
            *kjer = 6;
            break;
         case 13:      /* Error unpacking Section 6. */
            jer[6 + *ndjer] = 2;
            *kjer = 7;
            break;
         case 14:      /* Error unpacking Section 7. */
            jer[7 + *ndjer] = 2;
            *kjer = 8;
            break;
         default:
            jer[8 + *ndjer] = 2;
            jer[8] = 9999; /* Unknown error message. */
            *kjer = 9;
            break;
      }
      g2_free (gfld);
      return;
   }
   /* Check if data wasn't unpacked. */
   if (!gfld->unpacked) {
      jer[0 + *ndjer] = 2;
      *kjer = 1;
      g2_free (gfld);
      return;
   }

   /* Start going through the gfld structure and converting it to the needed
    * data output formats. */
   myAssert (*ns0 >= 16);
   MEMCPY_BIG (&(is0[0]), c_ipack, sizeof (sInt4));
   is0[6] = gfld->discipline;
   is0[7] = gfld->version;
   MEMCPY_BIG (&(is0[8]), c_ipack + 8, sizeof (sInt4));
   /* The following assert fails only if the GRIB message is more that 4
    * giga-bytes large, which I think would break the fortran library. */
   myAssert (is0[8] == 0);
   MEMCPY_BIG (&(is0[8]), c_ipack + 12, sizeof (sInt4));

   myAssert (*ns1 >= 21);
   myAssert (gfld->idsectlen >= 13);
   MEMCPY_BIG (&(is1[0]), c_ipack + 16, sizeof (sInt4));
   is1[4] = c_ipack[20];
   is1[5] = gfld->idsect[0];
   is1[7] = gfld->idsect[1];
   is1[9] = gfld->idsect[2];
   is1[10] = gfld->idsect[3];
   is1[11] = gfld->idsect[4];
   is1[12] = gfld->idsect[5]; /* Year */
   is1[14] = gfld->idsect[6]; /* Month */
   is1[15] = gfld->idsect[7]; /* Day */
   is1[16] = gfld->idsect[8]; /* Hour */
   is1[17] = gfld->idsect[9]; /* Min */
   is1[18] = gfld->idsect[10]; /* Sec */
   is1[19] = gfld->idsect[11];
   is1[20] = gfld->idsect[12];

   /* Fill out section lengths (separate procedure because of possibility of
    * having multiple grids.  Should combine fillOutSectLen g2_info, and
    * g2_getfld into one procedure to optimize it. */
   fillOutSectLen (c_ipack + 16 + is1[0], 4 * *nd5 - 15 - is1[0], subgNum,
                   is2, is3, is4, is5, is6, is7);

   /* Check if there is section 2 data. */
   if (gfld->locallen > 0) {
      /* The + 1 is so we don't overwrite the section length */
      memset ((void *) (is2 + 1), 0, (*ns2 - 1) * sizeof (sInt4));
      is2[4] = 2;
      is2[5] = gfld->local[0];
      /* check if MDL Local use simple packed data */
      if (is2[5] == 1) {
         mdl_LocalUnpack (gfld->local, gfld->locallen, idat, nidat,
                          rdat, nrdat);
      } else {
         /* local use section was not MDL packed, return it in is2. */
         for (i = 0; i < gfld->locallen; i++) {
            is2[i + 5] = gfld->local[i];
         }
      }
   } else {
      /* API specified that is2[0] = 0; idat[0] = 0, and rdat[0] = 0 */
      is2[0] = 0;
      idat[0] = 0;
      rdat[0] = 0;
   }

   is3[4] = 3;
   is3[5] = gfld->griddef;
   is3[6] = gfld->ngrdpts;
   if (*nd2x3 < gfld->ngrdpts) {
      jer[8 + *ndjer] = 2;
      jer[8] = 2001;    /* nd2x3 is not large enough */
      *kjer = 9;
      g2_free (gfld);
      return;
   }
   is3[10] = gfld->numoct_opt;
   is3[11] = gfld->interp_opt;
   is3[12] = gfld->igdtnum;
   gridIndex = getgridindex (gfld->igdtnum);
   if (gridIndex == -1) {
      jer[8 + *ndjer] = 2;
      jer[8] = 2003;    /* undefined sect 3 template */
      *kjer = 9;
      g2_free (gfld);
      return;
   }
   curIndex = 14;
   for (i = 0; i < gfld->igdtlen; i++) {
      const struct gridtemplate *templatesgrid = get_templatesgrid();
      is3[curIndex] = gfld->igdtmpl[i];
      curIndex += abs (templatesgrid[gridIndex].mapgrid[i]);
   }
   /* API attempts to return grid in scan mode 0100????.  Find the necessary
    * indexes into the is3 array for the attempt. */
   switch (gfld->igdtnum) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 40:
      case 41:
      case 42:
      case 43:
         scanIndex = 72 - 1;
         nxIndex = 31 - 1;
         nyIndex = 35 - 1;
         break;
      case 10:
         scanIndex = 60 - 1;
         nxIndex = 31 - 1;
         nyIndex = 35 - 1;
         break;
      case 20:
      case 30:
      case 31:
         scanIndex = 65 - 1;
         nxIndex = 31 - 1;
         nyIndex = 35 - 1;
         break;
      case 90:
         scanIndex = 64 - 1;
         nxIndex = 31 - 1;
         nyIndex = 35 - 1;
         break;
      case 110:
         scanIndex = 57 - 1;
         nxIndex = 31 - 1;
         nyIndex = 35 - 1;
         break;
      case 50:
      case 51:
      case 52:
      case 53:
      case 100:
      case 120:
      case 1000:
      case 1200:
      default:
         scanIndex = -1;
         nxIndex = -1;
         nyIndex = -1;
   }

   is4[4] = 4;
   is4[5] = gfld->num_coord;
   is4[7] = gfld->ipdtnum;
   pdsIndex = getpdsindex (gfld->ipdtnum);
   if (pdsIndex == -1) {
      jer[8 + *ndjer] = 2;
      jer[8] = 2004;    /* undefined sect 4 template */
      *kjer = 9;
      g2_free (gfld);
      return;
   }
   curIndex = 9;
   for (i = 0; i < gfld->ipdtlen; i++) {
      const struct pdstemplate *templatespds = get_templatespds();
      is4[curIndex] = gfld->ipdtmpl[i];
      curIndex += abs (templatespds[pdsIndex].mappds[i]);
   }

   is5[4] = 5;
   is5[5] = gfld->ndpts;
   is5[9] = gfld->idrtnum;
   drsIndex = getdrsindex (gfld->idrtnum);
   if (drsIndex == -1) {
      jer[8 + *ndjer] = 2;
      jer[8] = 2005;    /* undefined sect 5 template */
      *kjer = 9;
      g2_free (gfld);
      return;
   }
   curIndex = 11;
   for (i = 0; i < gfld->idrtlen; i++) {
      const struct drstemplate *templatesdrs = get_templatesdrs();
      is5[curIndex] = gfld->idrtmpl[i];
      curIndex += abs (templatesdrs[drsIndex].mapdrs[i]);
   }
   /* Mimic MDL's handling of reference value (IS5(12)) */
   memcpy (&f_temp, &(is5[11]), sizeof (float));
   is5[11] = (sInt4) f_temp;
   if ((is5[9] == 2) || (is5[9] == 3)) {
      if (is5[20] == 0) {
         memcpy (&(f_temp), &(is5[23]), sizeof (float));
         *xmissp = f_temp;
         is5[23] = (sInt4) f_temp;
         memcpy (&(f_temp), &(is5[27]), sizeof (float));
         *xmisss = f_temp;
         is5[27] = (sInt4) f_temp;
      } else {
         *xmissp = is5[23];
         *xmisss = is5[27];
      }
   }

   is6[4] = 6;
   is6[5] = gfld->ibmap;
   is7[4] = 7;

   if (subgNum + 1 == numfields) {
      *iendpk = 1;
   } else {
      *iendpk = 0;
   }

   if ((gfld->ibmap == 0) || (gfld->ibmap == 254)) {
      *ibitmap = 1;
   } else {
      *ibitmap = 0;
   }

   /* Check type of original field, before transfering the memory. */
   myAssert (*ns5 > 20);
   /* Check if NCEP had problems expanding the data.  If so we currently
    * abort.  May need to revisit this behavior. */
   if (!gfld->expanded) {
      jer[0 + *ndjer] = 2;
      *kjer = 1;
      g2_free (gfld);
      return;
   }
   f_ignoreScan = 0;
   /* Check if integer type... is5[20] == 1 implies integer (code table
    * 5.1), but only for certain templates. (0,1,2,3,40,41,40000,40010).
    * (not 50,51) */
   if ((is5[20] == 1) && ((is5[9] != 50) && (is5[9] != 51))) {
      /* integer data, use iain */
      if ((scanIndex < 0) || (nxIndex < 0) || (nyIndex < 0)) {
         ierr = TransferInt (gfld->fld, gfld->ngrdpts, *ibitmap, gfld->bmap,
                             1, &(dummyScan), 0, 0, *iclean, *xmissp,
                             iain, *nd2x3, ib);
      } else {
         ierr = TransferInt (gfld->fld, gfld->ngrdpts, *ibitmap, gfld->bmap,
                             f_ignoreScan, &(is3[scanIndex]), is3[nxIndex],
                             is3[nyIndex], *iclean, *xmissp, iain, *nd2x3,
                             ib);
      }
   } else {
      /* float data, use ain */
      if ((scanIndex < 0) || (nxIndex < 0) || (nyIndex < 0)) {
         ierr = TransferFloat (gfld->fld, gfld->ngrdpts, *ibitmap,
                               gfld->bmap, 1, &(dummyScan), 0, 0, *iclean,
                               *xmissp, ain, *nd2x3, ib);
      } else {
         ierr = TransferFloat (gfld->fld, gfld->ngrdpts, *ibitmap,
                               gfld->bmap, f_ignoreScan, &(is3[scanIndex]),
                               is3[nxIndex], is3[nyIndex], *iclean, *xmissp,
                               ain, *nd2x3, ib);
      }
   }
   if (ierr != 0) {
      switch (ierr) {
         case 1:       /* nd2x3 is too small */
            jer[0 + *ndjer] = 2;
            *kjer = 1;
            break;
         case 2:       /* nx * ny != ngrdpts */
            jer[5 + *ndjer] = 2;
            *kjer = 6;
            break;
         default:
            jer[8 + *ndjer] = 2;
            jer[8] = 9999; /* Unknown error message. */
            *kjer = 9;
      }
      g2_free (gfld);
      return;
   }
   g2_free (gfld);
}

/*****************************************************************************
 * validate() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Output all the return values from the API to a file.  This is primarily
 * for diagnostics purposes.
 *
 * ARGUMENTS
 * filename = File to write the data to. (Input)
 *      ain = The data if the original data was float data. (nd2x3) (Input)
 *     iain = The data if the original data was integer data. (nd2x3) (Input)
 *    nd2x3 = length of ain, iain, ib (Input)
 *     idat = MDL local use data (if any were unpacked from Sect 2). (Input)
 *    nidat = length of idat. (Input)
 *     rdat = floating point local use data (Input)
 *    nrdat = length of rdat. (Input)
 * is0, ns0 = Section 0 data, length of is0 (16) (Input)
 * is1, ns1 = Section 1 data, length of is1 (21) (Input)
 * is2, ns2 = Section 2 data, length of is2 (??) (Input)
 * is3, ns3 = Section 3 data, length of is3 (96 or 1600) (Input)
 * is4, ns4 = Section 4 data, length of is4 (60) (Input)
 * is5, ns5 = Section 5 data, length of is5 (49) (Input)
 * is6, ns6 = Section 6 data, length of is6 (6) (Input)
 * is7, ns7 = Section 7 data, length of ns7 (8) (Input)
 *       ib = Bitmap if user requested it, and it was packed (Input)
 *  ibitmap = 0 means ib is invalid, 1 means ib is valid. (Input)
 *   xmissp = Primary missing value. (Input)
 *   xmisss = Secondary missing value (Input)
 *   iendpk = flag if there is more grids in the message. (Input)
 * jer(ndjer,2) = error codes along with severity. (Input)
 *    ndjer = length of jer. (15) (Input)
 *     kjer = number of error messages stored in jer. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 * -1 = Problems opening the file.
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifdef notdef
static int validate (char *filename, float * ain, sInt4 * iain,
                     sInt4 * nd2x3, sInt4 * idat, sInt4 * nidat,
                     float * rdat, sInt4 * nrdat, sInt4 * is0, sInt4 * ns0,
                     sInt4 * is1, sInt4 * ns1, sInt4 * is2, sInt4 * ns2,
                     sInt4 * is3, sInt4 * ns3, sInt4 * is4, sInt4 * ns4,
                     sInt4 * is5, sInt4 * ns5, sInt4 * is6, sInt4 * ns6,
                     sInt4 * is7, sInt4 * ns7, sInt4 * ib, sInt4 * ibitmap,
                     float * xmissp, float * xmisss, sInt4 * iendpk,
                     sInt4 * jer, sInt4 * ndjer, sInt4 * kjer)
{
   FILE *fp;            /* Open file to write to. */
   int i;               /* Counter for printing to file. */

   if ((fp = fopen (filename, "wt")) == NULL) {
#ifdef DEBUG
      printf ("unable to open %s for write\n", filename);
#endif
      return -1;
   }
   for (i = 0; i < *ns0; i++) {
      fprintf (fp, "Sect 0 : %d of %d : %d\n", i, *ns0, is0[i]);
   }
   for (i = 0; i < *ns1; i++) {
      fprintf (fp, "Sect 1 : %d of %d : %d\n", i, *ns1, is1[i]);
   }
   for (i = 0; i < *ns2; i++) {
      fprintf (fp, "Sect 2 : %d of %d : %d\n", i, *ns2, is2[i]);
   }
   for (i = 0; i < idat[0]; i++) {
      fprintf (fp, "idat : %d of %d : %d\n", i, idat[0], idat[i]);
   }
   for (i = 0; i < rdat[0]; i++) {
      fprintf (fp, "rdat : %d of %f : %f\n", i, rdat[0], rdat[i]);
   }
   for (i = 0; i < *ns3; i++) {
      fprintf (fp, "Sect 3 : %d of %d : %d\n", i, *ns3, is3[i]);
   }
   for (i = 0; i < *ns4; i++) {
      fprintf (fp, "Sect 4 : %d of %d : %d\n", i, *ns4, is4[i]);
   }
   for (i = 0; i < *ns5; i++) {
      fprintf (fp, "Sect 5 : %d of %d : %d\n", i, *ns5, is5[i]);
   }
   for (i = 0; i < *ns6; i++) {
      fprintf (fp, "Sect 6 : %d of %d : %d\n", i, *ns6, is6[i]);
   }
   for (i = 0; i < *ns7; i++) {
      fprintf (fp, "Sect 7 : %d of %d : %d\n", i, *ns7, is7[i]);
   }
   fprintf (fp, "Xmissp = %f\n", *xmissp);
   fprintf (fp, "xmisss = %f\n", *xmisss);
   if ((is5[9] == 0) || (is5[9] == 1) || (is5[9] == 2) || (is5[9] == 3)) {
      if (is5[20] == 1) {
         for (i = 0; i < *nd2x3; i++) {
            fprintf (fp, "Int Data : %d of %d : %d\n", i, *nd2x3, iain[i]);
         }
      }
   } else {
      for (i = 0; i < *nd2x3; i++) {
         fprintf (fp, "Float Data : %d of %d : %f\n", i, *nd2x3, ain[i]);
      }
   }
   fprintf (fp, "ibitmap = %d\n", *ibitmap);
   if (*ibitmap) {
      for (i = 0; i < *nd2x3; i++) {
         fprintf (fp, "Bitmap Data : %d of %d : %d\n", i, *nd2x3, ib[i]);
      }
   }
   for (i = 0; i < *ndjer; i++) {
      fprintf (fp, "jer(i,1) %d jer(i,2) %d\n", jer[i], jer[i + *ndjer]);
   }
   fprintf (fp, "kjer = %d\n", *kjer);
   fprintf (fp, "iendpk = %d\n", *iendpk);
   fclose (fp);
   return 0;
}
#endif /* def notdef */

/*****************************************************************************
 * clear() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Clear all the return values from the API.  This is primarily for
 * diagnostics purposes.
 *
 * ARGUMENTS
 *      ain = The data if the original data was float data. (nd2x3) (Output)
 *     iain = The data if the original data was integer data. (nd2x3) (Output)
 *    nd2x3 = length of ain, iain, ib (Input)
 *     idat = MDL local use data (if any were unpacked from Sect 2). (Output)
 *    nidat = length of idat. (Input)
 *     rdat = floating point local use data (Output)
 *    nrdat = length of rdat. (Input)
 * is0, ns0 = Section 0 data, length of is0 (16) (Output) (Input)
 * is1, ns1 = Section 1 data, length of is1 (21) (Output) (Input)
 * is2, ns2 = Section 2 data, length of is2 (??) (Output) (Input)
 * is3, ns3 = Section 3 data, length of is3 (96 or 1600) (Output) (Input)
 * is4, ns4 = Section 4 data, length of is4 (60) (Output) (Input)
 * is5, ns5 = Section 5 data, length of is5 (49) (Output) (Input)
 * is6, ns6 = Section 6 data, length of is6 (6) (Output) (Input)
 * is7, ns7 = Section 7 data, length of ns7 (8) (Output) (Input)
 *       ib = Bitmap if user requested it, and it was packed (Output)
 *  ibitmap = 0 means ib is invalid, 1 means ib is valid. (Output)
 *   xmissp = Primary missing value. (Output)
 *   xmisss = Secondary missing value (Output)
 *   iendpk = flag if there is more grids in the message. (Output)
 * jer(ndjer,2) = error codes along with severity. (Output)
 *    ndjer = length of jer. (15) (Input)
 *     kjer = number of error messages stored in jer. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 * -1 = Problems opening the file.
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
/*
static void clear (float * ain, sInt4 * iain, sInt4 * nd2x3, sInt4 * idat,
                   sInt4 * nidat, float * rdat, sInt4 * nrdat, sInt4 * is0,
                   sInt4 * ns0, sInt4 * is1, sInt4 * ns1, sInt4 * is2,
                   sInt4 * ns2, sInt4 * is3, sInt4 * ns3, sInt4 * is4,
                   sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6,
                   sInt4 * ns6, sInt4 * is7, sInt4 * ns7, sInt4 * ib,
                   sInt4 * ibitmap, float * xmissp, float * xmisss,
                   sInt4 * iendpk, sInt4 * jer, sInt4 * ndjer, sInt4 * kjer)
{
   memset ((void *) ain, 0, *nd2x3 * sizeof (float));
   memset ((void *) iain, 0, *nd2x3 * sizeof (sInt4));
   memset ((void *) idat, 0, *nidat * sizeof (sInt4));
   memset ((void *) rdat, 0, *nrdat * sizeof (float));
   memset ((void *) is0, 0, *ns0 * sizeof (sInt4));
   memset ((void *) is1, 0, *ns1 * sizeof (sInt4));
   memset ((void *) is2, 0, *ns2 * sizeof (sInt4));
   memset ((void *) is3, 0, *ns3 * sizeof (sInt4));
   memset ((void *) is4, 0, *ns4 * sizeof (sInt4));
   if ((is5[9] == 2) || (is5[9] == 3)) {
      *xmissp = 0;
      *xmisss = 0;
   }
   memset ((void *) is5, 0, *ns5 * sizeof (sInt4));
   memset ((void *) is6, 0, *ns6 * sizeof (sInt4));
   memset ((void *) is7, 0, *ns7 * sizeof (sInt4));
   memset ((void *) ib, 0, *nd2x3 * sizeof (sInt4));
   *ibitmap = 0;
   *iendpk = 0;
   memset ((void *) jer, 0, 2 * *ndjer * sizeof (sInt4));
   *kjer = 0;
}
*/

/*****************************************************************************
 * BigByteCpy() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This is so we can copy upto 4 bytes from a big endian 4 byte int data
 * stream.
 *
 *   The reason this is needed is because the GRIB2 API required the GRIB2
 * message to be passed in as a big endian 4 byte int data stream instead of
 * something more reasonable (such as a stream of 1 byte char's)  By having
 * this routine we reduce the number of memswaps of the message on a little
 * endian system.
 *
 * ARGUMENTS
 *       dst = Where to copy the data to. (Output)
 *     ipack = The 4 byte int data stream. (Input)
 *       nd5 = length of ipack (Input)
 *  startInt = Which integer to start reading in ipack. (Input)
 * startByte = Which byte in the startInt to start reading from. (Input)
 *   numByte = How many bytes to read. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: NULL
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static void BigByteCpy (sInt4 * dst, sInt4 * ipack, sInt4 nd5,
                        unsigned int startInt, unsigned int startByte,
                        int numByte)
{
   static int Lshift[] = { 0, 8, 16, 24 }; /* Amounts to shift left by. */
   unsigned int intIndex; /* Where in ipack to read from. */
   unsigned int byteIndex; /* Where in intIndex to read from. */
   uInt4 curInt;        /* An unsigned version of the current int. */
   unsigned int curByte; /* The current byte we have read. */
   int i;               /* Loop counter over number of bytes to read. */

   myAssert (numByte <= 4);
   myAssert (startByte < 4);
   *dst = 0;
   intIndex = startInt;
   byteIndex = startByte;
   for (i = 0; i < numByte; i++) {
      curInt = (uInt4) ipack[intIndex];
      curByte = (curInt << Lshift[byteIndex]) >> 24;
      *dst = (*dst << 8) + curByte;
      byteIndex++;
      if (byteIndex == 4) {
         byteIndex = 0;
         intIndex++;
      }
   }
}

/*****************************************************************************
 * FindTemplateIDs() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This is so we can find out which templates are in this GRIB2 message.
 * Which allows us to determine if we can call the MDL routines, or if we
 * should call the NCEP routines instead.
 *
 * ARGUMENTS
 *      ipack = The 4 byte int data stream. (Input)
 *        nd5 = length of ipack (Input)
 *    subgNum = Which subgrid to look at. (Input)
 *    gdsTmpl = The gds template number for this subgrid. (Output)
 *    pdsTmpl = The pds template number for this subgrid. (Output)
 *    drsTmpl = The drs template number for this subgrid. (Output)
 * f_noBitmap = 0 has a bitmap, else doesn't have a bitmap. (Output)
 *  orderDiff = The order of the differencing in template 5.3 (1st, 2nd) (out)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = Ok.
 *  2 = invalid section number
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int FindTemplateIDs (sInt4 * ipack, sInt4 nd5, int subgNum,
                            sInt4 * gdsTmpl, sInt4 * pdsTmpl,
                            sInt4 * drsTmpl, sInt4 * numGrps,
                            uChar * f_noBitmap, sInt4 * orderDiff)
{
   unsigned int gNum = 0; /* Which sub group we are currently working with. */
   sInt4 offset;        /* Where in the bytes stream we currently are. */
   sInt4 sectLen;       /* The length of the current section. */
   sInt4 sectId;        /* The current section number. */
   sInt4 li_temp;       /* A temporary holder for the bitmap flag. */

   /* Jump over section 0. */
   offset = 16;
   while (gNum <= subgNum) {
      BigByteCpy (&sectLen, ipack, nd5, (offset / 4), (offset % 4), 4);
      /* Check if we just read section 8.  If so, then it is "7777" =
       * 926365495 regardless of endian'ness. */
      if (sectLen == 926365495L) {
#ifdef DEBUG
         printf ("Shouldn't see sect 8. Should stop after correct sect 5\n");
#endif
         return 2;
      }
      BigByteCpy (&sectId, ipack, nd5, ((offset + 4) / 4),
                  ((offset + 4) % 4), 1);
      switch (sectId) {
         case 1:
         case 2:
            break;
         case 3:
            BigByteCpy (gdsTmpl, ipack, nd5, ((offset + 12) / 4),
                        ((offset + 12) % 4), 2);
            break;
         case 4:
            BigByteCpy (pdsTmpl, ipack, nd5, ((offset + 7) / 4),
                        ((offset + 7) % 4), 2);
            break;
         case 5:
            BigByteCpy (drsTmpl, ipack, nd5, ((offset + 9) / 4),
                        ((offset + 9) % 4), 2);
            if ((*drsTmpl == 2) || (*drsTmpl == 3)) {
               BigByteCpy (numGrps, ipack, nd5, ((offset + 31) / 4),
                           ((offset + 31) % 4), 4);
            } else {
               *numGrps = 0;
            }
            if (*drsTmpl == 3) {
               BigByteCpy (&li_temp, ipack, nd5, ((offset + 44) / 4),
                           ((offset + 44) % 4), 1);
               *orderDiff = li_temp;
            } else {
               *orderDiff = 0;
            }
            break;
         case 6:
            BigByteCpy (&li_temp, ipack, nd5, ((offset + 5) / 4),
                        ((offset + 5) % 4), 1);
            if (li_temp == 255) {
               *f_noBitmap = 1;
            }
            gNum++;
            break;
         case 7:
            break;
         default:
#ifdef DEBUG
            printf ("Invalid section id %d.\n", sectId);
#endif
            return 2;
      }
      offset += sectLen;
   }
   return 0;
}

/*****************************************************************************
 * unpk_grib2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This procedure is the main API for decoding GRIB2 messages.  It is
 * intended to be a branching routine to call either the MDL GRIB2 library,
 * or the NCEP GRIB2 library, depending on which template it sees in the
 * message.
 *
 * ARGUMENTS
 *  kfildo = Unit number for output diagnostics (C ignores this). (Input)
 *     ain = contains the data if the original data was float data.
 *           (size = nd2x3) (Output)
 *    iain = contains the data if the original data was integer data.
 *           (size = nd2x3) (Output)
 *   nd2x3 = length of ain, iain, ib (Input)
 *           (at least the size of num grid points)
 *    idat = local use data if any that were unpacked from Section 2. (Output)
 *   nidat = length of idat. (Input)
 *    rdat = floating point local use data (Output)
 *   nrdat = length of rdat. (Input)
 *     is0 = Section 0 data (Output)
 *     ns0 = length of is0 (16 is fine) (Input)
 *     is1 = Section 1 data (Output)
 *     ns1 = length of is1 (21 is fine) (Input)
 *     is2 = Section 2 data (Output)
 *     ns2 = length of is2 () (Input)
 *     is3 = Section 3 data (Output)
 *     ns3 = length of is3 (96 or 1600) (Input)
 *     is4 = Section 4 data (Output)
 *     ns4 = length of is4 (60) (Input)
 *     is5 = Section 5 data (Output)
 *     ns5 = length of is5 (49 is fine) (Input)
 *     is6 = Section 6 data (Output)
 *     ns6 = length of is6 (6 is fine) (Input)
 *     is7 = Section 7 data (Output)
 *     ns7 = length of is7 (8 is fine) (Input)
 *      ib = Bitmap if user requested it, and it was packed (Output)
 * ibitmap = 0 means ib is invalid, 1 means ib is valid. (Output)
 *   ipack = The message to unpack (This is assumed to be Big endian) (Input)
 *     nd5 = The size of ipack (Input)
 *  xmissp = The floating point representation for the primary missing value.
 *           (Input/Output)
 *  xmisss = The floating point representation for the secondary missing
 *           value (Input/Output)
 *     new = 1 if this is the first grid to be unpacked, else 0. (Input)
 *  iclean = 1 means the user wants the unpacked data returned without
 *           missing values in it. 0 means embed the missing values. (Input)
 *  l3264b = Integer word length in bits (32 or 64) (Input)
 *  iendpk = A 1 means no more grids in this message, a 0 means more grids.
 *           (Output)
 * jer(ndjer,2) = error codes along with severity. (Output)
 *   ndjer = 1/2 length of jer. (>= 15) (Input)
 *    kjer = number of error messages stored in jer. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *  12/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * Inefficiencies: have to memswap ipack multiple times.
 * MDL handles is5[12], is5[23], and is5[27] in an "interesting" manner.
 * MDL attempts to always return grids in scan mode 0100????
 * ToDo: Check length of parameters better.
 *
 * According to MDL's unpk_grib2.f, it currently supports (so for others we
 * call the NCEP routines):
 *    TEMPLATE 3.0   EQUIDISTANT CYLINDRICAL LATITUDE/LONGITUDE
 *    TEMPLATE 3.10  MERCATOR
 *    TEMPLATE 3.20  POLAR STEREOGRAPHIC
 *    TEMPLATE 3.30  LAMBERT
 *    TEMPLATE 3.90  ORTHOGRAPHIC SPACE VIEW
 *    TEMPLATE 3.110 EQUATORIAL AZIMUTHAL EQUIDISTANT
 *    TEMPLATE 3.120 AZIMUTH-RANGE (RADAR)
 *
 *    TEMPLATE 4.0  ANALYSIS OR FORECAST AT A LEVEL AND POINT
 *    TEMPLATE 4.1  INDIVIDUAL ENSEMBLE
 *    TEMPLATE 4.2  DERIVED FORECAST BASED ON ENSEMBLES
 *    TEMPLATE 4.8  AVERAGE, ACCUMULATION, EXTREMES
 *    TEMPLATE 4.20 RADAR
 *    TEMPLATE 4.30 SATELLITE
 *
 *    TEMPLATE 5.0  SIMPLE PACKING
 *    TEMPLATE 5.2  COMPLEX PACKING
 *    TEMPLATE 5.3  COMPLEX PACKING AND SPATIAL DIFFERENCING
 *
 * Correction to "unpk_grib2.f" : It also supports:
 *    TEMPLATE 4.9  Probability forecast in a time interval
 *
 *****************************************************************************
 */
void unpk_grib2 (sInt4 * kfildo, float * ain, sInt4 * iain, sInt4 * nd2x3,
                 sInt4 * idat, sInt4 * nidat, float * rdat, sInt4 * nrdat,
                 sInt4 * is0, sInt4 * ns0, sInt4 * is1, sInt4 * ns1,
                 sInt4 * is2, sInt4 * ns2, sInt4 * is3, sInt4 * ns3,
                 sInt4 * is4, sInt4 * ns4, sInt4 * is5, sInt4 * ns5,
                 sInt4 * is6, sInt4 * ns6, sInt4 * is7, sInt4 * ns7,
                 sInt4 * ib, sInt4 * ibitmap, sInt4 * ipack, sInt4 * nd5,
                 float * xmissp, float * xmisss, sInt4 * inew,
                 sInt4 * iclean, sInt4 * l3264b, sInt4 * iendpk, sInt4 * jer,
                 sInt4 * ndjer, sInt4 * kjer)
{
   unsigned char *c_ipack; /* The compressed data as char instead of sInt4
                            * so it is easier to work with. */
   sInt4 gdsTmpl;
   sInt4 pdsTmpl;
   sInt4 drsTmpl;
   sInt4 numGrps;
      char f_useMDL = 0;   /* Instructed 3/8/2005 10:30 to not use MDL. */
   uChar f_noBitmap;    /* 0 if bitmap, else no bitmap. */
   sInt4 orderDiff;

   if (FindTemplateIDs (ipack, *nd5, 0, &gdsTmpl, &pdsTmpl, &drsTmpl,
                        &numGrps, &f_noBitmap, &orderDiff) != 0) {
      jer[0 + *ndjer] = 2;
      jer[0] = 3000;    /* Couldn't figure out which templates are used. */
      *kjer = 1;
   }
   if ((gdsTmpl != 0) && (gdsTmpl != 10) && (gdsTmpl != 20) &&
       (gdsTmpl != 30) && (gdsTmpl != 90) && (gdsTmpl != 110) &&
       (gdsTmpl != 120)) {
      f_useMDL = 0;
   }
   if ((pdsTmpl != 0) && (pdsTmpl != 1) && (pdsTmpl != 2) &&
       (pdsTmpl != 8) && (pdsTmpl != 9) && (pdsTmpl != 20) &&
       (pdsTmpl != 30)) {
      f_useMDL = 0;
   }
   if ((drsTmpl != 0) && (drsTmpl != 2) && (drsTmpl != 3)) {
      f_useMDL = 0;
   }
   /* MDL GRIB2 lib does not support drsTmpl 2 or 3 if there is a bitmap. */
   if ((!f_noBitmap) && ((drsTmpl == 2) || (drsTmpl == 3))) {
      f_useMDL = 0;
   }
   /* MDL GRIB2 lib does not support anything but second order differencing. */
   if ((drsTmpl == 3) && (orderDiff != 2) && (orderDiff != 0)) {
      f_useMDL = 0;
   }

#ifdef _FORTRAN
   if (f_useMDL) {
      jmin = (sInt4 *) malloc (numGrps * sizeof (sInt4));
      lbit = (sInt4 *) malloc (numGrps * sizeof (sInt4));
      nov = (sInt4 *) malloc (numGrps * sizeof (sInt4));
      iwork = (sInt4 *) malloc ((*nd2x3) * sizeof (sInt4));

      UNPK_G2MDL (kfildo, jmin, lbit, nov, iwork, ain, iain, nd2x3, idat,
                  nidat, rdat, nrdat, is0, ns0, is1, ns1, is2, ns2, is3, ns3,
                  is4, ns4, is5, ns5, is6, ns6, is7, ns7, ib, ibitmap, ipack,
                  nd5, xmissp, xmisss, inew, iclean, l3264b, iendpk, jer,
                  ndjer, kjer);
/*
      if (1==1) {
         int i;
         for (i=0; i < *nd2x3; i++) {
            if (ain[i] != 9999) {
               fprintf (stderr, "here grib2api.c: ain[%d] %f\n", i, ain[i]);
            }
         }
      }
*/
      free (jmin);
      free (lbit);
      free (nov);
      free (iwork);

      /* Check the behavior of the NCEP routines. */
/*
#ifdef DEBUG
      validate ("check2.txt", ain, iain, nd2x3, idat, nidat, rdat, nrdat,
                is0, ns0, is1, ns1, is2, ns2, is3, ns3, is4, ns4, is5, ns5,
                is6, ns6, is7, ns7, ib, ibitmap, xmissp, xmisss, iendpk,
                jer, ndjer, kjer);
      clear (ain, iain, nd2x3, idat, nidat, rdat, nrdat, is0, ns0, is1, ns1,
             is2, ns2, is3, ns3, is4, ns4, is5, ns5, is6, ns6, is7, ns7, ib,
             ibitmap, xmissp, xmisss, iendpk, jer, ndjer, kjer);
#ifdef LITTLE_ENDIAN
      memswp (ipack, sizeof (sInt4), *nd5);
#endif
      c_ipack = (unsigned char *) ipack;
      unpk_g2ncep (kfildo, ain, iain, nd2x3, idat, nidat, rdat, nrdat, is0,
                   ns0, is1, ns1, is2, ns2, is3, ns3, is4, ns4, is5, ns5,
                   is6, ns6, is7, ns7, ib, ibitmap, c_ipack, nd5, xmissp,
                   xmisss, inew, iclean, l3264b, iendpk, jer, ndjer, kjer);
#ifdef LITTLE_ENDIAN
      memswp (ipack, sizeof (sInt4), *nd5);
#endif
      validate ("check1.txt", ain, iain, nd2x3, idat, nidat, rdat, nrdat,
                is0, ns0, is1, ns1, is2, ns2, is3, ns3, is4, ns4, is5, ns5,
                is6, ns6, is7, ns7, ib, ibitmap, xmissp, xmisss, iendpk, jer,
                ndjer, kjer);
#endif
*/
   } else {
#endif
      /* Since NCEP's code works with byte level stuff, we need to un-do the
       * byte swap of the (int *) data, then cast it to an (unsigned char *) */
#ifdef LITTLE_ENDIAN
      /* Can't make this dependent on inew, since they could have a sequence
       * of get first message... do stuff, get second message, which
       * unfortunately means they would have to get the first message again,
       * causing 2 swaps, and breaking their second request for the first
       * message (as well as their second message). */
      memswp (ipack, sizeof (sInt4), *nd5);
#endif
      c_ipack = (unsigned char *) ipack;
      unpk_g2ncep (kfildo, ain, iain, nd2x3, idat, nidat, rdat, nrdat, is0,
                   ns0, is1, ns1, is2, ns2, is3, ns3, is4, ns4, is5, ns5,
                   is6, ns6, is7, ns7, ib, ibitmap, c_ipack, nd5, xmissp,
                   xmisss, inew, iclean, l3264b, iendpk, jer, ndjer, kjer);

#ifdef LITTLE_ENDIAN
      /* Swap back because we could be called again for the subgrid data. */
      memswp (ipack, sizeof (sInt4), *nd5);
#endif
#ifdef _FORTRAN
   }
#endif
}

/* Not sure I need this... It is intended to provide a way to call it from
 * FORTRAN, but I'm not sure it is needed. */
/* gcc has two __ if there is one _ in the procedure name. */
void unpk_grib2__ (sInt4 * kfildo, float * ain, sInt4 * iain,
                   sInt4 * nd2x3, sInt4 * idat, sInt4 * nidat, float * rdat,
                   sInt4 * nrdat, sInt4 * is0, sInt4 * ns0, sInt4 * is1,
                   sInt4 * ns1, sInt4 * is2, sInt4 * ns2, sInt4 * is3,
                   sInt4 * ns3, sInt4 * is4, sInt4 * ns4, sInt4 * is5,
                   sInt4 * ns5, sInt4 * is6, sInt4 * ns6, sInt4 * is7,
                   sInt4 * ns7, sInt4 * ib, sInt4 * ibitmap, sInt4 * ipack,
                   sInt4 * nd5, float * xmissp, float * xmisss,
                   sInt4 * inew, sInt4 * iclean, sInt4 * l3264b,
                   sInt4 * iendpk, sInt4 * jer, sInt4 * ndjer, sInt4 * kjer)
{
   unpk_grib2 (kfildo, ain, iain, nd2x3, idat, nidat, rdat, nrdat, is0, ns0,
               is1, ns1, is2, ns2, is3, ns3, is4, ns4, is5, ns5, is6, ns6,
               is7, ns7, ib, ibitmap, ipack, nd5, xmissp, xmisss, inew,
               iclean, l3264b, iendpk, jer, ndjer, kjer);
}

/*****************************************************************************
 * C_pkGrib2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This procedure is the main API for encoding GRIB2 messages.  It is
 * intended to call NCEP's GRIB2 library.
 *
 * ARGUMENTS
 *     ain = Contains the data to pack if the original data was float data.
 *           (size = nd2x3) (Input)
 *    iain = Contains the data to pack if the original data was integer data.
 *           (size = nd2x3) (Input)
 *      nx = The number of rows in the gridded product. (Input)
 *      ny = The number of columns in the gridded products. (Input)
 *    idat = local use data if it is integer (stored in section 2). (Input)
 *   nidat = length of idat. (Input)
 *    rdat = local use data if it is a float (Input)
 *   nrdat = length of rdat. (Input)
 *     is0 = Section 0 data (element 7 should be set by caller) (Input/Output)
 *     ns0 = length of is0 (16 is fine) (Input)
 *     is1 = Section 1 data (Input/Output)
 *     ns1 = length of is1 (21 is fine) (Input)
 *     is3 = Section 3 data (Input/Output)
 *     ns3 = length of is3 (96 or 1600) (Input)
 *     is4 = Section 4 data (Input/Output)
 *     ns4 = length of is4 (60) (Input)
 *     is5 = Section 5 data (Input/Output)
 *     ns5 = length of is5 (49 is fine) (Input)
 *     is6 = Section 6 data (Input/Output)
 *     ns6 = length of is6 (6 is fine) (Input)
 *     is7 = Section 7 data (Input/Output)
 *     ns7 = length of is7 (8 is fine) (Input)
 *      ib = Bitmap if user requested it, and it was packed (Input/Output)
 * ibitmap = 0 means ib is invalid, 1 means ib is valid. (Input/Output)
 *   ipack = The message to unpack (This is assumed to be Big endian) (Output)
 *     nd5 = The size of ipack (250 + NX*NY + (NX*NY)/8 + num local data) (In)
 *   missp = The integer representation for the primary missing value. (Input)
 *  xmissp = The float representation for the primary missing value. (Input)
 *   misss = The integer representation for the secondary missing value. (In)
 *  xmisss = The float representation for the secondary missing value (Input)
 *     new = 1 if this is the first grid to be unpacked, else 0. (Input)
 *   minpk = minimum size of groups in complex and second order differencing
 *           methods.  Recommended value 14 (Input)
 *  iclean = 1 means no primary missing values embedded in the data field,
 *           0 means there are primary missing values in the data (Input/Out)
 *  l3264b = Integer word length in bits (32 or 64) (Input)
 * jer(ndjer,2) = error codes along with severity. (Output)
 *   ndjer = 1/2 length of jer. (>= 15) (Input)
 *    kjer = number of error messages stored in jer. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * Inefficiencies: have to memswap ipack multiple times.
 * MDL handles is5[12], is5[23], and is5[27] in an "interesting" manner.
 * MDL attempts to always return grids in scan mode 0100????
 * ToDo: Check length of parameters better.
 *
 * According to MDL's pk_grib2.f, it currently supports (so for others we
 * call the NCEP routines):
 *    TEMPLATE 3.0   EQUIDISTANT CYLINDRICAL LATITUDE/LONGITUDE
 *    TEMPLATE 3.10  MERCATOR
 *    TEMPLATE 3.20  POLAR STEREOGRAPHIC
 *    TEMPLATE 3.30  LAMBERT
 *    TEMPLATE 3.90  ORTHOGRAPHIC SPACE VIEW
 *    TEMPLATE 3.110 EQUATORIAL AZIMUTHAL EQUIDISTANT
 *    TEMPLATE 3.120 AZIMUTH-RANGE (RADAR)
 *
 *    TEMPLATE 4.0  ANALYSIS OR FORECAST AT A LEVEL AND POINT
 *    TEMPLATE 4.1  INDIVIDUAL ENSEMBLE
 *    TEMPLATE 4.2  DERIVED FORECAST BASED ON ENSEMBLES
 *    TEMPLATE 4.8  AVERAGE, ACCUMULATION, EXTREMES
 *    TEMPLATE 4.20 RADAR
 *    TEMPLATE 4.30 SATELLITE
 *
 *    TEMPLATE 5.0  SIMPLE PACKING
 *    TEMPLATE 5.2  COMPLEX PACKING
 *    TEMPLATE 5.3  COMPLEX PACKING AND SPATIAL DIFFERENCING
 *
 * Correction to "pk_grib2.f" : It also supports:
 *    TEMPLATE 4.9  Probability forecast in a time interval
 *
 *****************************************************************************
 */
int C_pkGrib2 (unsigned char *cgrib, sInt4 *sec0, sInt4 *sec1,
               unsigned char *csec2, sInt4 lcsec2,
               sInt4 *igds, sInt4 *igdstmpl, sInt4 *ideflist,
               sInt4 idefnum, sInt4 ipdsnum, sInt4 *ipdstmpl,
               float *coordlist, sInt4 numcoord, sInt4 idrsnum,
               sInt4 *idrstmpl, float *fld, sInt4 ngrdpts,
               sInt4 ibmap, sInt4 *bmap)
{
   int ierr;      /* error value from grib2 library. */

   if ((ierr = g2_create (cgrib, sec0, sec1)) == -1) {
      /* Tried to use for version other than GRIB Ed 2 */
      return -1;
   }

   if ((ierr = g2_addlocal (cgrib, csec2, lcsec2)) < 0) {
      /* Some how got a bad section 2.  Should be impossible unless an
       * assert was broken. */
      return -2;
   }

   if ((ierr = g2_addgrid (cgrib, igds, igdstmpl, ideflist, idefnum)) < 0) {
      /* Some how got a bad section 3.  Only way would be should be with an
       * unsupported template number unless an assert was broken. */
      return -3;
   }

   if ((ierr = g2_addfield (cgrib, ipdsnum, ipdstmpl, coordlist, numcoord,
                            idrsnum, idrstmpl, fld, ngrdpts, ibmap,
                            bmap)) < 0) {
      return -4;
   }

   if ((ierr = g2_gribend (cgrib)) < 0) {
      return -5;
   }

   return ierr;
}

/*****************************************************************************
 * pk_grib2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   This procedure is the main API for encoding GRIB2 messages.  It is
 * intended to be a branching routine to call either the MDL GRIB2 library,
 * or the NCEP GRIB2 library, depending on which template it sees in the
 * message.
 *  Currently it calls both for debug purposes.
 *
 * ARGUMENTS
 *  kfildo = Unit number for output diagnostics (C ignores this). (Input)
 *     ain = Contains the data to pack if the original data was float data.
 *           (size = nd2x3) (Input)
 *    iain = Contains the data to pack if the original data was integer data.
 *           (size = nd2x3) (Input)
 *      nx = The number of rows in the gridded product. (Input)
 *      ny = The number of columns in the gridded products. (Input)
 *    idat = local use data if it is integer (stored in section 2). (Input)
 *   nidat = length of idat. (Input)
 *    rdat = local use data if it is a float (Input)
 *   nrdat = length of rdat. (Input)
 *     is0 = Section 0 data (element 7 should be set by caller) (Input/Output)
 *     ns0 = length of is0 (16 is fine) (Input)
 *     is1 = Section 1 data (Input/Output)
 *     ns1 = length of is1 (21 is fine) (Input)
 *     is3 = Section 3 data (Input/Output)
 *     ns3 = length of is3 (96 or 1600) (Input)
 *     is4 = Section 4 data (Input/Output)
 *     ns4 = length of is4 (60) (Input)
 *     is5 = Section 5 data (Input/Output)
 *     ns5 = length of is5 (49 is fine) (Input)
 *     is6 = Section 6 data (Input/Output)
 *     ns6 = length of is6 (6 is fine) (Input)
 *     is7 = Section 7 data (Input/Output)
 *     ns7 = length of is7 (8 is fine) (Input)
 *      ib = Bitmap if user requested it, and it was packed (Input/Output)
 * ibitmap = 0 means ib is invalid, 1 means ib is valid. (Input/Output)
 *   ipack = The message to unpack (This is assumed to be Big endian) (Output)
 *     nd5 = The size of ipack (250 + NX*NY + (NX*NY)/8 + num local data) (In)
 *   missp = The integer representation for the primary missing value. (Input)
 *  xmissp = The float representation for the primary missing value. (Input)
 *   misss = The integer representation for the secondary missing value. (In)
 *  xmisss = The float representation for the secondary missing value (Input)
 *     new = 1 if this is the first grid to be unpacked, else 0. (Input)
 *   minpk = minimum size of groups in complex and second order differencing
 *           methods.  Recommended value 14 (Input)
 *  iclean = 1 means no primary missing values embedded in the data field,
 *           0 means there are primary missing values in the data (Input/Out)
 *  l3264b = Integer word length in bits (32 or 64) (Input)
 * jer(ndjer,2) = error codes along with severity. (Output)
 *   ndjer = 1/2 length of jer. (>= 15) (Input)
 *    kjer = number of error messages stored in jer. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * Inefficiencies: have to memswap ipack multiple times.
 * MDL handles is5[12], is5[23], and is5[27] in an "interesting" manner.
 * MDL attempts to always return grids in scan mode 0100????
 * ToDo: Check length of parameters better.
 *
 * According to MDL's pk_grib2.f, it currently supports (so for others we
 * call the NCEP routines):
 *    TEMPLATE 3.0   EQUIDISTANT CYLINDRICAL LATITUDE/LONGITUDE
 *    TEMPLATE 3.10  MERCATOR
 *    TEMPLATE 3.20  POLAR STEREOGRAPHIC
 *    TEMPLATE 3.30  LAMBERT
 *    TEMPLATE 3.90  ORTHOGRAPHIC SPACE VIEW
 *    TEMPLATE 3.110 EQUATORIAL AZIMUTHAL EQUIDISTANT
 *    TEMPLATE 3.120 AZIMUTH-RANGE (RADAR)
 *
 *    TEMPLATE 4.0  ANALYSIS OR FORECAST AT A LEVEL AND POINT
 *    TEMPLATE 4.1  INDIVIDUAL ENSEMBLE
 *    TEMPLATE 4.2  DERIVED FORECAST BASED ON ENSEMBLES
 *    TEMPLATE 4.8  AVERAGE, ACCUMULATION, EXTREMES
 *    TEMPLATE 4.20 RADAR
 *    TEMPLATE 4.30 SATELLITE
 *
 *    TEMPLATE 5.0  SIMPLE PACKING
 *    TEMPLATE 5.2  COMPLEX PACKING
 *    TEMPLATE 5.3  COMPLEX PACKING AND SPATIAL DIFFERENCING
 *
 * Correction to "pk_grib2.f" : It also supports:
 *    TEMPLATE 4.9  Probability forecast in a time interval
 *
 *****************************************************************************
 */
void pk_grib2 (sInt4 * kfildo, float * ain, sInt4 * iain, sInt4 * nx,
               sInt4 * ny, sInt4 * idat, sInt4 * nidat, float * rdat,
               sInt4 * nrdat, sInt4 * is0, sInt4 * ns0, sInt4 * is1,
               sInt4 * ns1, sInt4 * is3, sInt4 * ns3, sInt4 * is4,
               sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6,
               sInt4 * ns6, sInt4 * is7, sInt4 * ns7, sInt4 * ib,
               sInt4 * ibitmap, sInt4 * ipack, sInt4 * nd5, sInt4 * missp,
               float * xmissp, sInt4 * misss, float * xmisss, sInt4 * inew,
               sInt4 * minpk, sInt4 * iclean, sInt4 * l3264b, sInt4 * jer,
               sInt4 * ndjer, sInt4 * kjer)
{
#ifndef _FORTRAN
   
   printf ("Can not pack things unless using FORTRAN!\n");
   return;

#else

   sInt4 gdsTmpl;
   sInt4 pdsTmpl;
   sInt4 drsTmpl;
   sInt4 *jmax;
   sInt4 *jmin;
   sInt4 *lbit;
   sInt4 *nov;
   sInt4 *misslx;
   sInt4 *newbox;
   sInt4 *newboxp;
   sInt4 *ia;
/*   float *a;*/
   char f_useMDL = 1;
   int i;

   myAssert (*ndjer >= 8);
   /* Init the error handling array. */
   memset ((void *) jer, 0, 2 * *ndjer * sizeof (sInt4));
   for (i = 0; i < 8; i++) {
      jer[i] = i * 100;
   }
   *kjer = 8;

   gdsTmpl = is3[13 - 1];
   pdsTmpl = is4[8 - 1];
   drsTmpl = is5[10 - 1];
   if ((gdsTmpl != 0) && (gdsTmpl != 10) && (gdsTmpl != 20) &&
       (gdsTmpl != 30) && (gdsTmpl != 90) && (gdsTmpl != 110) &&
       (gdsTmpl != 120)) {
      f_useMDL = 0;
   }
   if ((pdsTmpl != 0) && (pdsTmpl != 1) && (pdsTmpl != 2) &&
       (pdsTmpl != 8) && (pdsTmpl != 9) && (pdsTmpl != 20) &&
       (pdsTmpl != 30)) {
      f_useMDL = 0;
   }
   if ((drsTmpl != 0) && (drsTmpl != 2) && (drsTmpl != 3)) {
      f_useMDL = 0;
   }
/*
   printf ("Forcing it to not use MDL encoder. \n");
   f_useMDL = 0;
*/
   if (f_useMDL) {
      /* pk_cmplx.f ==> jmax(M), jmin(M), nov(M), lbit(M) */
      jmax = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      jmin = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      nov = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      lbit = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      /* pk_grib2.f ==> a(NX,NY), ia(NX,NY) */
/*      a = (float *) malloc ((*nx) * (*ny) * sizeof (float));*/
      ia = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      /* pack_gp.f ==> misslx(NDG = NXY = nx*ny) */
      misslx = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      /* Able to use jmax(nxy) for iwork(nxy) in int_map.f, and pk_missp.f */
      /* Able to use jmax(nxy) for work(nxy) in flt_map.f */
      /* reduce.f ==> newbox(ndg=nxy), newboxp(ndg=nxy) */
      newbox = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));
      newboxp = (sInt4 *) malloc ((*nx) * (*ny) * sizeof (sInt4));

      /* other known automatic arrays... itemp(nidat) pk_sect2.f */
      /* other known automatic arrays... rtemp(nrdat) pk_sect2.f */

      /* a and ia are equivalenced here. */
#ifdef _FORTRAN
      PK_G2MDL (kfildo, jmax, jmin, lbit, nov, misslx, (float *) ia, ia,
                newbox, newboxp, ain, iain, nx, ny, idat, nidat, rdat, nrdat,
                is0, ns0, is1, ns1, is3, ns3, is4, ns4, is5, ns5, is6, ns6,
                is7, ns7, ib, ibitmap, ipack, nd5, missp, xmissp, misss,
                xmisss, inew, minpk, iclean, l3264b, jer, ndjer, kjer);
#endif /* _FORTRAN */

      free (jmax);
      free (jmin);
      free (nov);
      free (lbit);
      free (misslx);
/*      free (a);*/
      free (ia);
      free (newbox);
      free (newboxp);
   } else {


#ifdef PKNCEP
#ifdef LITTLE_ENDIAN
      /* Can't make this dependent on inew, since they could have a sequence
       * of get first message... do stuff, get second message, which
       * unfortunately means they would have to get the first message again,
       * causing 2 swaps, and breaking their second request for the first
       * message (as well as their second message). */
/*
      memswp (ipack, sizeof (sInt4), *nd5);
*/
#endif
      c_ipack = (unsigned char *) ipack;
      pk_g2ncep (kfildo, ain, iain, nx, ny, idat, nidat, rdat, nrdat, is0,
                 ns0, is1, ns1, is3, ns3, is4, ns4, is5, ns5, is6, ns6, is7,
                 ns7, ib, ibitmap, c_ipack, nd5, missp, xmissp, misss, xmisss,
                 inew, minpk, iclean, l3264b, jer, ndjer, kjer);

#ifdef LITTLE_ENDIAN
      /* Swap back because we could be called again for the subgrid data. */
      memswp (ipack, sizeof (sInt4), *nd5);
#endif
#else

      printf ("Unable to use MDL Pack library?\n");
      printf ("gdsTmpl : %d , pdsTmpl %d : drsTmpl %d\n", gdsTmpl,
              pdsTmpl, drsTmpl);
      jer[0 + *ndjer] = 31415926;
      *kjer = 1;
#endif
   }

#endif /* _FORTRAN */
}
