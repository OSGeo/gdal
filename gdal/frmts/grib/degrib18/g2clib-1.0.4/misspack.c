#include <stdlib.h>
#include <math.h>
#include "grib2.h"

void misspack(g2float *fld,g2int ndpts,g2int idrsnum,g2int *idrstmpl,
              unsigned char *cpack, g2int *lcpack)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    misspack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2000-06-21
//
// ABSTRACT: This subroutine packs up a data field using a complex
//   packing algorithm as defined in the GRIB2 documention.  It
//   supports GRIB2 complex packing templates with or without
//   spatial differences (i.e. DRTs 5.2 and 5.3).
//   It also fills in GRIB2 Data Representation Template 5.2 or 5.3 
//   with the appropriate values.
//   This version assumes that Missing Value Management is being used and that
//   1 or 2 missing values appear in the data.
//
// PROGRAM HISTORY LOG:
// 2000-06-21  Gilbert
//
// USAGE:    misspack(g2float *fld,g2int ndpts,g2int idrsnum,g2int *idrstmpl,
//                    unsigned char *cpack, g2int *lcpack)
//   INPUT ARGUMENT LIST:
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
//   OUTPUT ARGUMENT LIST: 
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.3
//                [0] = Reference value - set by misspack routine.
//                [1] = Binary Scale Factor - unchanged from input
//                [2] = Decimal Scale Factor - unchanged from input
//                    .
//                    .
//     cpack    - The packed data field (character*1 array)
//     *lcpack   - length of packed field cpack().
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{

      g2int  *ifld, *ifldmiss, *jfld;
      g2int  *jmin, *jmax, *lbit;
      static g2int zero=0;
      g2int  *gref, *gwidth, *glen;
      g2int  glength, grpwidth;
      g2int  i, n, iofst, imin, ival1, ival2, isd, minsd, nbitsd = 0;
      g2int  nbitsgref, left, iwmax, ngwidthref, nbitsgwidth, ilmax;
      g2int  nglenref, nglenlast, nbitsglen /* , ij */;
      g2int  j, missopt, nonmiss, itemp, maxorig, nbitorig, miss1, miss2;
      g2int  ngroups, ng, num0, num1, num2;
      g2int  imax, lg, mtemp, ier, igmax;
      g2int  kfildo, minpk, inc, maxgrps, ibit, jbit, kbit, novref, lbitref;
      g2float  rmissp, rmisss, bscale, dscale, rmin, temp;
      static g2int simple_alg = 0;
      static g2float alog2=0.69314718;       //  ln(2.0)
      static g2int one=1;

      bscale=int_power(2.0,-idrstmpl[1]);
      dscale=int_power(10.0,idrstmpl[2]);
      missopt=idrstmpl[6];
      if ( missopt != 1 && missopt != 2 ) {
         printf("misspack: Unrecognized option.\n");
         *lcpack=-1;
         return;
      }
      else {    //  Get missing values
         rdieee(idrstmpl+7,&rmissp,1);
         if (missopt == 2) rdieee(idrstmpl+8,&rmisss,1);
      }
//
//  Find min value of non-missing values in the data,
//  AND set up missing value mapping of the field.
//
      ifldmiss = calloc(ndpts,sizeof(g2int));
      rmin=1E+37;
      if ( missopt ==  1 ) {        // Primary missing value only
         for ( j=0; j<ndpts; j++) {
           if (fld[j] == rmissp) {
              ifldmiss[j]=1;
           }
           else {
              ifldmiss[j]=0;
              if (fld[j] < rmin) rmin=fld[j];
           }
         }
      }
      if ( missopt ==  2 ) {        // Primary and secondary missing values
         for ( j=0; j<ndpts; j++ ) {
           if (fld[j] == rmissp) {
              ifldmiss[j]=1;
           }
           else if (fld[j] == rmisss) {
              ifldmiss[j]=2;
           }
           else {
              ifldmiss[j]=0;
              if (fld[j] < rmin) rmin=fld[j];
           }
         }
      }
//
//  Allocate work arrays:
//  Note: -ifldmiss[j],j=0,ndpts-1 is a map of original field indicating 
//         which of the original data values
//         are primary missing (1), sencondary missing (2) or non-missing (0).
//        -jfld[j],j=0,nonmiss-1 is a subarray of just the non-missing values 
//         from the original field.
//
      //if (rmin != rmax) {
        iofst=0;
        ifld = calloc(ndpts,sizeof(g2int));
        jfld = calloc(ndpts,sizeof(g2int));
        gref = calloc(ndpts,sizeof(g2int));
        gwidth = calloc(ndpts,sizeof(g2int));
        glen = calloc(ndpts,sizeof(g2int));
        //
        //  Scale original data
        //
        nonmiss=0;
        if (idrstmpl[1] == 0) {        //  No binary scaling
           imin=(g2int)RINT(rmin*dscale);
           //imax=(g2int)rint(rmax*dscale);
           rmin=(g2float)imin;
           for ( j=0; j<ndpts; j++) {
              if (ifldmiss[j] == 0) {
                jfld[nonmiss]=(g2int)RINT(fld[j]*dscale)-imin;
                nonmiss++;
              }
           }
        }
        else {                             //  Use binary scaling factor
           rmin=rmin*dscale;
           //rmax=rmax*dscale;
           for ( j=0; j<ndpts; j++ ) {
              if (ifldmiss[j] == 0) {
                jfld[nonmiss]=(g2int)RINT(((fld[j]*dscale)-rmin)*bscale);
                nonmiss++;
              }
           }
        }
        //
        //  Calculate Spatial differences, if using DRS Template 5.3
        //
        if (idrsnum == 3) {        // spatial differences
           if (idrstmpl[16]!=1 && idrstmpl[16]!=2) idrstmpl[16]=2;
           if (idrstmpl[16] == 1) {      // first order
              ival1=jfld[0];
              for ( j=nonmiss-1; j>0; j--)
                 jfld[j]=jfld[j]-jfld[j-1];
              jfld[0]=0;
           }
           else if (idrstmpl[16] == 2) {      // second order
              ival1=jfld[0];
              ival2=jfld[1];
              for ( j=nonmiss-1; j>1; j--)
                 jfld[j]=jfld[j]-(2*jfld[j-1])+jfld[j-2];
              jfld[0]=0;
              jfld[1]=0;
           }
           //
           //  subtract min value from spatial diff field
           //
           isd=idrstmpl[16];
           minsd=jfld[isd];
           for ( j=isd; j<nonmiss; j++ ) if ( jfld[j] < minsd ) minsd=jfld[j];
           for ( j=isd; j<nonmiss; j++ ) jfld[j]=jfld[j]-minsd;
           //
           //   find num of bits need to store minsd and add 1 extra bit
           //   to indicate sign
           //
           temp=log((double)(abs(minsd)+1))/alog2;
           nbitsd=(g2int)ceil(temp)+1;
           //
           //   find num of bits need to store ifld[0] ( and ifld[1]
           //   if using 2nd order differencing )
           //
           maxorig=ival1;
           if (idrstmpl[16]==2 && ival2>ival1) maxorig=ival2;
           temp=log((double)(maxorig+1))/alog2;
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
         //print *,'SDp ',ival1,ival2,minsd,nbitsd
        }       //  end of spatial diff section
        //
        //  Expand non-missing data values to original grid.
        //
        miss1=jfld[0];
        for ( j=0; j<nonmiss; j++) if (jfld[j] < miss1) miss1 = jfld[j];
        miss1--;
        miss2=miss1-1;
        n=0;
        for ( j=0; j<ndpts; j++) {
           if ( ifldmiss[j] == 0 ) {
              ifld[j]=jfld[n];
              n++;
           }
           else if ( ifldmiss[j] == 1 ) {
              ifld[j]=miss1;
           }
           else if ( ifldmiss[j] == 2 ) {
              ifld[j]=miss2;
           }
        }
        //
        //   Determine Groups to be used.
        //
        if ( simple_alg == 1 ) {
           //  set group length to 10 :  calculate number of groups
           //  and length of last group
           ngroups=ndpts/10;
           for (j=0;j<ngroups;j++) glen[j]=10;
           itemp=ndpts%10;
           if (itemp != 0) {
              ngroups++;
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
           pack_gp(&kfildo,ifld,&ndpts,&missopt,&minpk,&inc,&miss1,&miss2,
                        jmin,jmax,lbit,glen,&maxgrps,&ngroups,&ibit,&jbit,
                        &kbit,&novref,&lbitref,&ier);
           //printf("SAGier = %d %d %d %d %d %d\n",ier,ibit,jbit,kbit,novref,lbitref);
           for ( ng=0; ng<ngroups; ng++) glen[ng]=glen[ng]+novref;
           free(jmin);
           free(jmax);
           free(lbit);
        }
        //  
        //  For each group, find the group's reference value (min)
        //  and the number of bits needed to hold the remaining values
        //
        n=0;
        for ( ng=0; ng<ngroups; ng++) {
           //  how many of each type?
           num0=num1=num2=0;
           for (j=n; j<n+glen[ng]; j++) {
               if (ifldmiss[j] == 0 ) num0++;
               if (ifldmiss[j] == 1 ) num1++;
               if (ifldmiss[j] == 2 ) num2++;
           }
           if ( num0 == 0 ) {      // all missing values
              if ( num1 == 0 ) {       // all secondary missing
                gref[ng]=-2;
                gwidth[ng]=0;
              }
              else if ( num2 == 0 ) {       // all primary missing
                gref[ng]=-1;
                gwidth[ng]=0;
              }
              else {                          // both primary and secondary
                gref[ng]=0;
                gwidth[ng]=1;
              }
           }
           else {                      // contains some non-missing data
             //    find max and min values of group
             gref[ng]=2147483647;
             imax=-2147483647;
             j=n;
             for ( lg=0; lg<glen[ng]; lg++ ) {
                if ( ifldmiss[j] == 0 ) {
                  if (ifld[j] < gref[ng]) gref[ng]=ifld[j];
                  if (ifld[j] > imax) imax=ifld[j]; 
                }
                j++;
             }
             if (missopt == 1) imax=imax+1;
             if (missopt == 2) imax=imax+2;
             //   calc num of bits needed to hold data
             if ( gref[ng] != imax ) {
                temp=log((double)(imax-gref[ng]+1))/alog2;
                gwidth[ng]=(g2int)ceil(temp);
             }
             else {
                gwidth[ng]=0;
             }
           }
           //   Subtract min from data
           j=n;
           mtemp=(g2int)int_power(2.,gwidth[ng]);
           for ( lg=0; lg<glen[ng]; lg++ ) {
              if (ifldmiss[j] == 0)            // non-missing
                 ifld[j]=ifld[j]-gref[ng];
              else if (ifldmiss[j] == 1)         // primary missing
                 ifld[j]=mtemp-1;
              else if (ifldmiss[j] == 2)         // secondary missing
                 ifld[j]=mtemp-2;
              
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
        //printf(" GREFS: ");
        //for (j=0;j<ngroups;j++) printf(" %d",gref[j]); printf("\n");
        igmax=gref[0];
        for (j=1;j<ngroups;j++) if (gref[j] > igmax) igmax=gref[j];
        if (missopt == 1) igmax=igmax+1;
        if (missopt == 2) igmax=igmax+2;
        if (igmax != 0) {
           temp=log((double)(igmax+1))/alog2;
           nbitsgref=(g2int)ceil(temp);
           // reset the ref values of any "missing only" groups.
           mtemp=(g2int)int_power(2.,nbitsgref);
           for ( j=0; j<ngroups; j++ ) {
               if (gref[j] == -1) gref[j]=mtemp-1;
               if (gref[j] == -2) gref[j]=mtemp-2;
           }
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
        else {
           nbitsgref=0;
        }
        //
        //  Find max/min of the group widths and calc num of bits needed
        //  to pack each groups width value, then
        //  pack up group width values
        //
        //write(77,*)'GWIDTHS: ',(gwidth(j),j=1,ngroups)
        iwmax=gwidth[0];
        ngwidthref=gwidth[0];
        for (j=1;j<ngroups;j++) {
           if (gwidth[j] > iwmax) iwmax=gwidth[j];
           if (gwidth[j] < ngwidthref) ngwidthref=gwidth[j];
        }
        if (iwmax != ngwidthref) {
           temp=log((double)(iwmax-ngwidthref+1))/alog2;
           nbitsgwidth=(g2int)ceil(temp);
           for ( i=0; i<ngroups; i++) gwidth[i]=gwidth[i]-ngwidthref;
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
        //printf(" GLENS: ");
        //for (j=0;j<ngroups;j++) printf(" %d",glen[j]); printf("\n");
        ilmax=glen[0];
        nglenref=glen[0];
        for (j=1;j<ngroups-1;j++) {
           if (glen[j] > ilmax) ilmax=glen[j];
           if (glen[j] < nglenref) nglenref=glen[j];
        }
        nglenlast=glen[ngroups-1];
        if (ilmax != nglenref) {
           temp=log((double)(ilmax-nglenref+1))/alog2;
           nbitsglen=(g2int)ceil(temp);
           for ( i=0; i<ngroups-1; i++) glen[i]=glen[i]-nglenref;
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
        //write(77,*)'IFLDS: ',(ifld(j),j=1,ndpts)
        n=0;
        // ij=0;
        for ( ng=0; ng<ngroups; ng++) {
           glength=glen[ng]+nglenref;
           if (ng == (ngroups-1) ) glength=nglenlast;
           grpwidth=gwidth[ng]+ngwidthref;
       //write(77,*)'NGP ',ng,grpwidth,glength,gref(ng)
           if ( grpwidth != 0 ) {
              sbits(cpack,ifld+n,iofst,grpwidth,0,glength);
              iofst=iofst+(grpwidth*glength);
           }
       //  do kk=1,glength
       //     ij=ij+1
       //write(77,*)'SAG ',ij,fld(ij),ifld(ij),gref(ng),bscale,rmin,dscale
       //  enddo
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
        if ( ifld != 0 ) free(ifld);
        if ( jfld != 0 ) free(jfld);
        if ( ifldmiss != 0 ) free(ifldmiss);
        if ( gref != 0 ) free(gref);
        if ( gwidth != 0 ) free(gwidth);
        if ( glen != 0 ) free(glen);
      //}
      //else {          //   Constant field ( max = min )
      //  nbits=0;
      //  *lcpack=0;
      //  nbitsgref=0;
      //  ngroups=0;
      //}

//
//  Fill in ref value and number of bits in Template 5.2
//
      mkieee(&rmin,idrstmpl+0,1);   // ensure reference value is IEEE format
      idrstmpl[3]=nbitsgref;
      idrstmpl[4]=0;         // original data were reals
      idrstmpl[5]=1;         // general group splitting
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
