#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "grib2.h"

g2int pngunpack(unsigned char *cpack,g2int len,g2int *idrstmpl,g2int ndpts,
                g2float *fld)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    pngunpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2003-08-27
//
// ABSTRACT: This subroutine unpacks a data field that was packed into a
//   PNG image format
//   using info from the GRIB2 Data Representation Template 5.41 or 5.40010.
//
// PROGRAM HISTORY LOG:
// 2003-08-27  Gilbert
//
// USAGE:    pngunpack(unsigned char *cpack,g2int len,g2int *idrstmpl,g2int ndpts,
//                     g2float *fld)
//   INPUT ARGUMENT LIST:
//     cpack    - The packed data field (character*1 array)
//     len      - length of packed field cpack().
//     idrstmpl - Pointer to array of values for Data Representation
//                Template 5.41 or 5.40010
//     ndpts    - The number of data values to unpack
//
//   OUTPUT ARGUMENT LIST:
//     fld[]    - Contains the unpacked data values
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{

      g2int  *ifld;
      g2int  j,nbits,iret = 0,width,height;
      g2float  refD, refV,bscale,dscale, bdscale;
      unsigned char *ctemp;

      rdieee(idrstmpl+0,&refV,1);
      bscale = (g2float)int_power(2.0,idrstmpl[1]);
      dscale = (g2float)int_power(10.0,-idrstmpl[2]);
      bdscale = bscale * dscale;
      refD = refV * dscale;

      nbits = idrstmpl[3];
//
//  if nbits equals 0, we have a constant field where the reference value
//  is the data value at each gridpoint
//
      if (nbits != 0) {
         int nbytes = nbits/8;
         if( ndpts != 0 && nbytes > INT_MAX / ndpts )
         {
             return 1;
         }
         ifld=(g2int *)calloc(ndpts,sizeof(g2int));
         // Was checked just before
         // coverity[integer_overflow,overflow_sink]
         ctemp=(unsigned char *)calloc(ndpts*nbytes,1);
         if ( ifld == NULL || ctemp == NULL) {
            fprintf(stderr, "Could not allocate space in jpcunpack.\n"
                    "Data field NOT unpacked.\n");
            free(ifld);
            free(ctemp);
            return(1);
         }
         iret=(g2int)dec_png(cpack,len,&width,&height,ctemp, ndpts, nbits);
         gbits(ctemp,ndpts*nbytes,ifld,0,nbits,0,ndpts);
         for (j=0;j<ndpts;j++) {
            fld[j] = refD + bdscale*(g2float)(ifld[j]);
         }
         free(ctemp);
         free(ifld);
      }
      else {
         for (j=0;j<ndpts;j++) fld[j]=refD;
      }

      return(iret);
}
