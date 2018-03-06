#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int g2_unpack2(unsigned char *cgrib,g2int *iofst,g2int *lencsec2,unsigned char **csec2)
////$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_unpack2
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-31
//
// ABSTRACT: This subroutine unpacks Section 2 (Local Use Section)
//           as defined in GRIB Edition 2.
//
// PROGRAM HISTORY LOG:
// 2002-10-31  Gilbert
// 2008-12-23  Wesley   - Initialize lencsec2 Length of Local Use data
// 2010-08-05  Vuong    - If section 2 has zero length, ierr=0
//
// USAGE:    int g2_unpack2(unsigned char *cgrib,g2int *iofst,g2int *lencsec2,
//                          unsigned char **csec2)
//   INPUT ARGUMENT LIST:
//     cgrib    - char array containing Section 2 of the GRIB2 message
//     iofst    - Bit offset for the beginning of Section 2 in cgrib.
//
//   OUTPUT ARGUMENT LIST:
//     iofst    - Bit offset at the end of Section 2, returned.
//     lencsec2 - Length (in octets) of Local Use data
//     csec2    - Pointer to a char array containing local use data
//
//   RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                2 = Array passed is not section 2
//                6 = memory allocation error
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$//
{

      g2int ierr,isecnum;
      g2int lensec,ipos,j;

      ierr=0;
      *lencsec2=0;
      *csec2=0;    // NULL

      gbit(cgrib,&lensec,*iofst,32);        // Get Length of Section
      *iofst=*iofst+32;
      *lencsec2=lensec-5;
      gbit(cgrib,&isecnum,*iofst,8);         // Get Section Number
      *iofst=*iofst+8;
      ipos=(*iofst/8);

      if ( isecnum != 2 ) {
         ierr=2;
         *lencsec2=0;
         fprintf(stderr,"g2_unpack2: Not Section 2 data.\n");
         return(ierr);
      }

      if (*lencsec2 == 0) {
         ierr = 0;
         return(ierr);
      }

      *csec2=(unsigned char *)malloc(*lencsec2+1);
      if (*csec2 == 0) {
         ierr=6;
         *lencsec2=0;
         return(ierr);
      }

      //printf(" SAGIPO %d \n",(int)ipos);
      for (j=0;j<*lencsec2;j++) {
         *(*csec2+j)=cgrib[ipos+j];
      }
      *iofst=*iofst+(*lencsec2*8);

      return(ierr);    // End of Section 2 processing

}
