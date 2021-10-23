#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include "grib2.h"


void simpack(g2float *fld,g2int ndpts,g2int *idrstmpl,unsigned char *cpack,g2int *lcpack)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    simpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2002-11-06
//
// ABSTRACT: This subroutine packs up a data field using the simple
//   packing algorithm as defined in the GRIB2 documentation.  It
//   also fills in GRIB2 Data Representation Template 5.0 with the
//   appropriate values.
//
// PROGRAM HISTORY LOG:
// 2002-11-06  Gilbert
//
// USAGE:    CALL simpack(fld,ndpts,idrstmpl,cpack,lcpack)
//   INPUT ARGUMENT LIST:
//     fld[]    - Contains the data values to pack
//     ndpts    - The number of data values in array fld[]
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.0
//                [0] = Reference value - ignored on input
//                [1] = Binary Scale Factor
//                [2] = Decimal Scale Factor
//                [3] = Number of bits used to pack data, if value is
//                      > 0 and  <= 31.
//                      If this input value is 0 or outside above range
//                      then the num of bits is calculated based on given
//                      data and scale factors.
//                [4] = Original field type - currently ignored on input
//                      Data values assumed to be reals.
//
//   OUTPUT ARGUMENT LIST:
//     idrstmpl - Contains the array of values for Data Representation
//                Template 5.0
//                [0] = Reference value - set by simpack routine.
//                [1] = Binary Scale Factor - unchanged from input
//                [2] = Decimal Scale Factor - unchanged from input
//                [3] = Number of bits used to pack data, unchanged from
//                      input if value is between 0 and 31.
//                      If this input value is 0 or outside above range
//                      then the num of bits is calculated based on given
//                      data and scale factors.
//                [4] = Original field type - currently set = 0 on output.
//                      Data values assumed to be reals.
//     cpack    - The packed data field
//     lcpack   - length of packed field starting at cpack.
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
      g2int  *ifld;
      g2int  j,nbits,maxdif,nbittot,left;
      g2float  bscale,dscale,rmax,rmin,temp, ref, rmin_dscaled, rmax_dscaled;
      double maxnum;
      const g2float alog2=0.69314718f;       //  ln(2.0)

      bscale=(float)int_power(2.0,-idrstmpl[1]);
      dscale=(float)int_power(10.0,idrstmpl[2]);
      if (idrstmpl[3] <= 0 || idrstmpl[3] > 31)
         nbits=0;
      else
         nbits=idrstmpl[3];
//
//  Find max and min values in the data
//
      rmax=fld[0];
      rmin=fld[0];
      for (j=1;j<ndpts;j++) {
        if (fld[j] > rmax) rmax=fld[j];
        if (fld[j] < rmin) rmin=fld[j];
      }
      if( !(floor(rmin*dscale) >= -FLT_MAX && floor(rmin*dscale) <= FLT_MAX) )
      {
         fprintf(stderr,
                    "Scaled min value not representable on IEEE754 "
                    "single precision float\n");
        *lcpack = -1;
        return;
      }
      if( !(floor(rmax*dscale) >= -FLT_MAX && floor(rmax*dscale) <= FLT_MAX) )
      {
         fprintf(stderr,
                    "Scaled max value not representable on IEEE754 "
                    "single precision float\n");
        *lcpack = -1;
        return;
      }
      rmin_dscaled = rmin*dscale;
      rmax_dscaled = rmax*dscale;

      ifld=calloc(ndpts,sizeof(g2int));
      if( ifld == NULL )
      {
          fprintf(stderr, "Cannot allocate ifld in simpack()\n");
          *lcpack = -1;
          return;
      }

