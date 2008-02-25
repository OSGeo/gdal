#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int g2_info(unsigned char *cgrib,g2int *listsec0,g2int *listsec1,
            g2int *numfields,g2int *numlocal)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_info 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-28
//
// ABSTRACT: This subroutine searches through a GRIB2 message and
//   returns the number of gridded fields found in the message and
//   the number (and maximum size) of Local Use Sections.
//   Also various checks  are performed
//   to see if the message is a valid GRIB2 message.
//
// PROGRAM HISTORY LOG:
// 2002-10-28  Gilbert
//
// USAGE:   int g2_info(unsigned char *cgrib,g2int *listsec0,g2int *listsec1,
//            g2int *numfields,g2int *numlocal)
//   INPUT ARGUMENT:
//     cgrib    - Character pointer to the GRIB2 message
//
//   OUTPUT ARGUMENTS:      
//     listsec0 - pointer to an array containing information decoded from 
//                GRIB Indicator Section 0.
//                Must be allocated with >= 3 elements.
//                listsec0[0]=Discipline-GRIB Master Table Number
//                            (see Code Table 0.0)
//                listsec0[1]=GRIB Edition Number (currently 2)
//                listsec0[2]=Length of GRIB message
//     listsec1 - pointer to an array containing information read from GRIB 
//                Identification Section 1.
//                Must be allocated with >= 13 elements.
//                listsec1[0]=Id of orginating centre (Common Code Table C-1)
//                listsec1[1]=Id of orginating sub-centre (local table)
//                listsec1[2]=GRIB Master Tables Version Number (Code Table 1.0)
//                listsec1[3]=GRIB Local Tables Version Number 
//                listsec1[4]=Significance of Reference Time (Code Table 1.1)
//                listsec1[5]=Reference Time - Year (4 digits)
//                listsec1[6]=Reference Time - Month
//                listsec1[7]=Reference Time - Day
//                listsec1[8]=Reference Time - Hour
//                listsec1[9]=Reference Time - Minute
//                listsec1[10]=Reference Time - Second
//                listsec1[11]=Production status of data (Code Table 1.2)
//                listsec1[12]=Type of processed data (Code Table 1.3)
//     numfields- The number of gridded fields found in the GRIB message.
//                That is, the number of occurences of Sections 4 - 7.
//     numlocal - The number of Local Use Sections ( Section 2 ) found in 
//                the GRIB message.
//
//     RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                1 = Beginning characters "GRIB" not found.
//                2 = GRIB message is not Edition 2.
//                3 = Could not find Section 1, where expected.
//                4 = End string "7777" found, but not where expected.
//                5 = End string "7777" not found at end of message.
//                6 = Invalid section number found.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{
 
      g2int ierr,mapsec1len=13;
      g2int mapsec1[13]={2,2,1,1,1,2,1,1,1,1,1,1,1};
      g2int  i,j,istart,iofst,lengrib,lensec0,lensec1;
      g2int ipos,isecnum,nbits,lensec;

      ierr=0;
      *numlocal=0;
      *numfields=0;
//
//  Check for beginning of GRIB message in the first 100 bytes
//
      istart=-1;
      for (j=0;j<100;j++) {
        if (cgrib[j]=='G' && cgrib[j+1]=='R' &&cgrib[j+2]=='I' &&
            cgrib[j+3]=='B') {
          istart=j;
          break;
        }
      }
      if (istart == -1) {
        printf("g2_info:  Beginning characters GRIB not found.");
        ierr=1;
        return(ierr);
      }
//
//  Unpack Section 0 - Indicator Section 
//
      iofst=8*(istart+6);
      gbit(cgrib,listsec0+0,iofst,8);     // Discipline
      iofst=iofst+8;
      gbit(cgrib,listsec0+1,iofst,8);     // GRIB edition number
      iofst=iofst+8;
      iofst=iofst+32;
      gbit(cgrib,&lengrib,iofst,32);        // Length of GRIB message
      iofst=iofst+32;
      listsec0[2]=lengrib;
      lensec0=16;
      ipos=istart+lensec0;
//
//  Currently handles only GRIB Edition 2.
//  
      if (listsec0[1] != 2) {
        printf("g2_info: can only decode GRIB edition 2.");
        ierr=2;
        return(ierr);
      }
//
//  Unpack Section 1 - Identification Section
//
      gbit(cgrib,&lensec1,iofst,32);        // Length of Section 1
      iofst=iofst+32;
      gbit(cgrib,&isecnum,iofst,8);         // Section number ( 1 )
      iofst=iofst+8;
      if (isecnum != 1) {
        printf("g2_info: Could not find section 1.");
        ierr=3;
        return(ierr);
      }
      //
      //   Unpack each input value in array listsec1 into the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mapsec1.
      //
      for (i=0;i<mapsec1len;i++) {
        nbits=mapsec1[i]*8;
        gbit(cgrib,listsec1+i,iofst,nbits);
        iofst=iofst+nbits;
      }
      ipos=ipos+lensec1;
//
//  Loop through the remaining sections to see if they are valid.
//  Also count the number of times Section 2
//  and Section 4 appear.
//
      for (;;) {
        if (cgrib[ipos]=='7' && cgrib[ipos+1]=='7' && cgrib[ipos+2]=='7' &&
            cgrib[ipos+3]=='7') {
          ipos=ipos+4;
          if (ipos != (istart+lengrib)) {
            printf("g2_info: '7777' found, but not where expected.\n");
            ierr=4;
            return(ierr);
          }
          break;
        }
        
        iofst=ipos*8;
        gbit(cgrib,&lensec,iofst,32);        // Get Length of Section
        iofst=iofst+32;
        gbit(cgrib,&isecnum,iofst,8);         // Get Section number
        iofst=iofst+8;
        ipos=ipos+lensec;                 // Update beginning of section pointer
        if (ipos > (istart+lengrib)) {
          printf("g2_info: '7777'  not found at end of GRIB message.\n");
          ierr=5;
          return(ierr);
        }
        if ( isecnum>=2 && isecnum<=7 ) {
           if (isecnum == 2)      // Local Section 2
              //   increment counter for total number of local sections found
              (*numlocal)++;
            
           else if (isecnum == 4)
              //   increment counter for total number of fields found
              (*numfields)++;
        }
        else {
           printf("g2_info: Invalid section number found in GRIB message: %d\n"                   ,isecnum);
           ierr=6;
           return(ierr);
        }
        
      }

      return(0);

}
