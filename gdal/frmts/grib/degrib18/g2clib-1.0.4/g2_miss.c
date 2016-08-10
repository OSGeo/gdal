#include "grib2.h"

void g2_miss( gribfield *gfld, float *rmiss, int *nmiss )
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_miss 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2004-12-16
//
// ABSTRACT: This routine checks the Data Representation Template to see if
// missing value management is used, and returns the missing value(s) 
// in the data field.
//
// PROGRAM HISTORY LOG:
// 2004-12-16  Gilbert
//
// USAGE:    g2_miss( gribfield *gfld, float *rmiss, int *nmiss )
//                   
//   INPUT ARGUMENT LIST:
//     *gfld    - pointer to gribfield structure (defined in include file 
//                   grib2.h)
//
//   OUTPUT ARGUMENT LIST:      
//     rmiss    - List of the missing values used
//     nmiss    - NUmber of the missing values included in the field
//
// REMARKS:  rmiss must be allocated in the calling program with enough space 
//           hold all the missing values.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{
    g2int     itype;    

    /*
     *  Missing value management currently only used in
     *  DRT's 5.2 and 5.3.
     */
    if ( gfld->idrtnum != 2  &&  gfld->idrtnum != 3 ) {
       *nmiss=0;
       return;
    }
                                                                                
    itype = gfld->idrtmpl[4];
    if ( gfld->idrtmpl[6] == 1 ) {
         *nmiss=1;
         if (itype == 0)
            rdieee(gfld->idrtmpl+7,rmiss+0,1);
         else
            rmiss[0]=(float)gfld->idrtmpl[7];
    }
    else if ( gfld->idrtmpl[6] == 2 ) {
         *nmiss=2;
         if (itype == 0) {
            rdieee(gfld->idrtmpl+7,rmiss+0,1);
            rdieee(gfld->idrtmpl+8,rmiss+1,1);
         }
         else {
            rmiss[0]=(float)gfld->idrtmpl[7];
            rmiss[1]=(float)gfld->idrtmpl[8];
         }
    }
    else {
       *nmiss=0;
    }

}
