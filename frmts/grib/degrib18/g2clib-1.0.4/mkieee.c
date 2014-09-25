#include <stdlib.h>
#include <math.h>
#include "grib2.h"


void mkieee(g2float *a,g2int *rieee,g2int num)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    mkieee 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-29
//
// ABSTRACT: This subroutine stores a list of real values in 
//   32-bit IEEE floating point format.
//
// PROGRAM HISTORY LOG:
// 2002-10-29  Gilbert
//
// USAGE:    mkieee(g2float *a,g2int *rieee,g2int num);
//   INPUT ARGUMENT LIST:
//     a        - Input array of floating point values.
//     num      - Number of floating point values to convert.
//
//   OUTPUT ARGUMENT LIST:      
//     rieee    - Output array of data values in 32-bit IEEE format
//                stored in g2int integer array.  rieee must be allocated
//                with at least 4*num bytes of memory before calling this
//                function.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{

      g2int  j,n,ieee,iexp,imant;
      double  /* alog2, */ atemp;

      static double  two23,two126;
      static g2int test=0;
    //g2intu msk1=0x80000000;        // 10000000000000000000000000000000 binary
    //g2int msk2=0x7F800000;         // 01111111100000000000000000000000 binary
    //g2int msk3=0x007FFFFF;         // 00000000011111111111111111111111 binary

      if ( test == 0 ) {
         two23=(double)int_power(2.0,23);
         two126=(double)int_power(2.0,126);
         test=1;
      }

      // alog2=0.69314718;       //  ln(2.0)

      for (j=0;j<num;j++) {
      
        ieee=0;

        if (a[j] == 0.0) {
          rieee[j]=ieee;
          continue;
        }
        
//
//  Set Sign bit (bit 31 - leftmost bit)
//
        if (a[j] < 0.0) {
          ieee= 1 << 31;
          atemp=-1.0*a[j];
        }
        else {
          ieee= 0 << 31;
          atemp=a[j];
        }
        //printf("sign %ld %x \n",ieee,ieee);
//
//  Determine exponent n with base 2
//
        if ( atemp >= 1.0 ) {
           n = 0;
           while ( int_power(2.0,n+1) <= atemp ) {
              n++;
           }
        }
        else {
           n = -1;
           while ( int_power(2.0,n) > atemp ) {
              n--;
           }
        }
        //n=(g2int)floor(log(atemp)/alog2);
        iexp=n+127;
        if (n >  127) iexp=255;     // overflow
        if (n < -127) iexp=0;
        //printf("exp %ld %ld \n",iexp,n);
        //      set exponent bits ( bits 30-23 )
        ieee = ieee | ( iexp << 23 );
//
//  Determine Mantissa
// 
        if (iexp != 255) {
          if (iexp != 0) 
            atemp=(atemp/int_power(2.0,n))-1.0;
          else
            atemp=atemp*two126;
          imant=(g2int)RINT(atemp*two23);
        }
        else {
          imant=0;
        }
        //printf("mant %ld %x \n",imant,imant);
        //      set mantissa bits ( bits 22-0 )
        ieee = ieee | imant;
//
//  Transfer IEEE bit string to rieee array
//
        rieee[j]=ieee;

      }

      return;

}
