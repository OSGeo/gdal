#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

static float DoubleToFloatClamp(double val) {
   if (val >= FLT_MAX) return FLT_MAX;
   if (val <= -FLT_MAX) return -FLT_MAX;
   return (float)val;
}

int comunpack(unsigned char *cpack,g2int cpack_length,g2int lensec,g2int idrsnum,g2int *idrstmpl,g2int ndpts,g2float *fld)
////$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    comunpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2002-10-29
//
// ABSTRACT: This subroutine unpacks a data field that was packed using a
//   complex packing algorithm as defined in the GRIB2 documentation,
//   using info from the GRIB2 Data Representation Template 5.2 or 5.3.
//   Supports GRIB2 complex packing templates with or without
//   spatial differences (i.e. DRTs 5.2 and 5.3).
//
// PROGRAM HISTORY LOG:
// 2002-10-29  Gilbert
// 2004-12-16  Gilbert  -  Added test ( provided by Arthur Taylor/MDL )
//                         to verify that group widths and lengths are
//                         consistent with section length.
//
// USAGE:    int comunpack(unsigned char *cpack,g2int lensec,g2int idrsnum,
//                         g2int *idrstmpl, g2int ndpts,g2float *fld)
//   INPUT ARGUMENT LIST:
//     cpack    - pointer to the packed data field.
//     lensec   - length of section 7 (used for error checking).
//     idrsnum  - Data Representation Template number 5.N
//                Must equal 2 or 3.
//     idrstmpl - pointer to the array of values for Data Representation
//                Template 5.2 or 5.3
//     ndpts    - The number of data values to unpack
//
//   OUTPUT ARGUMENT LIST:
//     fld      - Contains the unpacked data values.  fld must be allocated
//                with at least ndpts*sizeof(g2float) bytes before
//                calling this routine.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$//
{

      g2int nbitsd=0,isign;
      g2int j,iofst,ival1,ival2,minsd,itemp,l,k,n,non=0;
      g2int *ifld=NULL,*ifldmiss=NULL;
      g2int *gref=NULL,*gwidth=NULL,*glen=NULL;
      g2int itype,ngroups,nbitsgref,nbitsgwidth,nbitsglen;
      g2int msng1,msng2;
      g2float ref,bscale,dscale,rmiss1,rmiss2;
      g2int totBit, totLen;
      g2int errorOccurred = 0;

      //printf('IDRSTMPL: ',(idrstmpl(j),j=1,16)
      rdieee(idrstmpl+0,&ref,1);
//      printf("SAGTref: %f\n",ref);
      bscale = DoubleToFloatClamp(int_power(2.0,idrstmpl[1]));
      dscale = DoubleToFloatClamp(int_power(10.0,-idrstmpl[2]));
      nbitsgref = idrstmpl[3];
      itype = idrstmpl[4];
      ngroups = idrstmpl[9];
      nbitsgwidth = idrstmpl[11];
      nbitsglen = idrstmpl[15];
      if (idrsnum == 3)
         nbitsd=idrstmpl[17]*8;

      //   Constant field

      if (ngroups == 0) {
         for (j=0;j<ndpts;j++) fld[j]=ref;
         return(0);
      }

      /* To avoid excessive memory allocations. Not completely sure */
      /* if this test is appropriate (the 10 and 2 are arbitrary), */
      /* but it doesn't seem to make sense to have ngroups much larger than */
      /* ndpts */
      if( ngroups < 0 || ngroups - 10 > ndpts / 2 ) {
          return -1;
      }

      /* Early test in particular case for more general test belows */
      /* "Test to see if the group widths and lengths are consistent with number of */
      /*  values, and length of section 7. */
      if( idrstmpl[12] < 0 || idrstmpl[14] < 0 || idrstmpl[14] > ndpts )
          return -1;
      if( nbitsglen == 0 &&
          ((ngroups > 1 && idrstmpl[12] != (ndpts - idrstmpl[14]) / (ngroups - 1)) ||
           idrstmpl[12] * (ngroups-1) + idrstmpl[14] != ndpts) )
      {
          return -1;
      }

      iofst=0;
      ifld=(g2int *)calloc(ndpts,sizeof(g2int));
      //printf("ALLOC ifld: %d %x\n",(int)ndpts,ifld);
      gref=(g2int *)calloc(ngroups,sizeof(g2int));
      //printf("ALLOC gref: %d %x\n",(int)ngroups,gref);
      gwidth=(g2int *)calloc(ngroups,sizeof(g2int));
      //printf("ALLOC gwidth: %d %x\n",(int)ngroups,gwidth);
      if( ifld == NULL || gref == NULL || gwidth == NULL )
      {
          free(ifld);
          free(gref);
          free(gwidth);
          return -1;
      }
//
//  Get missing values, if supplied
//
      if ( idrstmpl[6] == 1 ) {
         if (itype == 0)
            rdieee(idrstmpl+7,&rmiss1,1);
         else
            rmiss1=(g2float)idrstmpl[7];
      }
      if ( idrstmpl[6] == 2 ) {
         if (itype == 0) {
            rdieee(idrstmpl+7,&rmiss1,1);
            rdieee(idrstmpl+8,&rmiss2,1);
         }
         else {
            rmiss1=(g2float)idrstmpl[7];
            rmiss2=(g2float)idrstmpl[8];
         }
      }

      //printf("RMISSs: %f %f %f \n",rmiss1,rmiss2,ref);
//
//  Extract spatial differencing values, if using DRS Template 5.3
//
      if (idrsnum == 3) {
         if (nbitsd != 0) {
// one mistake here should be unsigned int
              gbit(cpack,&ival1,iofst,nbitsd);
              iofst=iofst+nbitsd;
//              gbit(cpack,&isign,iofst,1);
//              iofst=iofst+1
//              gbit(cpack,&ival1,iofst,nbitsd-1);
//              iofst=iofst+nbitsd-1;
//              if (isign == 1) ival1=-ival1;
              if (idrstmpl[16] == 2) {
// one mistake here should be unsigned int
                 gbit(cpack,&ival2,iofst,nbitsd);
                 iofst=iofst+nbitsd;
//                 gbit(cpack,&isign,iofst,1);
//                 iofst=iofst+1;
//                 gbit(cpack,&ival2,iofst,nbitsd-1);
//                 iofst=iofst+nbitsd-1;
//                 if (isign == 1) ival2=-ival2;
              }
              gbit(cpack,&isign,iofst,1);
              iofst=iofst+1;
              gbit(cpack,&minsd,iofst,nbitsd-1);
              iofst=iofst+nbitsd-1;
              if (isign == 1 && minsd != INT_MIN) minsd=-minsd;
         }
         else {
              ival1=0;
              ival2=0;
              minsd=0;
         }
       //printf("SDu %ld %ld %ld %ld \n",ival1,ival2,minsd,nbitsd);
      }
//
//  Extract Each Group's reference value
//
      //printf("SAG1: %ld %ld %ld \n",nbitsgref,ngroups,iofst);
      if (nbitsgref != 0) {
         if( gbits(cpack,cpack_length,gref+0,iofst,nbitsgref,0,ngroups) != 0 )
         {
             free(ifld);
             free(gwidth);
             free(gref);
             return -1;
         }
         itemp=nbitsgref*ngroups;
         iofst=iofst+itemp;
         if (itemp%8 != 0) iofst=iofst+(8-(itemp%8));
      }
#if 0
      else {
         for (j=0;j<ngroups;j++)
              gref[j]=0;
      }
#endif

//
//  Extract Each Group's bit width
//
      //printf("SAG2: %ld %ld %ld %ld \n",nbitsgwidth,ngroups,iofst,idrstmpl[10]);
      if (nbitsgwidth != 0) {
         if( gbits(cpack,cpack_length,gwidth+0,iofst,nbitsgwidth,0,ngroups) != 0 )
         {
             free(ifld);
             free(gwidth);
             free(gref);
             return -1;
         }
         itemp=nbitsgwidth*ngroups;
         iofst=iofst+itemp;
         if (itemp%8 != 0) iofst=iofst+(8-(itemp%8));
      }
#if 0
      else {
         for (j=0;j<ngroups;j++)
                gwidth[j]=0;
      }
#endif

      for (j=0;j<ngroups;j++)
      {
          if( gwidth[j] > INT_MAX - idrstmpl[10] )
          {
             free(ifld);
             free(gwidth);
             free(gref);
             return -1;
          }
          gwidth[j]=gwidth[j]+idrstmpl[10];
      }

//
//  Extract Each Group's length (number of values in each group)
//
      glen=(g2int *)calloc(ngroups,sizeof(g2int));
      if( glen == NULL )
      {
        free(ifld);
        free(gwidth);
        free(gref);
        return -1;
      }
      //printf("ALLOC glen: %d %x\n",(int)ngroups,glen);
      //printf("SAG3: %ld %ld %ld %ld %ld \n",nbitsglen,ngroups,iofst,idrstmpl[13],idrstmpl[12]);
      if (nbitsglen != 0) {
         if( gbits(cpack,cpack_length,glen,iofst,nbitsglen,0,ngroups) != 0 )
         {
            free(ifld);
            free(gwidth);
            free(glen);
            free(gref);
             return -1;
         }
         itemp=nbitsglen*ngroups;
         iofst=iofst+itemp;
         if (itemp%8 != 0) iofst=iofst+(8-(itemp%8));
      }
#if 0
      else {
         for (j=0;j<ngroups;j++)
              glen[j]=0;
      }
#endif

      for (j=0;j<ngroups;j++)
      {
           if( glen[j] < 0 ||
               (idrstmpl[13] != 0 && glen[j] > INT_MAX / idrstmpl[13]) ||
               glen[j] *  idrstmpl[13] > INT_MAX - idrstmpl[12] )
           {
                free(ifld);
                free(gwidth);
                free(glen);
                free(gref);
                return -1;
           }
           glen[j]=(glen[j]*idrstmpl[13])+idrstmpl[12];
      }
      glen[ngroups-1]=idrstmpl[14];
//
//  Test to see if the group widths and lengths are consistent with number of
//  values, and length of section 7.
//
      totBit = 0;
      totLen = 0;
      for (j=0;j<ngroups;j++) {
        g2int width_mult_len;
        if( gwidth[j] < 0 || glen[j] < 0 ||
            (gwidth[j] > 0 && glen[j] > INT_MAX / gwidth[j]) )
        {
            errorOccurred = 1;
            break;
        }
        width_mult_len = gwidth[j]*glen[j];
        if( totBit > INT_MAX - width_mult_len )
        {
            errorOccurred = 1;
            break;
        }
        totBit += width_mult_len;
        if( totLen > INT_MAX - glen[j] )
        {
            errorOccurred = 1;
            break;
        }
        totLen += glen[j];
      }
      if (errorOccurred || totLen != ndpts || totBit / 8. > lensec) {
        free(ifld);
        free(gwidth);
        free(glen);
        free(gref);
        return 1;
      }
//
//  For each group, unpack data values
//
      if ( idrstmpl[6] == 0 ) {        // no missing values
         n=0;
         for (j=0;j<ngroups;j++) {
           if (gwidth[j] != 0) {
             if( gbits(cpack,cpack_length,ifld+n,iofst,gwidth[j],0,glen[j]) != 0 )
             {
                 free(ifld);
                 free(gwidth);
                 free(glen);
                 free(gref);
                 return -1;
             }
             for (k=0;k<glen[j];k++) {
               ifld[n]=ifld[n]+gref[j];
               n=n+1;
             }
           }
           else {
             for (l=n;l<n+glen[j];l++) ifld[l]=gref[j];
             n=n+glen[j];
           }
           iofst=iofst+(gwidth[j]*glen[j]);
         }
      }
      else if ( idrstmpl[6]==1 || idrstmpl[6]==2 ) {
         // missing values included
         ifldmiss=(g2int *)malloc(ndpts*sizeof(g2int));
         //printf("ALLOC ifldmiss: %d %x\n",(int)ndpts,ifldmiss);
         //for (j=0;j<ndpts;j++) ifldmiss[j]=0;
         n=0;
         non=0;
         for (j=0;j<ngroups;j++) {
           //printf(" SAGNGP %d %d %d %d\n",j,gwidth[j],glen[j],gref[j]);
           if (gwidth[j] != 0) {
             msng1=(g2int)int_power(2.0,gwidth[j])-1;
             msng2=msng1-1;
             if( gbits(cpack,cpack_length,ifld+n,iofst,gwidth[j],0,glen[j]) != 0 )
             {
                 free(ifld);
                 free(gwidth);
                 free(glen);
                 free(gref);
                 free(ifldmiss);
                 return -1;
             }
             iofst=iofst+(gwidth[j]*glen[j]);
             for (k=0;k<glen[j];k++) {
               if (ifld[n] == msng1) {
                  ifldmiss[n]=1;
                  //ifld[n]=0;
               }
               else if (idrstmpl[6]==2 && ifld[n]==msng2) {
                  ifldmiss[n]=2;
                  //ifld[n]=0;
               }
               else {
                  ifldmiss[n]=0;
                  ifld[non++]=ifld[n]+gref[j];
               }
               n++;
             }
           }
           else {
             msng1=(g2int)int_power(2.0,nbitsgref)-1;
             msng2=msng1-1;
             if (gref[j] == msng1) {
                for (l=n;l<n+glen[j];l++) ifldmiss[l]=1;
             }
             else if (idrstmpl[6]==2 && gref[j]==msng2) {
                for (l=n;l<n+glen[j];l++) ifldmiss[l]=2;
             }
             else {
                for (l=n;l<n+glen[j];l++) ifldmiss[l]=0;
                for (l=non;l<non+glen[j];l++) ifld[l]=gref[j];
                non += glen[j];
             }
             n=n+glen[j];
           }
         }
      }

      if ( gref != 0 ) free(gref);
      if ( gwidth != 0 ) free(gwidth);
      if ( glen != 0 ) free(glen);
//
//  If using spatial differences, add overall min value, and
//  sum up recursively
//
      //printf("SAGod: %ld %ld\n",idrsnum,idrstmpl[16]);
      if (idrsnum == 3) {         // spatial differencing
         if (idrstmpl[16] == 1) {      // first order
            ifld[0]=ival1;
            if ( idrstmpl[6] == 0 ) itemp=ndpts;        // no missing values
            else  itemp=non;
            for (n=1;n<itemp;n++) {
               if( (minsd > 0 && ifld[n] > INT_MAX - minsd) ||
                   (minsd < 0 && ifld[n] < INT_MIN - minsd) )
               {
                   free(ifldmiss);
                   free(ifld);
                   return -1;
               }
               ifld[n]=ifld[n]+minsd;
               if( (ifld[n-1] > 0 && ifld[n] > INT_MAX - ifld[n-1]) ||
                   (ifld[n-1] < 0 && ifld[n] < INT_MIN - ifld[n-1]) )
               {
                   free(ifldmiss);
                   free(ifld);
                   return -1;
               }
               ifld[n]=ifld[n]+ifld[n-1];
            }
         }
         else if (idrstmpl[16] == 2) {    // second order
            ifld[0]=ival1;
            ifld[1]=ival2;
            if ( idrstmpl[6] == 0 ) itemp=ndpts;        // no missing values
            else  itemp=non;
            for (n=2;n<itemp;n++) {
               /* Lazy way of detecting int overflows: operate on double */
               double tmp = (double)ifld[n]+(double)minsd+(2.0 * ifld[n-1])-ifld[n-2];
               if( tmp > INT_MAX || tmp < INT_MIN )
               {
                   free(ifldmiss);
                   free(ifld);
                   return -1;
               }
               ifld[n]=(int)tmp;
            }
         }
      }
//
//  Scale data back to original form
//
      //printf("SAGT: %f %f %f\n",ref,bscale,dscale);
      if ( idrstmpl[6] == 0 ) {        // no missing values
         for (n=0;n<ndpts;n++) {
            fld[n]=(((g2float)ifld[n]*bscale)+ref)*dscale;
         }
      }
      else if ( idrstmpl[6]==1 || idrstmpl[6]==2 ) {
         // missing values included
         non=0;
         for (n=0;n<ndpts;n++) {
            if ( ifldmiss[n] == 0 ) {
               fld[n]=(((g2float)ifld[non++]*bscale)+ref)*dscale;
               //printf(" SAG %d %f %d %f %f %f\n",n,fld[n],ifld[non-1],bscale,ref,dscale);
            }
            else if ( ifldmiss[n] == 1 )
               fld[n]=rmiss1;
            else if ( ifldmiss[n] == 2 )
               fld[n]=rmiss2;
         }
         if ( ifldmiss != 0 ) free(ifldmiss);
      }

      if ( ifld != 0 ) free(ifld);

      return(0);

}
