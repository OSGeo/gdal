/*****************************************************************************
 * scan.c
 *
 * DESCRIPTION
 *    This file contains the code that is used to assist with handling the
 * possible scan values of the grid.
 *
 * HISTORY
 *   10/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#include "scan.h"

/*****************************************************************************
 * ScanIndex2XY() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To convert from the index of the GRIB2 message which is defined by the
 * scan parameter, to one that seemed reasonable.  The choice for internal
 * array orientation boiled down to either (scan = 0000) (start from upper
 * left and across similar to a CRT screen) or (scan = 0100) (start at lower
 * left and go up ).
 *   It was decided that (scan 0100) was what people expected.  The only catch
 * is that Spatial Analyst requires (scan = 0000), so when writing to that
 * format we have to switch.
 *   For more info on scan flags: see Grib2 "Flag" Table 3.4
 *
 * ARGUMENTS
 *    row = The index in the scanned in data. (Input)
 *   X, Y = The x,y position in a scan == 0100 world. (Output)
 *   scan = The orientation of the GRIB2 grid. (Input)
 * Nx, Ny = The Dimensions of the grid (Input).
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *   Returns x, y, in bounds of [1..Nx], [1..Ny]
 *      Assuming row is in [0..Nx*Ny)
 *
 * HISTORY
 *   10/2002 Arthur Taylor (MDL/RSIS): Created.
 *    7/2003 AAT: Switched to x, y [1..Nx] because that is what the map
 *           routines give.
 *
 * NOTES
 * scan based on Grib2 "Flag" Table 3.4
 *  scan & GRIB2BIT_1 => decrease x
 *  scan & GRIB2BIT_2 => increase y
 *  scan & GRIB2BIT_3 => adjacent points in y direction consecutive.
 *  scan & GRIB2BIT_4 => adjacent rows scan in opposite directions.
 *****************************************************************************
 */
void ScanIndex2XY (sInt4 row, sInt4 * X, sInt4 * Y, uChar scan, sInt4 Nx,
                   sInt4 Ny)
{
   sInt4 x;             /* local copy of x */
   sInt4 y;             /* local copy of y */

   if (scan & GRIB2BIT_3) {
      x = row / Ny;
      if ((scan & GRIB2BIT_4) && ((x % 2) == 1)) {
         y = (Ny - 1) - (row % Ny);
      } else {
         y = row % Ny;
      }
   } else {
      y = row / Nx;
      if ((scan & GRIB2BIT_4) && ((y % 2) == 1)) {
         x = (Nx - 1) - (row % Nx);
      } else {
         x = row % Nx;
      }
   }
   if (scan & GRIB2BIT_1) {
      x = (Nx - 1 - x);
   }
   if (!(scan & GRIB2BIT_2)) {
      y = (Ny - 1 - y);
   }
   /* Changed following two lines (with the + 1) on 7/22/2003 */
   *X = x + 1;
   *Y = y + 1;
}

/*****************************************************************************
 * XY2ScanIndex() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To convert from an x,y coordinate system that matches scan = 0100 to the
 * scan index of the GRIB2 message as defined by the scan parameter.
 *   This tends to be less important than ScanIndex2XY, but is provided for
 * testing purposes, and in case it is useful.
 *
 * ARGUMENTS
 *    Row = The index in the scanned in data. (Output)
 *   x, y = The x,y position in a (scan = 0100) world. (Input)
 *   scan = The orientation of the GRIB2 grid. (Input)
 * Nx, Ny = The Dimensions of the grid (Input).
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *   Returns row in [0..Nx*Ny)
 *      Assuming x, y, is in bounds of [1..Nx], [1..Ny]
 *
 * HISTORY
 *   10/2002 Arthur Taylor (MDL/RSIS): Created.
 *    7/2003 AAT: Switched to x, y [1..Nx] because that is what the map
 *           routines give.
 *
 * NOTES
 * scan based on Grib2 "Flag" Table 3.4
 *  scan & GRIB2BIT_1 => decrease x
 *  scan & GRIB2BIT_2 => increase y
 *  scan & GRIB2BIT_3 => adjacent points in y direction consecutive.
 *  scan & GRIB2BIT_4 => adjacent rows scan in opposite directions.
 *****************************************************************************
 */
