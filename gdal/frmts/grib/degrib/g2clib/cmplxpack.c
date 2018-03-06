#include "grib2.h"

void cmplxpack(g2float *fld,g2int ndpts, g2int idrsnum,g2int *idrstmpl,
               unsigned char *cpack, g2int *lcpack)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    cmplxpack
//   PRGMMR: Gilbert          ORG: W/NP11    DATE: 2004-08-27
//
// ABSTRACT: This subroutine packs up a data field using a complex
//   packing algorithm as defined in the GRIB2 documentation.  It
//   supports GRIB2 complex packing templates with or without
//   spatial differences (i.e. DRTs 5.2 and 5.3).
//   It also fills in GRIB2 Data Representation Template 5.2 or 5.3
//   with the appropriate values.
//
// PROGRAM HISTORY LOG:
// 2004-08-27  Gilbert
//
// USAGE:    cmplxpack(g2float *fld,g2int ndpts, g2int idrsnum,g2int *idrstmpl,
//             unsigned char *cpack, g2int *lcpack)
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
//                [0] = Reference value - set by compack routine.
//                [1] = Binary Scale Factor - unchanged from input
//                [2] = Decimal Scale Factor - unchanged from input
//                    .
//                    .
//     cpack    - The packed data field (character*1 array)
//     lcpack   - length of packed field cpack[].
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{


      if ( idrstmpl[6] == 0 ) {       // No internal missing values
         compack(fld,ndpts,idrsnum,idrstmpl,cpack,lcpack);
      }
      else if ( idrstmpl[6] == 1  ||  idrstmpl[6] == 2) {
         misspack(fld,ndpts,idrsnum,idrstmpl,cpack,lcpack);
      }
      else {
         printf("cmplxpack: Don:t recognize Missing value option.");
         *lcpack=-1;
      }

}
