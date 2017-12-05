#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int g2_unpack1(unsigned char *cgrib,g2int *iofst,g2int **ids,g2int *idslen)
/*//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_unpack1
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-29
//
// ABSTRACT: This subroutine unpacks Section 1 (Identification Section)
//           as defined in GRIB Edition 2.
//
// PROGRAM HISTORY LOG:
// 2002-10-29  Gilbert
//
// USAGE:    int g2_unpack1(unsigned char *cgrib,g2int *iofst,g2int **ids,
//                          g2int *idslen)
//   INPUT ARGUMENTS:
//     cgrib    - char array containing Section 1 of the GRIB2 message
//     iofst    - Bit offset for the beginning of Section 1 in cgrib.
//
//   OUTPUT ARGUMENTS:
//     iofst    - Bit offset at the end of Section 1, returned.
//     ids      - address of pointer to integer array containing information
//                read from Section 1, the Identification section.
//            ids[0]  = Identification of originating Centre
//                                 ( see Common Code Table C-1 )
//            ids[1]  = Identification of originating Sub-centre
//            ids[2]  = GRIB Master Tables Version Number
//                                 ( see Code Table 1.0 )
//            ids[3]  = GRIB Local Tables Version Number
//                                 ( see Code Table 1.1 )
//            ids[4]  = Significance of Reference Time (Code Table 1.2)
//            ids[5]  = Year ( 4 digits )
//            ids[6]  = Month
//            ids[7]  = Day
//            ids[8]  = Hour
//            ids[9]  = Minute
//            ids[10]  = Second
//            ids[11]  = Production status of processed data
//                                 ( see Code Table 1.3 )
//            ids[12]  = Type of processed data ( see Code Table 1.4 )
//     idslen   - Number of elements in ids[].
//
//   RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                2 = Array passed is not section 1
//                6 = memory allocation error
//
// REMARKS:
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$
*/
{

      g2int i,lensec,nbits,ierr,isecnum;
      g2int mapid[13]={2,2,1,1,1,2,1,1,1,1,1,1,1};

      ierr=0;
      *idslen=13;
      *ids=0;

      gbit(cgrib,&lensec,*iofst,32);        // Get Length of Section
      *iofst=*iofst+32;
      gbit(cgrib,&isecnum,*iofst,8);         // Get Section Number
      *iofst=*iofst+8;

      if ( isecnum != 1 ) {
         ierr=2;
         *idslen=13;
         fprintf(stderr,"g2_unpack1: Not Section 1 data.\n");
         return(ierr);
      }

      //
      //   Unpack each value into array ids from the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mapid.
      //
      *ids=(g2int *)calloc(*idslen,sizeof(g2int));
      if (*ids == 0) {
         ierr=6;
         return(ierr);
      }

      for (i=0;i<*idslen;i++) {
        nbits=mapid[i]*8;
        gbit(cgrib,*ids+i,*iofst,nbits);
        *iofst=*iofst+nbits;
      }

      return(ierr);    // End of Section 1 processing
}