//
//  If max and min values are not equal, pack up field.
//  If they are equal, we have a constant field, and the reference
//  value (rmin) is the value for each point in the field and
//  set nbits to 0.
//
      if ( (rmax_dscaled - rmin_dscaled >= 1) ||
           (rmin != rmax && nbits!=0 && idrstmpl[1]==0) ) {
        int done = 0;
        //
        //  Determine which algorithm to use based on user-supplied
        //  binary scale factor and number of bits.
        //
        if (nbits==0 && idrstmpl[1]==0) {
           //
           //  No binary scaling and calculate minimum number of
           //  bits in which the data will fit.
           //
           if( dscale != 1.0 )
           {
              rmin_dscaled = (float)floor(rmin_dscaled);
           }
           if( (double)(rmax_dscaled - rmin_dscaled) > (double)INT_MAX )
           {
                nbits = 31;
           }
           else
           {
                temp=(float)(log(ceil(rmax_dscaled - rmin_dscaled))/alog2);
                nbits=(g2int)ceil(temp);
                //   scale data
                if( nbits > 31 )
                {
                    nbits = 31;
                }
                else
                {
                    done = 1;
                    for(j=0;j<ndpts;j++)
                        ifld[j]=(g2int)RINT(fld[j]*dscale -rmin_dscaled);
                    ref = rmin_dscaled;
                }
           }
        }

        if (!done && nbits!=0 && idrstmpl[1]==0) {
           //
           //  Use number of bits specified by user and
           //  adjust binary scaling factor to accommodate data.
           //
           if( dscale != 1.0 )
           {
              rmin_dscaled = (float)floor(rmin_dscaled);
           }
           maxnum=int_power(2.0,nbits)-1;
           temp=(float)(log(maxnum/(rmax_dscaled-rmin_dscaled))/alog2);
           idrstmpl[1]=(g2int)ceil(-1.0*temp);
           bscale=(float)int_power(2.0,-idrstmpl[1]);
           //   scale data
           for (j=0;j<ndpts;j++)
             ifld[j]=(g2int)RINT(((fld[j]*dscale)-rmin_dscaled)*bscale);
           ref=rmin_dscaled;
        }
        else if (nbits==0 && idrstmpl[1]!=0) {
           //
           //  Use binary scaling factor and calculate minimum number of
           //  bits in which the data will fit.
           //
           maxdif=(g2int)RINT((rmax_dscaled-rmin_dscaled)*bscale);
           temp=(float)(log((double)(maxdif+1))/alog2);
           nbits=(g2int)ceil(temp);
           //   scale data
           for (j=0;j<ndpts;j++)
             ifld[j]=(g2int)RINT(((fld[j]*dscale)-rmin_dscaled)*bscale);
           ref=rmin_dscaled;
        }
        else if (nbits!=0 && idrstmpl[1]!=0) {
           //
           //  Use binary scaling factor and use minimum number of
           //  bits specified by user.   Dangerous - may loose
           //  information if binary scale factor and nbits not set
           //  properly by user.
           //
           //   scale data
           for (j=0;j<ndpts;j++)
             ifld[j]=(g2int)RINT(((fld[j]*dscale)-rmin_dscaled)*bscale);
           ref=rmin_dscaled;
        }
        //
        //  Pack data, Pad last octet with Zeros, if necessary,
        //  and calculate the length of the packed data in bytes
        //
        sbits(cpack,ifld+0,0,nbits,0,ndpts);
        nbittot=nbits*ndpts;
        left=8-(nbittot%8);
        if (left != 8) {
          sbit(cpack,&zero,nbittot,left);   // Pad with zeros to fill Octet
          nbittot=nbittot+left;
        }
        *lcpack=nbittot/8;
      }
      else {
        /* Force E and D to 0 to avoid compatibility issues */
        idrstmpl[1]=0;
        idrstmpl[2]=0;
        if( dscale != 1.0 )
        {
          ref = (float)floor(rmin * dscale) / dscale;
        }
        else
        {
          ref = rmin;
        }
        nbits=0;
        *lcpack=0;
      }

//
//  Fill in ref value and number of bits in Template 5.0
//
      //printf("SAGmkieee %f\n",ref);
      mkieee(&ref,idrstmpl+0,1);   // ensure reference value is IEEE format
      //printf("SAGmkieee %ld\n",idrstmpl[0]);
      idrstmpl[3]=nbits;
      idrstmpl[4]=0;         // original data were reals

      free(ifld);
}
