#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"


g2int g2_unpack4(unsigned char *cgrib,g2int *iofst,g2int *ipdsnum,g2int **ipdstmpl,
               g2int *mappdslen,g2float **coordlist,g2int *numcoord)
////$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_unpack4 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-31
//
// ABSTRACT: This subroutine unpacks Section 4 (Product Definition Section)
//           as defined in GRIB Edition 2.
//
// PROGRAM HISTORY LOG:
// 2002-10-31  Gilbert
//
// USAGE:    int g2_unpack4(unsigned char *cgrib,g2int *iofst,g2int *ipdsnum,
//                          g2int **ipdstmpl,g2int *mappdslen,
//                          g2float **coordlist,g2int *numcoord)
//   INPUT ARGUMENTS:
//     cgrib    - Char array containing Section 4 of the GRIB2 message
//     iofst    - Bit offset of the beginning of Section 4 in cgrib.
//
//   OUTPUT ARGUMENTS:      
//     iofst    - Bit offset of the end of Section 4, returned.
//     ipdsnum  - Product Definition Template Number ( see Code Table 4.0)
//     ipdstmpl - Pointer to integer array containing the data values for 
//                the specified Product Definition
//                Template ( N=ipdsnum ).  Each element of this integer
//                array contains an entry (in the order specified) of Product
//                Definition Template 4.N
//     mappdslen- Number of elements in ipdstmpl[].  i.e. number of entries
//                in Product Definition Template 4.N  ( N=ipdsnum ).
//     coordlist- Pointer to real array containing floating point values 
//                intended to document
//                the vertical discretisation associated to model data
//                on hybrid coordinate vertical levels.  (part of Section 4)
//     numcoord - number of values in array coordlist.
//
//   RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                2 = Not section 4
//                5 = "GRIB" message contains an undefined Product Definition
//                    Template.
//                6 = memory allocation error
//
// REMARKS: 
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$//
{

      g2int ierr,needext,i,j,nbits,isecnum;
      g2int lensec,isign,newlen;
      g2int *coordieee;
      g2int *lipdstmpl=0;
      g2float *lcoordlist;
      xxtemplate *mappds;

      ierr=0;
      *ipdstmpl=0;    // NULL
      *coordlist=0;    // NULL

      gbit(cgrib,&lensec,*iofst,32);        // Get Length of Section
      *iofst=*iofst+32;
      gbit(cgrib,&isecnum,*iofst,8);         // Get Section Number
      *iofst=*iofst+8;

      if ( isecnum != 4 ) {
         ierr=2;
         *numcoord=0;
         *mappdslen=0;
        // fprintf(stderr,"g2_unpack4: Not Section 4 data.\n");
         return(ierr);
      }

      gbit(cgrib,numcoord,*iofst,16);    // Get num of coordinate values
      *iofst=*iofst+16;
      gbit(cgrib,ipdsnum,*iofst,16);    // Get Prod. Def Template num.
      *iofst=*iofst+16;

      //   Get Product Definition Template
      mappds=getpdstemplate(*ipdsnum);
      if (mappds == 0) {       // undefine template
        ierr=5;
        *mappdslen=0;
        return(ierr);
      }
      *mappdslen=mappds->maplen;
      needext=mappds->needext;
      //
      //   Unpack each value into array ipdstmpl from the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mappds.
      //
      if (*mappdslen > 0) lipdstmpl=(g2int *)calloc(*mappdslen,sizeof(g2int));
      if (lipdstmpl == 0) {
         ierr=6;
         *mappdslen=0;
         *ipdstmpl=0;     //NULL
         if ( mappds != 0 ) free(mappds);
         return(ierr);
      }
      else {
         *ipdstmpl=lipdstmpl;
      }
      for (i=0;i<mappds->maplen;i++) {
        nbits=abs(mappds->map[i])*8;
        if ( mappds->map[i] >= 0 ) {
          gbit(cgrib,lipdstmpl+i,*iofst,nbits);
        }
        else {
          gbit(cgrib,&isign,*iofst,1);
          gbit(cgrib,lipdstmpl+i,*iofst+1,nbits-1);
          if (isign == 1) lipdstmpl[i]=-1*lipdstmpl[i];
        }
        *iofst=*iofst+nbits;
      }
      //
      //   Check to see if the Product Definition Template needs to be
      //   extended.
      //   The number of values in a specific template may vary
      //   depending on data specified in the "static" part of the
      //   template.
      //
      if ( needext ==1 ) {
        free(mappds);
        mappds=extpdstemplate(*ipdsnum,lipdstmpl);
        newlen=mappds->maplen+mappds->extlen;
        lipdstmpl=(g2int *)realloc(lipdstmpl,newlen*sizeof(g2int));
        *ipdstmpl=lipdstmpl;
        //   Unpack the rest of the Product Definition Template
        j=0;
        for (i=*mappdslen;i<newlen;i++) {
          nbits=abs(mappds->ext[j])*8;
          if ( mappds->ext[j] >= 0 ) {
            gbit(cgrib,lipdstmpl+i,*iofst,nbits);
          }
          else {
            gbit(cgrib,&isign,*iofst,1);
            gbit(cgrib,lipdstmpl+i,*iofst+1,nbits-1);
            if (isign == 1) lipdstmpl[i]=-1*lipdstmpl[i];
          }
          *iofst=*iofst+nbits;
          j++;
        }
        *mappdslen=newlen;
      }
      if( mappds->ext != 0 ) free(mappds->ext);
      if( mappds != 0 ) free(mappds);
      //
      //   Get Optional list of vertical coordinate values
      //   after the Product Definition Template, if necessary.
      //
      *coordlist=0;    // NULL
      if ( *numcoord != 0 ) {
         coordieee=(g2int *)calloc(*numcoord,sizeof(g2int));
         lcoordlist=(g2float *)calloc(*numcoord,sizeof(g2float));
         if (coordieee == 0 || lcoordlist == 0) {
            ierr=6;
            *numcoord=0;
            *coordlist=0;    // NULL
            if( coordieee != 0 ) free(coordieee);
            if( lcoordlist != 0 ) free(lcoordlist);
            return(ierr);
         }
         else {
            *coordlist=lcoordlist;
         }
        gbits(cgrib,coordieee,*iofst,32,0,*numcoord);
        rdieee(coordieee,*coordlist,*numcoord);
        free(coordieee);
        *iofst=*iofst+(32*(*numcoord));
      }
      
      return(ierr);    // End of Section 4 processing

}
