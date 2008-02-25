#include <stdio.h>
#include "grib2.h"

#define MAPSEC1LEN 13

g2int g2_create(unsigned char *cgrib,g2int *listsec0,g2int *listsec1)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_create 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-31
//
// ABSTRACT: This routine initializes a new GRIB2 message and packs
//   GRIB2 sections 0 (Indicator Section) and 1 (Identification Section).
//   This routine is used with routines "g2_addlocal", "g2_addgrid", 
//   "g2_addfield", and "g2_gribend" to create a complete GRIB2 message.  
//   g2_create must be called first to initialize a new GRIB2 message.
//   Also, a call to g2_gribend is required to complete GRIB2 message
//   after all fields have been added.
//
// PROGRAM HISTORY LOG:
// 2002-10-31  Gilbert
//
// USAGE:    int g2_create(unsigned char *cgrib,g2int *listsec0,g2int *listsec1)
//   INPUT ARGUMENTS:
//     cgrib    - Character array to contain the GRIB2 message
//     listsec0 - Contains information needed for GRIB Indicator Section 0.
//                Must be dimensioned >= 2.
//                listsec0[0]=Discipline-GRIB Master Table Number
//                            (see Code Table 0.0)
//                listsec0[1]=GRIB Edition Number (currently 2)
//     listsec1 - Contains information needed for GRIB Identification Section 1.
//                Must be dimensioned >= 13.
//                listsec1[0]=Id of orginating centre (Common Code Table C-1)
//                listsec1[1]=Id of orginating sub-centre (local table)
//                listsec1[2]=GRIB Master Tables Version Number (Code Table 1.0)
//                listsec1[3]=GRIB Local Tables Version Number (Code Table 1.1)
//                listsec1[4]=Significance of Reference Time (Code Table 1.2)
//                listsec1[5]=Reference Time - Year (4 digits)
//                listsec1[6]=Reference Time - Month
//                listsec1[7]=Reference Time - Day
//                listsec1[8]=Reference Time - Hour
//                listsec1[9]=Reference Time - Minute
//                listsec1[10]=Reference Time - Second
//                listsec1[11]=Production status of data (Code Table 1.3)
//                listsec1[12]=Type of processed data (Code Table 1.4)
//
//   OUTPUT ARGUMENTS:      
//     cgrib    - Char array to contain the new GRIB2 message.
//                Must be allocated large enough to store the entire
//                GRIB2 message.
//
//   RETURN VALUES:
//     ierr     - return code.
//              > 0 = Current size of new GRIB2 message
//               -1 = Tried to use for version other than GRIB Edition 2
//
// REMARKS: This routine is intended for use with routines "g2_addlocal", 
//          "g2_addgrid", "g2_addfield", and "g2_gribend" to create a complete 
//          GRIB2 message.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{

      g2int  ierr;
      g2int   zero=0,one=1;
      g2int   mapsec1len=MAPSEC1LEN;
      g2int   mapsec1[MAPSEC1LEN]={ 2,2,1,1,1,2,1,1,1,1,1,1,1 };
      g2int   i,lensec0,lensec1,iofst,ibeg,nbits,len;

      ierr=0;
//
//  Currently handles only GRIB Edition 2.
//  
      if (listsec0[1] != 2) {
        printf("g2_create: can only code GRIB edition 2.");
        ierr=-1;
        return (ierr);
      }
//
//  Pack Section 0 - Indicator Section 
//  ( except for total length of GRIB message )
//
      cgrib[0]=0x47;   // 'G'            // Beginning of GRIB message
      cgrib[1]=0x52;   // 'R'
      cgrib[2]=0x49;   // 'I'
      cgrib[3]=0x42;   // 'B'
      sbit(cgrib,&zero,32,16);           // reserved for future use
      sbit(cgrib,listsec0+0,48,8);       // Discipline
      sbit(cgrib,listsec0+1,56,8);       // GRIB edition number
      lensec0=16;      // bytes (octets)
//
//  Pack Section 1 - Identification Section
//
      ibeg=lensec0*8;        //   Calculate offset for beginning of section 1
      iofst=ibeg+32;         //   leave space for length of section
      sbit(cgrib,&one,iofst,8);     // Store section number ( 1 )
      iofst=iofst+8;
      //
      //   Pack up each input value in array listsec1 into the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mapsec1.
      //
      for (i=0;i<mapsec1len;i++) {
        nbits=mapsec1[i]*8;
        sbit(cgrib,listsec1+i,iofst,nbits);
        iofst=iofst+nbits;
      }
      //
      //   Calculate length of section 1 and store it in octets
      //   1-4 of section 1.
      //
      lensec1=(iofst-ibeg)/8;
      sbit(cgrib,&lensec1,ibeg,32);
//
//  Put current byte total of message into Section 0
//
      sbit(cgrib,&zero,64,32);
      len=lensec0+lensec1;
      sbit(cgrib,&len,96,32);

      return (len);

}
