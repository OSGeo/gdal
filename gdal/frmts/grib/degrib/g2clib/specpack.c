#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "grib2.h"


void specpack(g2float *fld,g2int ndpts,g2int JJ,g2int KK,g2int MM,
              g2int *idrstmpl,unsigned char *cpack,g2int *lcpack)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    specpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2002-12-19
//
// ABSTRACT: This subroutine packs a spectral data field using the complex
//   packing algorithm for spherical harmonic data as
//   defined in the GRIB2 Data Representation Template 5.51.
//
// PROGRAM HISTORY LOG:
// 2002-12-19  Gilbert
//
// USAGE:    void specpack(g2float *fld,g2int ndpts,g2int JJ,g2int KK,g2int MM,
//                        g2int *idrstmpl,insigned char *cpack,g2int *lcpack)
//   INPUT ARGUMENT LIST:
//     fld[]    - Contains the packed data values
//     ndpts    - The number of data values to pack
//     JJ       - J - pentagonal resolution parameter
//     KK       - K - pentagonal resolution parameter
//     MM       - M - pentagonal resolution parameter
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.51
//
//   OUTPUT ARGUMENT LIST:
//     cpack    - The packed data field (character*1 array)
//     lcpack   - length of packed field cpack().
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{

      g2int    *ifld,tmplsim[5];
      g2float  /* bscale, dscale, */ *unpk,*tfld;
      g2float  *pscale,tscale;
      g2int    Js,Ks,Ms,Ts,Ns,inc,incu,incp,n,Nm,m,ipos;

      /* bscale = int_power(2.0,-idrstmpl[1]); */
      /* dscale = int_power(10.0,idrstmpl[2]); */
      Js=idrstmpl[5];
      Ks=idrstmpl[6];
      Ms=idrstmpl[7];
      Ts=idrstmpl[8];

//
//   Calculate Laplacian scaling factors for each possible wave number.
//
      pscale=(g2float *)malloc((JJ+MM)*sizeof(g2float));
      tscale=(g2float)(idrstmpl[4]*1E-6);
      for (n=Js;n<=JJ+MM;n++)
           pscale[n]=(float)pow((g2float)(n*(n+1)),tscale);
//
//   Separate spectral coeffs into two lists; one to contain unpacked
//   values within the sub-spectrum Js, Ks, Ms, and the other with values
//   outside of the sub-spectrum to be packed.
//
      tfld=(g2float *)malloc(ndpts*sizeof(g2float));
      unpk=(g2float *)malloc(ndpts*sizeof(g2float));
      ifld=(g2int *)malloc(ndpts*sizeof(g2int));
      inc=0;
      incu=0;
      incp=0;
      for (m=0;m<=MM;m++) {
         Nm=JJ;      // triangular or trapezoidal
         if ( KK == JJ+MM ) Nm=JJ+m;          // rhombodial
         Ns=Js;      // triangular or trapezoidal
         if ( Ks == Js+Ms ) Ns=Js+m;          // rhombodial
         for (n=m;n<=Nm;n++) {
            if (n<=Ns && m<=Ms) {       // save unpacked value
               unpk[incu++]=fld[inc++];         // real part
               unpk[incu++]=fld[inc++];     // imaginary part
            }
            else {                       // Save value to be packed and scale
                                         // Laplacian scale factor
               tfld[incp++]=fld[inc++]*pscale[n];      // real part
               tfld[incp++]=fld[inc++]*pscale[n];      // imaginary part
            }
         }
      }

      free(pscale);

      if (incu != Ts) {
         printf("specpack: Incorrect number of unpacked values %d given:\n",(int)Ts);
         printf("specpack: Resetting idrstmpl[8] to %d\n",(int)incu);
         Ts=incu;
      }
//
//  Add unpacked values to the packed data array in 32-bit IEEE format
//
      mkieee(unpk,(g2int *)cpack,Ts);
      ipos=4*Ts;
//
//  Scale and pack the rest of the coefficients
//
      tmplsim[1]=idrstmpl[1];
      tmplsim[2]=idrstmpl[2];
      tmplsim[3]=idrstmpl[3];
      simpack(tfld,ndpts-Ts,tmplsim,cpack+ipos,lcpack);
      *lcpack=(*lcpack)+ipos;
//
//  Fill in Template 5.51
//
      idrstmpl[0]=tmplsim[0];
      idrstmpl[1]=tmplsim[1];
      idrstmpl[2]=tmplsim[2];
      idrstmpl[3]=tmplsim[3];
      idrstmpl[8]=Ts;
      idrstmpl[9]=1;         // Unpacked spectral data is 32-bit IEEE

      free(tfld);
      free(unpk);
      free(ifld);

      return;
}
