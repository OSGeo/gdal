#include <stdio.h>
#include "grib2.h"

g2int g2_addlocal(unsigned char *cgrib,unsigned char *csec2,g2int lcsec2)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_addlocal
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-11-01
//
// ABSTRACT: This routine adds a Local Use Section (Section 2) to
//   a GRIB2 message.  It is used with routines "g2_create",
//   "g2_addgrid", "g2_addfield",
//   and "g2_gribend" to create a complete GRIB2 message.
//   g2_create must be called first to initialize a new GRIB2 message.
//
// PROGRAM HISTORY LOG:
// 2002-11-01  Gilbert
//
// USAGE:    int g2_addlocal(unsigned char *cgrib,unsigned char *csec2,
//                           g2int lcsec2)
//   INPUT ARGUMENTS:
//     cgrib    - Char array that contains the GRIB2 message to which section
//                2 should be added.
//     csec2    - Character array containing information to be added in
//                Section 2.
//     lcsec2   - Number of bytes of character array csec2 to be added to
//                Section 2.
//
//   OUTPUT ARGUMENT:
//     cgrib    - Char array to contain the updated GRIB2 message.
//                Must be allocated large enough to store the entire
//                GRIB2 message.
//
//   RETURN VALUES:
//     ierr     - Return code.
//              > 0 = Current size of updated GRIB2 message
//               -1 = GRIB message was not initialized.  Need to call
//                    routine gribcreate first.
//               -2 = GRIB message already complete.  Cannot add new section.
//               -3 = Sum of Section byte counts doesn't add to total byte count
//               -4 = Previous Section was not 1 or 7.
//
// REMARKS: Note that the Local Use Section ( Section 2 ) can only follow
//          Section 1 or Section 7 in a GRIB2 message.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$
{

      g2int ierr;
      const unsigned char G=0x47;       // 'G'
      const unsigned char R=0x52;       // 'R'
      const unsigned char I=0x49;       // 'I'
      const unsigned char B=0x42;       // 'B'
      const unsigned char seven=0x37;   // '7'

      const g2int two=2;
      g2int   j,k,lensec2,iofst,ibeg,lencurr,ilen,len,istart;
      g2int   isecnum;

      ierr=0;
//
//  Check to see if beginning of GRIB message exists
//
      if ( cgrib[0]!=G || cgrib[1]!=R || cgrib[2]!=I || cgrib[3]!=B ) {
        printf("g2_addlocal: GRIB not found in given message.\n");
        printf("g2_addlocal: Call to routine g2_create required to initialize GRIB message.\n");
        ierr=-1;
        return(ierr);
      }
//
//  Get current length of GRIB message
//
      gbit(cgrib,&lencurr,96,32);
//
//  Check to see if GRIB message is already complete
//
      if ( cgrib[lencurr-4]==seven && cgrib[lencurr-3]==seven &&
           cgrib[lencurr-2]==seven && cgrib[lencurr-1]==seven ) {
        printf("g2_addlocal: GRIB message already complete.  Cannot add new section.\n");
        ierr=-2;
        return(ierr);
      }
//
//  Loop through all current sections of the GRIB message to
//  find the last section number.
//
      len=16;    // length of Section 0
      for (;;) {
      //    Get section number and length of next section
        iofst=len*8;
        gbit(cgrib,&ilen,iofst,32);
        iofst=iofst+32;
        gbit(cgrib,&isecnum,iofst,8);
        len=len+ilen;
      //    Exit loop if last section reached
        if ( len == lencurr ) break;
      //    If byte count for each section doesn't match current
      //    total length, then there is a problem.
        if ( len > lencurr ) {
          printf("g2_addlocal: Section byte counts don't add to total.\n");
          printf("g2_addlocal: Sum of section byte counts = %d\n",len);
          printf("g2_addlocal: Total byte count in Section 0 = %d\n",lencurr);
          ierr=-3;
          return(ierr);
        }
      }
//
//  Section 2 can only be added after sections 1 and 7.
//
      if ( (isecnum!=1) && (isecnum!=7) ) {
        printf("g2_addlocal: Section 2 can only be added after Section 1 or Section 7.\n");
        printf("g2_addlocal: Section %d was the last found in given GRIB message.\n",isecnum);
        ierr=-4;
        return(ierr);
      }
//
//  Add Section 2  - Local Use Section
//
      ibeg=lencurr*8;        //   Calculate offset for beginning of section 2
      iofst=ibeg+32;         //   leave space for length of section
      sbit(cgrib,&two,iofst,8);     // Store section number ( 2 )
      istart=lencurr+5;
      //cgrib(istart+1:istart+lcsec2)=csec2(1:lcsec2)
      k=0;
      for (j=istart;j<istart+lcsec2;j++) {
         cgrib[j]=csec2[k++];
      }
      //
      //   Calculate length of section 2 and store it in octets
      //   1-4 of section 2.
      //
      lensec2=lcsec2+5;      // bytes
      sbit(cgrib,&lensec2,ibeg,32);

//
//  Update current byte total of message in Section 0
//
      lencurr+=lensec2;
      sbit(cgrib,&lencurr,96,32);

      return(lencurr);

}
