#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

static float DoubleToFloatClamp(double val) {
   if (val >= FLT_MAX) return FLT_MAX;
   if (val <= -FLT_MAX) return -FLT_MAX;
   return (float)val;
}

g2int jpcunpack(unsigned char *cpack,g2int len,g2int *idrstmpl,g2int ndpts,
                g2float **fld)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    jpcunpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2003-08-27
//
// ABSTRACT: This subroutine unpacks a data field that was packed into a
//   JPEG2000 code stream
//   using info from the GRIB2 Data Representation Template 5.40 or 5.40000.
//
// PROGRAM HISTORY LOG:
// 2003-08-27  Gilbert
//
// USAGE:    jpcunpack(unsigned char *cpack,g2int len,g2int *idrstmpl,g2int ndpts,
//                     g2float *fld)
//   INPUT ARGUMENT LIST:
//     cpack    - The packed data field (character*1 array)
//     len      - length of packed field cpack().
//     idrstmpl - Pointer to array of values for Data Representation
//                Template 5.40 or 5.40000
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
      g2int  j,nbits, iret;
      g2float  ref,bscale,dscale;

      rdieee(idrstmpl+0,&ref,1);
      bscale = DoubleToFloatClamp(int_power(2.0,idrstmpl[1]));
      dscale = DoubleToFloatClamp(int_power(10.0,-idrstmpl[2]));
      nbits = idrstmpl[3];
//
//  if nbits equals 0, we have a constant field where the reference value
//  is the data value at each gridpoint
//
      *fld = 0;
      if (nbits != 0) {

         ifld = NULL;
         iret= (g2int) dec_jpeg2000(cpack,len,&ifld,ndpts);
         if( iret != 0 )
         {
             free(ifld);
             return -1;
         }
         *fld =(g2float *)calloc(ndpts,sizeof(g2float));
         if( *fld == 0 )
         {
             free(ifld);
             return -1;
         }
         for (j=0;j<ndpts;j++) {
           (*fld)[j]=(((g2float)ifld[j]*bscale)+ref)*dscale;
         }
         free(ifld);
      }
      else {
         // Limit to 2 GB
         if( ndpts > 500 * 1024 * 1024 )
         {
             fprintf(stderr, "jpcunpack: ndpts = %d > 500 * 1024 * 1024", ndpts );
             return -1;
         }
         *fld =(g2float *)calloc(ndpts,sizeof(g2float));
         if( *fld == 0 )
         {
             return -1;
         }
         for (j=0;j<ndpts;j++) (*fld)[j]=ref * dscale;
      }

      return(0);
}
