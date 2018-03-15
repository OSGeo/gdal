#include "grib2.h"

void rdieee(g2int *rieee,g2float *a,g2int num)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    rdieee
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-25
//
// ABSTRACT: This subroutine reads a list of real values in
//   32-bit IEEE floating point format.
//
// PROGRAM HISTORY LOG:
// 2002-10-25  Gilbert
//
// USAGE:    void rdieee(g2int *rieee,g2float *a,g2int num)
//   INPUT ARGUMENT LIST:
//     rieee    - g2int array of floating point values in 32-bit IEEE format.
//     num      - Number of floating point values to convert.
//
//   OUTPUT ARGUMENT LIST:
//     a        - float array of real values.  a must be allocated with at
//                least 4*num bytes of memory before calling this function.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{

      g2int  j;
      g2int  isign,iexp,imant;

      g2float  sign,temp;
      static const double two23 = 1.1920928955078125e-07; // pow(2,-23)
      static const double two126 = 1.1754943508222875e-38; // pow(2,-126)


      g2intu msk1=0x80000000;        // 10000000000000000000000000000000 binary
      g2int msk2=0x7F800000;         // 01111111100000000000000000000000 binary
      g2int msk3=0x007FFFFF;         // 00000000011111111111111111111111 binary

      for (j=0;j<num;j++) {
//
//  Extract sign bit, exponent, and mantissa
//
        isign=(rieee[j]&msk1)>>31;
        iexp=(rieee[j]&msk2)>>23;
        imant=(rieee[j]&msk3);
        //printf("SAGieee= %ld %ld %ld\n",isign,iexp,imant);

        sign=1.0;
        if (isign == 1) sign=-1.0;

        if ( (iexp > 0) && (iexp < 255) ) {
          temp=(g2float)int_power(2.0,(iexp-127));
          a[j]=(float)(sign*temp*(1.0+(two23*(g2float)imant)));
        }
        else if ( iexp == 0 ) {
          if ( imant != 0 )
            a[j]=(float)(sign*two126*two23*(g2float)imant);
          else
            a[j]=(float)(sign*0.0);

        }
        else if ( iexp == 255 )
           a[j]=(float)(sign*(1E+37));


      }

}
