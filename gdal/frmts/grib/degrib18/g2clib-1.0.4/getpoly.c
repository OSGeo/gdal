#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int getpoly(unsigned char *csec3,g2int *jj,g2int *kk,g2int *mm)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    getpoly 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-12-11
//
// ABSTRACT: This subroutine returns the J, K, and M pentagonal resolution
//   parameters specified in a GRIB Grid Definition Section used
//   spherical harmonic coefficients using GDT 5.50 through 5.53
//
// PROGRAM HISTORY LOG:
// 2002-12-11  Gilbert
//
// USAGE:    int getpoly(unsigned char *csec3,g2int *jj,g2int *kk,g2int *mm)
//   INPUT ARGUMENTS:
//     csec3    - Character array that contains the packed GRIB2 GDS
//
//   OUTPUT ARGUMENTS:      
//         JJ   = J - pentagonal resolution parameter
//         KK   = K - pentagonal resolution parameter
//         MM   = M - pentagonal resolution parameter
//
// REMARKS:  Returns JJ, KK, and MM set to zero, if grid template
//           not recognized.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{
    
      g2int   *igdstmpl,*list_opt;
      g2int   *igds;
      g2int   iofst,igdtlen,num_opt,jerr;

      iofst=0;       // set offset to beginning of section
      jerr=g2_unpack3(csec3,&iofst,&igds,&igdstmpl,
                      &igdtlen,&list_opt,&num_opt);
      if (jerr == 0) {
         switch ( igds[4] )     //  Template number
         {
           case 50:     // Spherical harmonic coefficients
           case 51:
           case 52:
           case 53:
           {
              *jj=igdstmpl[0];
              *kk=igdstmpl[1];
              *mm=igdstmpl[2];
              break;
           }
           default:
           {
              *jj=0;
              *kk=0;
              *mm=0;
              break;
           }
         }     // end switch
      }
      else {
         *jj=0;
         *kk=0;
         *mm=0;
      }
        
      if (igds != 0) free(igds);
      if (igdstmpl != 0) free(igdstmpl);
      if (list_opt != 0) free(list_opt);

      return 0;
}