void XY2ScanIndex (sInt4 * Row, sInt4 x, sInt4 y, uChar scan, sInt4 Nx,
                   sInt4 Ny)
{
   sInt4 row;           /* local copy of row */

   /* Added following two lines on 7/22/2003 */
   x = x - 1;
   y = y - 1;
   if (scan & GRIB2BIT_1) {
      x = (Nx - 1 - x);
   }
   if (!(scan & GRIB2BIT_2)) {
      y = (Ny - 1 - y);
   }
   if (scan & GRIB2BIT_3) {
      if ((scan & GRIB2BIT_4) && ((x % 2) == 1)) {
         row = Ny - 1 - y + x * Ny;
      } else {
         row = y + x * Ny;
      }
   } else {
      if ((scan & GRIB2BIT_4) && ((y % 2) == 1)) {
         row = Nx - 1 - x + y * Nx;
      } else {
         row = x + y * Nx;
      }
   }
   *Row = row;
}

/*****************************************************************************
 * main() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To test the ScanIndex2XY, and XY2ScanIndex routines, to make sure that
 * they are inverses of each other, for all possible scan values.  Also to
 * see what a sample array looks like in the various scans, and to make sure
 * that we are generating (scan = 0100) data.
 *
 * ARGUMENTS
 * argc = The number of arguments on the command line. (Input)
 * argv = The arguments on the command line. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   10/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifdef TEST_SCAN
#include <stdio.h>
int main (int argc, char **argv)
{
   int data[3][4];
   int ray1[6];
   int ray2[6];
   sInt4 Nx = 2, Ny = 3;
   sInt4 NxNy = 6;
   sInt4 row, x, y;
   sInt4 x1, y1;
   int i;
   int scan;

   /* Set up sample data. */
   for (x = 1; x <= Nx; x++) {
      for (y = 1; y <= Ny; y++) {
         data[x][y] = 1 + x + (y * 2);
      }
   }
   for (i = 0; i < 16; i++) {
      scan = i << 4;
      /* Print scan info. */
      printf ("Checking xy2row -> row2xy for scan %d ", i);
      if (scan & GRIB2BIT_1)
         printf ("-1");
      else
         printf ("-0");
      if (scan & GRIB2BIT_2)
         printf ("-1");
      else
         printf ("-0");
      if (scan & GRIB2BIT_3)
         printf ("-1");
      else
         printf ("-0");
      if (scan & GRIB2BIT_4)
         printf ("-1");
      else
         printf ("-0");
      printf ("\n");

      /* Test invertiblity of functions. */
      for (x = 1; x <= Nx; x++) {
         for (y = 1; y <= Ny; y++) {
            XY2ScanIndex (&row, x, y, scan, Nx, Ny);
            ScanIndex2XY (row, &x1, &y1, scan, Nx, Ny);
            if ((x1 != x) || (y1 != y)) {
               printf ("   %ld %ld .. %ld .. %ld %ld \n", x, y, row, x1, y1);
            }
         }
      }

      /* Set up sample scan data. */
      for (x = 1; x <= Nx; x++) {
         for (y = 1; y <= Ny; y++) {
            XY2ScanIndex (&row, x, y, scan, Nx, Ny);
            ray1[row] = data[x][y];
         }
      }

      /* Convert from ray1[] to ray2[] where ray2[] is scan value 0100. */
      for (x = 0; x < NxNy; x++) {
         printf ("%d ", ray1[x]);
         ScanIndex2XY (x, &x1, &y1, scan, Nx, Ny);
         /* 
          * To get scan 0000 do the following:
          * row = x1 + ((Ny-1) - y1) * Nx;
          */
         row = (x1 - 1) + (y1 - 1) * Nx;
         ray2[row] = ray1[x];
      }
      printf ("\n");
      for (x = 0; x < NxNy; x++) {
         printf ("%d ", ray2[x]);
      }
      printf ("\n");
   }
   return 0;
}
#endif
