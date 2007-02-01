#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"


g2int simunpack(unsigned char *cpack,g2int *idrstmpl,g2int ndpts,g2float *fld)
////$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    simunpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2002-10-29
//
// ABSTRACT: This subroutine unpacks a data field that was packed using a 
//   simple packing algorithm as defined in the GRIB2 documention,
//   using info from the GRIB2 Data Representation Template 5.0.
//
// PROGRAM HISTORY LOG:
// 2002-10-29  Gilbert
//
// USAGE:    int simunpack(unsigned char *cpack,g2int *idrstmpl,g2int ndpts,
//                         g2float *fld)
//   INPUT ARGUMENT LIST:
//     cpack    - pointer to the packed data field.
//     idrstmpl - pointer to the array of values for Data Representation
//                Template 5.0
//     ndpts    - The number of data values to unpack
//
//   OUTPUT ARGUMENT LIST:
//     fld      - Contains the unpacked data values.  fld must be allocated
//                with at least ndpts*sizeof(g2float) bytes before
//                calling this routine.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$//
{

      g2int  *ifld;
      g2int  j,nbits,itype;
      g2float ref,bscale,dscale;

      
      rdieee(idrstmpl+0,&ref,1);
      bscale = int_power(2.0,idrstmpl[1]);
      dscale = int_power(10.0,-idrstmpl[2]);
      nbits = idrstmpl[3];
      itype = idrstmpl[4];

      ifld=(g2int *)calloc(ndpts,sizeof(g2int));
      if ( ifld == 0 ) {
         fprintf(stderr,"Could not allocate space in simunpack.\n  Data field NOT upacked.\n");
         return(1);
      }
      
//
//  if nbits equals 0, we have a constant field where the reference value
//  is the data value at each gridpoint
//
      if (nbits != 0) {
         gbits(cpack,ifld,0,nbits,0,ndpts);
         for (j=0;j<ndpts;j++) {
           fld[j]=(((g2float)ifld[j]*bscale)+ref)*dscale;
         }
      }
      else {
         for (j=0;j<ndpts;j++) {
           fld[j]=ref;
         }
      }

      free(ifld);
      return(0);
}
