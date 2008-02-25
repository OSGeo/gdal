#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"


g2int g2_unpack5(unsigned char *cgrib,g2int *iofst,g2int *ndpts,g2int *idrsnum,
               g2int **idrstmpl,g2int *mapdrslen)
////$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_unpack5 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-31
//
// ABSTRACT: This subroutine unpacks Section 5 (Data Representation Section)
//           as defined in GRIB Edition 2.
//
// PROGRAM HISTORY LOG:
// 2002-10-31  Gilbert
//
// USAGE:    int g2_unpack5(unsigned char *cgrib,g2int *iofst,g2int *ndpts,
//                          g2int *idrsnum,g2int **idrstmpl,g2int *mapdrslen)
//   INPUT ARGUMENTS:
//     cgrib    - char array containing Section 5 of the GRIB2 message
//     iofst    - Bit offset for the beginning of Section 5 in cgrib.
//
//   OUTPUT ARGUMENTS:      
//     iofst    - Bit offset at the end of Section 5, returned.
//     ndpts    - Number of data points unpacked and returned.
//     idrsnum  - Data Representation Template Number ( see Code Table 5.0)
//     idrstmpl - Pointer to an integer array containing the data values for 
//                the specified Data Representation
//                Template ( N=idrsnum ).  Each element of this integer
//                array contains an entry (in the order specified) of Data
//                Representation Template 5.N
//     mapdrslen- Number of elements in idrstmpl[].  i.e. number of entries
//                in Data Representation Template 5.N  ( N=idrsnum ).
//
//   RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                2 = Not Section 5
//                6 = memory allocation error
//                7 = "GRIB" message contains an undefined Data
//                    Representation Template.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$//
{
      g2int ierr,needext,i,j,nbits,isecnum;
      g2int lensec,isign,newlen;
      g2int *lidrstmpl=0;
      xxtemplate *mapdrs;

      ierr=0;
      *idrstmpl=0;       //NULL

      gbit(cgrib,&lensec,*iofst,32);        // Get Length of Section
      *iofst=*iofst+32;
      gbit(cgrib,&isecnum,*iofst,8);         // Get Section Number
      *iofst=*iofst+8;

      if ( isecnum != 5 ) {
         ierr=2;
         *ndpts=0;
         *mapdrslen=0;
        // fprintf(stderr,"g2_unpack5: Not Section 5 data.\n");
         return(ierr);
      }

      gbit(cgrib,ndpts,*iofst,32);    // Get num of data points
      *iofst=*iofst+32;
      gbit(cgrib,idrsnum,*iofst,16);     // Get Data Rep Template Num.
      *iofst=*iofst+16;

      //   Gen Data Representation Template
      mapdrs=getdrstemplate(*idrsnum);
      if (mapdrs == 0) {
        ierr=7;
        *mapdrslen=0;
        return(ierr);
      }
      *mapdrslen=mapdrs->maplen;
      needext=mapdrs->needext;
      //
      //   Unpack each value into array ipdstmpl from the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mapdrs.
      //
      if (*mapdrslen > 0) lidrstmpl=(g2int *)calloc(*mapdrslen,sizeof(g2int));
      if (lidrstmpl == 0) {
         ierr=6;
         *mapdrslen=0;
         *idrstmpl=0;     //NULL
         if ( mapdrs != 0 ) free(mapdrs);
         return(ierr);
      }
      else {
         *idrstmpl=lidrstmpl;
      }
      for (i=0;i<mapdrs->maplen;i++) {
        nbits=abs(mapdrs->map[i])*8;
        if ( mapdrs->map[i] >= 0 ) {
          gbit(cgrib,lidrstmpl+i,*iofst,nbits);
        }
        else {
          gbit(cgrib,&isign,*iofst,1);
          gbit(cgrib,lidrstmpl+i,*iofst+1,nbits-1);
          if (isign == 1) lidrstmpl[i]=-1*lidrstmpl[i];
        }
        *iofst=*iofst+nbits;
      }
      //
      //   Check to see if the Data Representation Template needs to be
      //   extended.
      //   The number of values in a specific template may vary
      //   depending on data specified in the "static" part of the
      //   template.
      //
      if ( needext == 1 ) {
        free(mapdrs);
        mapdrs=extdrstemplate(*idrsnum,lidrstmpl);
        newlen=mapdrs->maplen+mapdrs->extlen;
        lidrstmpl=(g2int *)realloc(lidrstmpl,newlen*sizeof(g2int));
        *idrstmpl=lidrstmpl;
        //   Unpack the rest of the Data Representation Template
        j=0;
        for (i=*mapdrslen;i<newlen;i++) {
          nbits=abs(mapdrs->ext[j])*8;
          if ( mapdrs->ext[j] >= 0 ) {
            gbit(cgrib,lidrstmpl+i,*iofst,nbits);
          }
          else {
            gbit(cgrib,&isign,*iofst,1);
            gbit(cgrib,lidrstmpl+i,*iofst+1,nbits-1);
            if (isign == 1) lidrstmpl[i]=-1*lidrstmpl[i];
          }
          *iofst=*iofst+nbits;
          j++;
        }
        *mapdrslen=newlen;
      }
      if( mapdrs->ext != 0 ) free(mapdrs->ext);
      if( mapdrs != 0 ) free(mapdrs);

      return(ierr);    // End of Section 5 processing

}
