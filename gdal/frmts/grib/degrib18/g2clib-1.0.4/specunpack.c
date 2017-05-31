#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "grib2.h"


g2int specunpack(unsigned char *cpack,g2int *idrstmpl,g2int ndpts,g2int JJ,
               g2int KK, g2int MM, g2float *fld)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    specunpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2000-06-21
//
// ABSTRACT: This subroutine unpacks a spectral data field that was packed
//   using the complex packing algorithm for spherical harmonic data as
//   defined in the GRIB2 documentation,
//   using info from the GRIB2 Data Representation Template 5.51.
//
// PROGRAM HISTORY LOG:
// 2000-06-21  Gilbert
//
// USAGE:    int specunpack(unsigned char *cpack,g2int *idrstmpl,
//                          g2int ndpts,g2int JJ,g2int KK,g2int MM,g2float *fld)
//   INPUT ARGUMENT LIST:
//     cpack    - pointer to the packed data field.
//     idrstmpl - pointer to the array of values for Data Representation
//                Template 5.51
//     ndpts    - The number of data values to unpack (real and imaginary parts)
//     JJ       - J - pentagonal resolution parameter
//     KK       - K - pentagonal resolution parameter
//     MM       - M - pentagonal resolution parameter
//
//   OUTPUT ARGUMENT LIST:
//     fld()    - Contains the unpacked data values.   fld must be allocated
//                with at least ndpts*sizeof(g2float) bytes before
//                calling this routine.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$
{

      g2int  *ifld,j,iofst,nbits;
      g2float  ref,bscale,dscale,*unpk;
      g2float  *pscale,tscale;
      g2int   Js,Ks,Ms,Ts,Ns,Nm,n,m;
      g2int   inc,incu,incp;

      rdieee(idrstmpl+0,&ref,1);
      bscale = (float)int_power(2.0,idrstmpl[1]);
      dscale = (float)int_power(10.0,-idrstmpl[2]);
      nbits = idrstmpl[3];
      Js=idrstmpl[5];
      Ks=idrstmpl[6];
      Ms=idrstmpl[7];
      Ts=idrstmpl[8];

      if (idrstmpl[9] == 1) {           // unpacked floats are 32-bit IEEE

         unpk=(g2float *)malloc(ndpts*sizeof(g2float));
         ifld=(g2int *)malloc(ndpts*sizeof(g2int));

         gbits(cpack,G2_UNKNOWN_SIZE,ifld,0,32,0,Ts);
         iofst=32*Ts;
         rdieee(ifld,unpk,Ts);          // read IEEE unpacked floats
         gbits(cpack,G2_UNKNOWN_SIZE,ifld,iofst,nbits,0,ndpts-Ts);  // unpack scaled data
//
//   Calculate Laplacian scaling factors for each possible wave number.
//
         pscale=(g2float *)malloc((JJ+MM+1)*sizeof(g2float));
         tscale=(g2float)(idrstmpl[4]*1E-6);
         for (n=Js;n<=JJ+MM;n++)
              pscale[n]=(float)pow((g2float)(n*(n+1)),-tscale);
//
//   Assemble spectral coeffs back to original order.
//
         inc=0;
         incu=0;
         incp=0;
         for (m=0;m<=MM;m++) {
            Nm=JJ;      // triangular or trapezoidal
            if ( KK == JJ+MM ) Nm=JJ+m;          // rhombodial
            Ns=Js;      // triangular or trapezoidal
            if ( Ks == Js+Ms ) Ns=Js+m;          // rhombodial
            for (n=m;n<=Nm;n++) {
               if (n<=Ns && m<=Ms) {    // grab unpacked value
                  fld[inc++]=unpk[incu++];         // real part
                  fld[inc++]=unpk[incu++];     // imaginary part
               }
               else {                       // Calc coeff from packed value
                  fld[inc++]=(((g2float)ifld[incp++]*bscale)+ref)*
                            dscale*pscale[n];          // real part
                  fld[inc++]=(((g2float)ifld[incp++]*bscale)+ref)*
                            dscale*pscale[n];          // imaginary part
               }
            }
         }

         free(pscale);
         free(unpk);
         free(ifld);

      }
      else {
         printf("specunpack: Cannot handle 64 or 128-bit floats.\n");
         for (j=0;j<ndpts;j++) fld[j]=0.0;
         return -3;
      }

      return 0;
}
