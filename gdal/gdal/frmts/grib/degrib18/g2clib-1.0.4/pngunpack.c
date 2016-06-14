#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

int dec_png(unsigned char *,g2int *,g2int *,char *);

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
      g2int  j,nbits,iret,width,height;
      g2float  ref,bscale,dscale;
      unsigned char *ctemp;

      rdieee(idrstmpl+0,&ref,1);
      bscale = int_power(2.0,idrstmpl[1]);
      dscale = int_power(10.0,-idrstmpl[2]);
      nbits = idrstmpl[3];
//
//  if nbits equals 0, we have a constant field where the reference value
//  is the data value at each gridpoint
//
      if (nbits != 0) {

         ifld=(g2int *)calloc(ndpts,sizeof(g2int));
         ctemp=(unsigned char *)calloc(ndpts*4,1);
         if ( ifld == 0 || ctemp == 0) {
            fprintf(stderr, "Could not allocate space in jpcunpack.\n"
                    "Data field NOT unpacked.\n");
            return(1);
         }
         iret=(g2int)dec_png(cpack,&width,&height,ctemp);
         gbits(ctemp,ifld,0,nbits,0,ndpts);
         for (j=0;j<ndpts;j++) {
           fld[j]=(((g2float)ifld[j]*bscale)+ref)*dscale;
         }
         free(ctemp);
         free(ifld);
      }
      else {
         for (j=0;j<ndpts;j++) fld[j]=ref;
      }

      return(0);
}
