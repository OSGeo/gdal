#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int getdim(unsigned char *csec3,g2int *width,g2int *height,g2int *iscan)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    getdim 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-12-11
//
// ABSTRACT: This subroutine returns the dimensions and scanning mode of 
//   a grid definition packed in GRIB2 Grid Definition Section 3 format.
//
// PROGRAM HISTORY LOG:
// 2002-12-11  Gilbert
//
// USAGE:    int getdim(unsigned char *csec3,g2int *width,
//                      g2int *height, g2int *iscan)
//   INPUT ARGUMENT LIST:
//     csec3    - Character array that contains the packed GRIB2 GDS
//
//   OUTPUT ARGUMENT LIST:      
//     width    - x (or i) dimension of the grid.
//     height   - y (or j) dimension of the grid.
//     iscan    - Scanning mode ( see Code Table 3.4 )
//
// REMARKS:  Returns width and height set to zero, if grid template
//           not recognized.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{
    
      g2int  *igdstmpl,*list_opt;
      g2int  *igds;
      g2int  iofst,igdtlen,num_opt,jerr;

      igdstmpl=0;
      list_opt=0;
      igds=0;
      iofst=0;       // set offset to beginning of section
      jerr= g2_unpack3(csec3,&iofst,&igds,&igdstmpl,
                       &igdtlen,&list_opt,&num_opt);
      if (jerr == 0) {
         switch ( igds[4] )     //  Template number
         {
           case 0:    // Lat/Lon
           case 1:
           case 2:
           case 3:
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[18];
              break;
           }
           case 10:   // Mercator
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[15];
              break;
           }
           case 20:   // Polar Stereographic
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[17];
              break;
           }
           case 30:   // Lambert Conformal
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[17];
              break;
           }
           case 40:   // Gaussian
           case 41:
           case 42:
           case 43:
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[18];
              break;
           }
           case 90:   // Space View/Orthographic
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[16];
              break;
           }
           case 110:   // Equatorial Azimuthal
           {
              *width=igdstmpl[7];
              *height=igdstmpl[8];
              *iscan=igdstmpl[15];
              break;
           }
           default:
           {
              *width=0;
              *height=0;
              *iscan=0;
              break;
           }
         }  // end switch
      }
      else {
         *width=0;
         *height=0;
      }

      if (igds != 0) free(igds);
      if (igdstmpl != 0) free(igdstmpl);
      if (list_opt != 0) free(list_opt);

      return 0;
}
