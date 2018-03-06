#include <stdlib.h>
#include <math.h>
#include "grib2.h"


void compack(g2float *fld,g2int ndpts,g2int idrsnum,g2int *idrstmpl,
             unsigned char *cpack,g2int *lcpack)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    compack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2002-11-07
//
// ABSTRACT: This subroutine packs up a data field using a complex
//   packing algorithm as defined in the GRIB2 documentation.  It
//   supports GRIB2 complex packing templates with or without
//   spatial differences (i.e. DRTs 5.2 and 5.3).
//   It also fills in GRIB2 Data Representation Template 5.2 or 5.3
//   with the appropriate values.
//
// PROGRAM HISTORY LOG:
// 2002-11-07  Gilbert
//
// USAGE:    void compack(g2float *fld,g2int ndpts,g2int idrsnum,
//                g2int *idrstmpl,unsigned char *cpack,g2int *lcpack)
//
//   INPUT ARGUMENTS:
//     fld[]    - Contains the data values to pack
//     ndpts    - The number of data values in array fld[]
//     idrsnum  - Data Representation Template number 5.N
//                Must equal 2 or 3.
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.2 or 5.3
//                [0] = Reference value - ignored on input
//                [1] = Binary Scale Factor
//                [2] = Decimal Scale Factor
//                    .
//                    .
//                [6] = Missing value management
//                [7] = Primary missing value
//                [8] = Secondary missing value
//                    .
//                    .
//               [16] = Order of Spatial Differencing  ( 1 or 2 )
//                    .
//                    .
//
//   OUTPUT ARGUMENTS:
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.3
//                [0] = Reference value - set by compack routine.
//                [1] = Binary Scale Factor - unchanged from input
//                [2] = Decimal Scale Factor - unchanged from input
//                    .
//                    .
//     cpack    - The packed data field
//     lcpack   - length of packed field cpack (or -1 in case of error)
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$
{

      const g2int zero=0;
      g2int  *ifld,*gref,*glen,*gwidth;
      g2int  *jmin, *jmax, *lbit;
      g2int  i,j,n, /* nbits, */ imin,imax,left;
      g2int  isd,itemp,ilmax,ngwidthref=0,nbitsgwidth=0;
      g2int  nglenref=0,nglenlast=0,iofst,ival1,ival2=0;
      g2int  minsd,nbitsd=0,maxorig,nbitorig,ngroups;
      g2int  lg,ng,igmax,iwmax,nbitsgref;
      g2int  glength,grpwidth,nbitsglen=0;
      g2int  kfildo, minpk, inc, maxgrps, ibit, jbit, kbit, novref, lbitref;
      g2int  missopt, miss1, miss2, ier = 0;
      g2float  bscale,dscale,rmax,rmin,temp;
      const g2int simple_alg = 0;
      const g2float alog2=0.69314718f;       //  ln(2.0)
      const g2int one=1;

      bscale=(float)int_power(2.0,-idrstmpl[1]);
      dscale=(float)int_power(10.0,idrstmpl[2]);
//
//  Find max and min values in the data
//
      rmax=fld[0];
      rmin=fld[0];
      for (j=1;j<ndpts;j++) {
        if (fld[j] > rmax) rmax=fld[j];
        if (fld[j] < rmin) rmin=fld[j];
      }

//
//  If max and min values are not equal, pack up field.
//  If they are equal, we have a constant field, and the reference
//  value (rmin) is the value for each point in the field and
//  set nbits to 0.
//
      if (rmin != rmax) {
        iofst=0;
        ifld=calloc(ndpts,sizeof(g2int));
        gref=calloc(ndpts,sizeof(g2int));
        gwidth=calloc(ndpts,sizeof(g2int));
        glen=calloc(ndpts,sizeof(g2int));
        if( ifld == NULL || gref == NULL || gwidth == NULL || glen == NULL )
        {
            free(ifld);
            free(gref);
            free(gwidth);
            free(glen);
            *lcpack = -1;
            return;
        }
        //
        //  Scale original data
        //
        if (idrstmpl[1] == 0) {        //  No binary scaling
           imin=(g2int)RINT(rmin*dscale);
           //imax=(g2int)rint(rmax*dscale);
           rmin=(g2float)imin;
           for (j=0;j<ndpts;j++)
              ifld[j]=(g2int)RINT(fld[j]*dscale)-imin;
        }
        else {                             //  Use binary scaling factor
           rmin=rmin*dscale;
           //rmax=rmax*dscale;
           for (j=0;j<ndpts;j++)
             ifld[j]=(g2int)RINT(((fld[j]*dscale)-rmin)*bscale);
        }
        //
        //  Calculate spatial differences, if using DRS Template 5.3.
        //
        if (idrsnum == 3) {        // spatial differences
           if (idrstmpl[16]!=1 && idrstmpl[16]!=2) idrstmpl[16]=1;
           if (idrstmpl[16] == 1) {      // first order
              ival1=ifld[0];
              for (j=ndpts-1;j>0;j--)
                 ifld[j]=ifld[j]-ifld[j-1];
              ifld[0]=0;
           }
           else if (idrstmpl[16] == 2) {      // second order
              ival1=ifld[0];
              ival2=ifld[1];
              for (j=ndpts-1;j>1;j--)
                 ifld[j]=ifld[j]-(2*ifld[j-1])+ifld[j-2];
              ifld[0]=0;
              ifld[1]=0;
           }
           //
           //  subtract min value from spatial diff field
           //
           isd=idrstmpl[16];
           minsd=ifld[isd];
           for (j=isd;j<ndpts;j++)  if ( ifld[j] < minsd ) minsd=ifld[j];
           for (j=isd;j<ndpts;j++)  ifld[j]=ifld[j]-minsd;
           //
           //   find num of bits need to store minsd and add 1 extra bit
           //   to indicate sign
           //
           temp=(float)(log((double)(abs(minsd)+1))/alog2);
           nbitsd=(g2int)ceil(temp)+1;
           //
           //   find num of bits need to store ifld[0] ( and ifld[1]
           //   if using 2nd order differencing )
           //
           maxorig=ival1;
           if (idrstmpl[16]==2 && ival2>ival1) maxorig=ival2;
           temp=(float)(log((double)(maxorig+1))/alog2);
           nbitorig=(g2int)ceil(temp)+1;
           if (nbitorig > nbitsd) nbitsd=nbitorig;
           //   increase number of bits to even multiple of 8 ( octet )
           if ( (nbitsd%8) != 0) nbitsd=nbitsd+(8-(nbitsd%8));
           //
           //  Store extra spatial differencing info into the packed
           //  data section.
           //
           if (nbitsd != 0) {
              //   pack first original value
              if (ival1 >= 0) {
                 sbit(cpack,&ival1,iofst,nbitsd);
                 iofst=iofst+nbitsd;
              }
              else {
                 sbit(cpack,&one,iofst,1);
                 iofst=iofst+1;
                 itemp=abs(ival1);
                 sbit(cpack,&itemp,iofst,nbitsd-1);
                 iofst=iofst+nbitsd-1;
              }
              if (idrstmpl[16] == 2) {
               //  pack second original value
                 if (ival2 >= 0) {
                    sbit(cpack,&ival2,iofst,nbitsd);
                    iofst=iofst+nbitsd;
                 }
                 else {
                    sbit(cpack,&one,iofst,1);
                    iofst=iofst+1;
                    itemp=abs(ival2);
                    sbit(cpack,&itemp,iofst,nbitsd-1);
                    iofst=iofst+nbitsd-1;
                 }
              }
              //  pack overall min of spatial differences
              if (minsd >= 0) {
                 sbit(cpack,&minsd,iofst,nbitsd);
                 iofst=iofst+nbitsd;
              }
              else {
                 sbit(cpack,&one,iofst,1);
                 iofst=iofst+1;
                 itemp=abs(minsd);
                 sbit(cpack,&itemp,iofst,nbitsd-1);
                 iofst=iofst+nbitsd-1;
              }
           }
           //printf("SDp %ld %ld %ld %ld\n",ival1,ival2,minsd,nbitsd);
        }     //  end of spatial diff section
        //
        //   Determine Groups to be used.
        //
        if ( simple_alg == 1 ) {
           //  set group length to 10;  calculate number of groups
           //  and length of last group
           ngroups=ndpts/10;
           for (j=0;j<ngroups;j++) glen[j]=10;
           itemp=ndpts%10;
           if (itemp != 0) {
              ngroups=ngroups+1;
              glen[ngroups-1]=itemp;
           }
        }
        else {
           // Use Dr. Glahn's algorithm for determining grouping.
           //
           kfildo=6;
           minpk=10;
           inc=1;
           maxgrps=(ndpts/minpk)+1;
           jmin = calloc(maxgrps,sizeof(g2int));
           jmax = calloc(maxgrps,sizeof(g2int));
           lbit = calloc(maxgrps,sizeof(g2int));
           if( jmin == NULL || jmax == NULL || lbit == NULL )
           {
                free(jmin);
                free(jmax);
                free(lbit);

                free(ifld);
                free(gref);
                free(gwidth);
                free(glen);
                *lcpack = -1;
                return;
           }
           missopt=0;
           pack_gp(&kfildo,ifld,&ndpts,&missopt,&minpk,&inc,&miss1,&miss2,
                        jmin,jmax,lbit,glen,&maxgrps,&ngroups,&ibit,&jbit,
                        &kbit,&novref,&lbitref,&ier);
           //print *,'SAGier = ',ier,ibit,jbit,kbit,novref,lbitref
           for ( ng=0; ng<ngroups; ng++) glen[ng]=glen[ng]+novref;
           free(jmin);
           free(jmax);
           free(lbit);
           if( ier != 0 )
           {
                free(ifld);
                free(gref);
                free(gwidth);
                free(glen);
                *lcpack = -1;
                return;
           }
        }
        //
        //  For each group, find the group's reference value
        //  and the number of bits needed to hold the remaining values
        //
        n=0;
        for (ng=0;ng<ngroups;ng++) {
           //    find max and min values of group
           gref[ng]=ifld[n];
           imax=ifld[n];
           j=n+1;
           for (lg=1;lg<glen[ng];lg++) {
              if (ifld[j] < gref[ng]) gref[ng]=ifld[j];
              if (ifld[j] > imax) imax=ifld[j];
              j++;
           }
           //   calc num of bits needed to hold data
           if ( gref[ng] != imax ) {
              temp=(float)(log((double)(imax-gref[ng]+1))/alog2);
              gwidth[ng]=(g2int)ceil(temp);
           }
           else
              gwidth[ng]=0;
           //   Subtract min from data
           j=n;
           for (lg=0;lg<glen[ng];lg++) {
              ifld[j]=ifld[j]-gref[ng];
              j++;
           }
           //   increment fld array counter
           n=n+glen[ng];
        }
        //
        //  Find max of the group references and calc num of bits needed
        //  to pack each groups reference value, then
        //  pack up group reference values
        //
        igmax=gref[0];
        for (j=1;j<ngroups;j++) if (gref[j] > igmax) igmax=gref[j];
        if (igmax != 0) {
           temp=(float)(log((double)(igmax+1))/alog2);
           nbitsgref=(g2int)ceil(temp);
           sbits(cpack,gref,iofst,nbitsgref,0,ngroups);
           itemp=nbitsgref*ngroups;
           iofst=iofst+itemp;
           //         Pad last octet with Zeros, if necessary,
           if ( (itemp%8) != 0) {
              left=8-(itemp%8);
              sbit(cpack,&zero,iofst,left);
              iofst=iofst+left;
           }
        }
        else
           nbitsgref=0;
        //
        //  Find max/min of the group widths and calc num of bits needed
        //  to pack each groups width value, then
        //  pack up group width values
        //
        iwmax=gwidth[0];
        ngwidthref=gwidth[0];
        for (j=1;j<ngroups;j++) {
           if (gwidth[j] > iwmax) iwmax=gwidth[j];
           if (gwidth[j] < ngwidthref) ngwidthref=gwidth[j];
        }
        if (iwmax != ngwidthref) {
           temp=(float)(log((double)(iwmax-ngwidthref+1))/alog2);
           nbitsgwidth=(g2int)ceil(temp);
           for (i=0;i<ngroups;i++)
              gwidth[i]=gwidth[i]-ngwidthref;
           sbits(cpack,gwidth,iofst,nbitsgwidth,0,ngroups);
           itemp=nbitsgwidth*ngroups;
           iofst=iofst+itemp;
           //         Pad last octet with Zeros, if necessary,
           if ( (itemp%8) != 0) {
              left=8-(itemp%8);
              sbit(cpack,&zero,iofst,left);
              iofst=iofst+left;
           }
        }
        else {
           nbitsgwidth=0;
           for (i=0;i<ngroups;i++) gwidth[i]=0;
        }
        //
        //  Find max/min of the group lengths and calc num of bits needed
        //  to pack each groups length value, then
        //  pack up group length values
        //
        //write(77,*)'GLENS: ',(glen(j),j=1,ngroups)
        ilmax=glen[0];
        nglenref=glen[0];
        for (j=1;j<ngroups-1;j++) {
           if (glen[j] > ilmax) ilmax=glen[j];
           if (glen[j] < nglenref) nglenref=glen[j];
        }
        nglenlast=glen[ngroups-1];
        if (ilmax != nglenref) {
           temp=(float)(log((double)(ilmax-nglenref+1))/alog2);
           nbitsglen=(g2int)ceil(temp);
           for (i=0;i<ngroups-1;i++)  glen[i]=glen[i]-nglenref;
           sbits(cpack,glen,iofst,nbitsglen,0,ngroups);
           itemp=nbitsglen*ngroups;
           iofst=iofst+itemp;
           //         Pad last octet with Zeros, if necessary,
           if ( (itemp%8) != 0) {
              left=8-(itemp%8);
              sbit(cpack,&zero,iofst,left);
              iofst=iofst+left;
           }
        }
        else {
           nbitsglen=0;
           for (i=0;i<ngroups;i++) glen[i]=0;
        }
        //
        //  For each group, pack data values
        //
        n=0;
        for (ng=0;ng<ngroups;ng++) {
           glength=glen[ng]+nglenref;
           if (ng == (ngroups-1) ) glength=nglenlast;
           grpwidth=gwidth[ng]+ngwidthref;
           if ( grpwidth != 0 ) {
              sbits(cpack,ifld+n,iofst,grpwidth,0,glength);
              iofst=iofst+(grpwidth*glength);
           }
           n=n+glength;
        }
        //         Pad last octet with Zeros, if necessary,
        if ( (iofst%8) != 0) {
           left=8-(iofst%8);
           sbit(cpack,&zero,iofst,left);
           iofst=iofst+left;
        }
        *lcpack=iofst/8;
        //
        free(ifld);
        free(gref);
        free(gwidth);
        free(glen);
      }
      else {          //   Constant field ( max = min )
        /* nbits=0; */
        *lcpack=0;
        nbitsgref=0;
        ngroups=0;
      }

//
//  Fill in ref value and number of bits in Template 5.2
//
      mkieee(&rmin,idrstmpl+0,1);   // ensure reference value is IEEE format
      idrstmpl[3]=nbitsgref;
      idrstmpl[4]=0;         // original data were reals
      idrstmpl[5]=1;         // general group splitting
      idrstmpl[6]=0;         // No internal missing values
      idrstmpl[7]=0;         // Primary missing value
      idrstmpl[8]=0;         // secondary missing value
      idrstmpl[9]=ngroups;          // Number of groups
      idrstmpl[10]=ngwidthref;       // reference for group widths
      idrstmpl[11]=nbitsgwidth;      // num bits used for group widths
      idrstmpl[12]=nglenref;         // Reference for group lengths
      idrstmpl[13]=1;                // length increment for group lengths
      idrstmpl[14]=nglenlast;        // True length of last group
      idrstmpl[15]=nbitsglen;        // num bits used for group lengths
      if (idrsnum == 3) {
         idrstmpl[17]=nbitsd/8;      // num bits used for extra spatial
                                     // differencing values
      }

}
