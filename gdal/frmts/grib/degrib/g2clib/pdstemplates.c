#include <stdlib.h>
#include "grib2.h"
#include "pdstemplates.h"

/* GDAL: in original g2clib, this is in pdstemplates.h */
static const struct pdstemplate templatespds[MAXPDSTEMP] = {
             // 4.0: Analysis or Forecast at Horizontal Level/Layer
             //      at a point in time
         {0,15,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },
             // 4.1: Individual Ensemble Forecast at Horizontal Level/Layer
             //      at a point in time
         {1,18,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1} },
             // 4.2: Derived Fcst based on whole Ensemble at Horiz Level/Layer
             //      at a point in time
         {2,17,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1} },
             // 4.3: Derived Fcst based on Ensemble cluster over rectangular
             //      area at Horiz Level/Layer at a point in time
         {3,31,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,-4,4,4,1,-1,4,-1,4} },
             // 4.4: Derived Fcst based on Ensemble cluster over circular
             //      area at Horiz Level/Layer at a point in time
         {4,30,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,4,4,1,-1,4,-1,4} },
             // 4.5: Probablility Forecast at Horiz Level/Layer
             //      at a point in time
         {5,22,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,-1,-4,-1,-4} },
             // 4.6: Percentile Forecast at Horiz Level/Layer
             //      at a point in time
         {6,16,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1} },
             // 4.7: Analysis or Forecast Error at Horizontal Level/Layer
             //      at a point in time
         {7,15,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },
             // 4.8: Ave/Accum/etc... at Horiz Level/Layer
             //      in a time interval
         {8,29,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.9: Probablility Forecast at Horiz Level/Layer
             //      in a time interval
         {9,36,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,-1,-4,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.10: Percentile Forecast at Horiz Level/Layer
             //       in a time interval
         {10,30,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.11: Individual Ensemble Forecast at Horizontal Level/Layer
             //       in a time interval
         {11,32,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.12: Derived Fcst based on whole Ensemble at Horiz Level/Layer
             //       in a time interval
         {12,31,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.13: Derived Fcst based on Ensemble cluster over rectangular
             //       area at Horiz Level/Layer in a time interval
         {13,45,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,-4,4,4,1,-1,4,-1,4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.14: Derived Fcst based on Ensemble cluster over circular
             //       area at Horiz Level/Layer in a time interval
         {14,44,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,4,4,1,-1,4,-1,4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.15: Average, accumulation, extreme values or other statistically-processed values over a
             // spatial area at a horizontal level or in a horizontal layer at a point in time
         {15,18,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1} },
             // 4.20: Radar Product
         {20,19,0, {1,1,1,1,1,-4,4,2,4,2,1,1,1,1,1,2,1,3,2} },
             // 4.30: Satellite Product
         {30,5,1, {1,1,1,1,1} },
             // 4.31: Satellite Product
         {31,5,1, {1,1,1,1,1} },
             // 4.40: Analysis or forecast at a horizontal level or in a horizontal layer
             // at a point in time for atmospheric chemical constituents
         {40,16,0, {1,1,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },
             // 4.41: Individual ensemble forecast, control and perturbed, at a horizontal level or
             // in a horizontal layer at a point in time for atmospheric chemical constituents
         {41,19,0, {1,1,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1} },
             // 4.42: Average, accumulation, and/or extreme values or other statistically-processed values
             // at a horizontal level or in a horizontal layer in a continuous or non-continuous
             // time interval for atmospheric chemical constituents
         {42,30,1, {1,1,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.43: Individual ensemble forecast, control and perturbed, at a horizontal level
             // or in a horizontal layer in a continuous or non-continuous
             // time interval for atmospheric chemical constituents
         {43,33,1, {1,1,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.254: CCITT IA5 Character String
         {254,3,0, {1,1,4} },
             // 4.1000: Cross section of analysis or forecast
             //         at a point in time
         {1000,9,0, {1,1,1,1,1,2,1,1,-4} },
             // 4.1001: Cross section of Ave/Accum/etc... analysis or forecast
             //         in a time interval
         {1001,16,0, {1,1,1,1,1,2,1,1,4,4,1,1,1,4,1,4} },
             // 4.1001: Cross section of Ave/Accum/etc... analysis or forecast
             //         over latitude or longitude
         {1002,15,0, {1,1,1,1,1,2,1,1,-4,1,1,1,4,4,2} },
             // 4.1100: Hovmoller-type grid w/ no averaging or other
             //         statistical processing
         {1100,15,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },
             // 4.1100: Hovmoller-type grid with averaging or other
             //         statistical processing
         {1101,22,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,4,1,1,1,4,1,4} },
             // 4.32:Simulate (synthetic) Satellite Product
         {32,10,1, {1,1,1,1,1,2,1,1,-4,1} }, // (last-1)th coefficient corrected from - 2 to -4 by ERO
             // 4.44: Analysis or forecast at a horizontal level or in a horizontal layer
             // at a point in time for Aerosol
         {44,21,0, {1,1,2,1,-1,-4,-1,-4,1,1,1,2,1,1,-2,1,-1,-4,1,-1,-4} },
             // 4.45: Individual ensemble forecast, control and 
             // perturbed,  at a horizontal level or in a horizontal layer
             // at a point in time for Aerosol
         {45,24,0, {1,1,2,1,-1,-4,-1,-4,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1} },
             // 4.46: Ave or Accum or Extreme value at level/layer
             // at horizontal level or in a horizontal in a continuous or
             // non-continuous time interval for Aerosol
         {46,35,1, {1,1,2,1,-1,-4,-1,-4,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.47: Individual ensemble forecast, control and 
             // perturbed, at horizontal level or in a horizontal
             // in a continuous or non-continuous time interval for Aerosol
         {47,38,1, {1,1,1,2,1,-1,-4,-1,-4,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },

             //             PDT 4.48
             // 4.48: Analysis or forecast at a horizontal level or in a horizontal layer
             // at a point in time for Optical Properties of Aerosol
         {48,26,0, {1,1,2,1,-1,-4,-1,-4,1,-1,-4,-1,-4,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },

             //             VALIDATION --- PDT 4.50
             // 4.50: Analysis or forecast of multi component parameter or
             // matrix element at a point in time
         {50,21,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,4,4,4,4} },

             //             VALIDATION --- PDT 4.52
             // 4.52: Analysis or forecast of Wave parameters
             // at the Sea surface at a point in time
         {52,15,0, {1,1,1,1,1,1,1,1,2,1,1,-4,1,-1,-4} },

             // 4.51: Categorical forecasts at a horizontal level or
             // in a horizontal layer at a point in time
         {51,16,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1} },

             // 4.91: Categorical forecasts at a horizontal level or
             // in a horizontal layer at a point in time
             // in a continuous or non-continuous time interval
         {91,36,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,-1,-4,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
// PDT 4.33  (07/29/2013)
             // 4.33: Individual ensemble forecast, control, perturbed,
             // at a horizontal level or in a  horizontal layer
             // at a point in time for simulated (synthetic) Satellite data
         {33,18,1, {1,1,1,1,1,2,1,1,-4,1,2,2,2,-1,-4,1,1,1} },
// PDT 4.34  (07/29/2013)
             // 4.34: Individual ensemble forecast, control, perturbed,
             // at a horizontal level or in a  horizontal layer,in a continuous or
             // non-continuous interval for simulated (synthetic) Satellite data
         {34,32,1, {1,1,1,1,1,2,1,1,-4,1,2,2,2,-1,-4,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
// PDT 4.53  (07/29/2013)
             // 4.53:  Partitioned parameters at
             // horizontal level or horizontal layer
             // at a point in time
         {53,19,1, {1,1,1,1,4,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4} },
// PDT 4.54  (07/29/2013)
             // 4.54: Individual ensemble forecast, control, perturbed,
             // at a horizontal level or in a  horizontal layer
             // at a point in time for partitioned parameters
         {54,22,1, {1,1,1,1,4,2,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1} },
// PDT 4.57  (10/07/2015)
             // 4.57: Analysis or Forecast at a horizontal or in a
             // horizontal layer at a point in time for
             // atmospheric chemical constituents based on 
             // a distribution function
         {57,7,1, {1,1,2,2,2,2,1} },
// PDT 4.60  (10/07/2015)
             // 4.60: Individual ensemble reforecast, control and perturbed,
             // at a horizontal level or in a horizontal layer
             // at a point in time
         {60,24,0, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1} },
// PDT 4.61  (10/07/2015)
             // 4.61: Individual ensemble reforecast, control and perturbed,
             // at a horizontal level or in a  horizontal layer
             // in a continuous or non-continuous time interval
         {61,38,1, {1,1,1,1,1,2,1,1,-4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
//             VALIDATION --- PDT 4.35
// PDT 4.35  (10/07/2015)
             // 4.35: Individual ensemble reforecast, control and perturbed,
             // at a horizontal level or in a  horizontal layer
             // in a continuous or non-continuous time interval
         {35,6,1, {1,1,1,1,1,1} }

      } ;

const struct pdstemplate *get_templatespds()
{
    return templatespds;
}

g2int getpdsindex(g2int number)
///$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    getpdsindex
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2001-06-28
//
// ABSTRACT: This function returns the index of specified Product
//   Definition Template 4.NN (NN=number) in array templates.
//
// PROGRAM HISTORY LOG:
// 2001-06-28  Gilbert
// 2009-01-14  Vuong     Changed structure name template to gtemplate
// 2009-12-15  Vuong     Added Product Definition Template 4.31
//                       Added Product Definition Template 4.15
// 2010-08-03  Vuong     Added Product Definition Template 4.42 and 4.43
// 2010-12-08  Vuong     Corrected Product Definition Template 4.42 and 4.43
// 2012-03-29  Vuong     Added Templates 4.44,4.45,4.46,4.47,4.48,4.50,
//                       4.51,4.91,4.32 and 4.52
// 2013-08-05  Vuong     Corrected 4.91 and added Templates 4.33,4.34,4.53,4.54
// 2015-10-07  Vuong     Added Templates 4.57, 4.60, 4.61 and
//                       allow a forecast time to be negative
// USAGE:    index=getpdsindex(number)
//   INPUT ARGUMENT LIST:
//     number   - NN, indicating the number of the Product Definition
//                Template 4.NN that is being requested.
//
// RETURNS:  Index of PDT 4.NN in array templates, if template exists.
//           = -1, otherwise.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$/
{
           g2int j,l_getpdsindex=-1;

           for (j=0;j<MAXPDSTEMP;j++) {
              if (number == templatespds[j].template_num) {
                 l_getpdsindex=j;
                 return(l_getpdsindex);
              }
           }

           return(l_getpdsindex);
}


gtemplate *getpdstemplate(g2int number)
///$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    getpdstemplate
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-11
//
// ABSTRACT: This subroutine returns PDS template information for a
//   specified Product Definition Template 4.NN.
//   The number of entries in the template is returned along with a map
//   of the number of octets occupied by each entry.  Also, a flag is
//   returned to indicate whether the template would need to be extended.
//
// PROGRAM HISTORY LOG:
// 2000-05-11  Gilbert
// 2009-01-14  Vuong     Changed structure name template to gtemplate
// 2009-08-05  Vuong     Added Product Definition Template 4.31
// 2010-08-03  Vuong     Added Product Definition Template 4.42 and 4.43
// 2010-12-08  Vuong     Corrected Product Definition Template 4.42 and 4.43
// 2012-02-15  Vuong     Added Templates 4.44,4.45,4.46,4.47,4.48,4.50,
//                       4.51,4.91,4.32 and 4.52
// 2013-08-05  Vuong     Corrected 4.91 and added Templates 4.33,4.34,4.53,4.54
// 2015-10-07  Vuong     Added Templates 4.57, 4.60, 4.61 and
//                       allow a forecast time to be negative
//
// USAGE:    CALL getpdstemplate(number)
//   INPUT ARGUMENT LIST:
//     number   - NN, indicating the number of the Product Definition
//                Template 4.NN that is being requested.
//
//   RETURN VALUE:
//        - Pointer to the returned template struct.
//          Returns NULL pointer, if template not found.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$/
{
           g2int l_index;
           gtemplate *new;

           l_index=getpdsindex(number);

           if (l_index != -1) {
              new=(gtemplate *)malloc(sizeof(gtemplate));
              new->type=4;
              new->num=templatespds[l_index].template_num;
              new->maplen=templatespds[l_index].mappdslen;
              new->needext=templatespds[l_index].needext;
              new->map=(g2int *)templatespds[l_index].mappds;
              new->extlen=0;
              new->ext=0;        //NULL
              return(new);
           }
           else {
             //printf("getpdstemplate: PDS Template 4.%d not defined.\n",(int)number);
             return(0);        //NULL
           }

         return(0);        //NULL
}


gtemplate *extpdstemplate(g2int number,g2int *list)
///$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    extpdstemplate
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-11
//
// ABSTRACT: This subroutine generates the remaining octet map for a
//   given Product Definition Template, if required.  Some Templates can
//   vary depending on data values given in an earlier part of the
//   Template, and it is necessary to know some of the earlier entry
//   values to generate the full octet map of the Template.
//
// PROGRAM HISTORY LOG:
// 2000-05-11  Gilbert
// 2009-01-14  Vuong     Changed structure name template to gtemplate
// 2009-08-05  Vuong     Added Product Definition Template 4.31
// 2010-08-03  Vuong     Added Product Definition Template 4.42 and 4.43
// 2010-12-08  Vuong     Corrected Product Definition Template 4.42 and 4.43
// 2012-02-15  Vuong     Added Templates 4.44,4.45,4.46,4.47,4.48,4.50,
//                       4.51,4.91,4.32 and 4.52
// 2013-08-05  Vuong     Corrected 4.91 and added Templates 4.33,4.34,4.53,4.54
// 2015-10-07  Vuong     Added Templates 4.57, 4.60, 4.61 and
//                       allow a forecast time to be negative
//
// USAGE:    CALL extpdstemplate(number,list)
//   INPUT ARGUMENT LIST:
//     number   - NN, indicating the number of the Product Definition
//                Template 4.NN that is being requested.
//     list()   - The list of values for each entry in the
//                the Product Definition Template 4.NN.
//
//   RETURN VALUE:
//        - Pointer to the returned template struct.
//          Returns NULL pointer, if template not found.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  IBM SP
//
//$$$
{
           gtemplate *new;
           g2int l_index,i,j,k,l;

           l_index=getpdsindex(number);
           if (l_index == -1) return(0);

           new=getpdstemplate(number);
           if (new == NULL) return NULL;

           if ( ! new->needext ) return(new);

           if ( number == 3 ) {
              new->extlen=list[26];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=1;
              }
           }
           else if ( number == 4 ) {
              new->extlen=list[25];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=1;
              }
           }
           else if ( number == 8 ) {
              if ( list[21] > 1 ) {
                 new->extlen=(list[21]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[21];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[23+k];
                    }
                 }
              }
           }
           else if ( number == 9 ) {
              if ( list[28] > 1 ) {
                 new->extlen=(list[28]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[28];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[30+k];
                    }
                 }
              }
           }
           else if ( number == 10 ) {
              if ( list[22] > 1 ) {
                 new->extlen=(list[22]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[22];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[24+k];
                    }
                 }
              }
           }
           else if ( number == 11 ) {
              if ( list[24] > 1 ) {
                 new->extlen=(list[24]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[24];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[26+k];
                    }
                 }
              }
           }
           else if ( number == 12 ) {
              if ( list[23] > 1 ) {
                 new->extlen=(list[23]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[23];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[25+k];
                    }
                 }
              }
           }
           else if ( number == 13 ) {
              new->extlen=(list[37] > 1 ? ((list[37]-1)*6) : 0)+list[26];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              if ( list[37] > 1 ) {
                 for (j=2;j<=list[37];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[39+k];
                    }
                 }
              }
              l=(list[37]-1)*6;
              if ( l<0 ) l=0;
              for (i=0;i<list[26];i++) {
                new->ext[l+i]=1;
              }
           }
           else if ( number == 14 ) {
              new->extlen=(list[36] > 1 ? ((list[36]-1)*6) : 0) +list[25];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              if ( list[36] > 1 ) {
                 for (j=2;j<=list[36];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[38+k];
                    }
                 }
              }
              l=(list[36]-1)*6;
              if ( l<0 ) l=0;
              for (i=0;i<list[25];i++) {
                new->ext[l+i]=1;
              }
           }
           else if ( number == 30 ) {
              new->extlen=list[4]*5;
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<list[4];i++) {
                 l=i*5;
                 new->ext[l]=2;
                 new->ext[l+1]=2;
                 new->ext[l+2]=1;
                 new->ext[l+3]=1;
                 new->ext[l+4]=4;
              }
           }
           else if ( number == 31 ) {
              new->extlen=list[4]*5;
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<list[4];i++) {
                 l=i*5;
                 new->ext[l]=2;
                 new->ext[l+1]=2;
                 new->ext[l+2]=2;
                 new->ext[l+3]=1;
                 new->ext[l+4]=4;
              }
           }
           else if ( number == 42 ) {
              if ( list[22] > 1 ) {
                 new->extlen=(list[22]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[22];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[24+k];
                    }
                 }
              }
           }
           else if ( number == 43 ) {
              if ( list[25] > 1 ) {
                 new->extlen=(list[25]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[25];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[27+k];
                    }
                 }
              }
           }
           else if ( number == 32 ) {
              new->extlen=list[9]*5; /* ERO: was 10, but wrong given the below loop */
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<list[9];i++) {
                 l=i*5;
                 new->ext[l]=2;
                 new->ext[l+1]=2;
                 new->ext[l+2]=2;
                 new->ext[l+3]=-1;
                 new->ext[l+4]=-4;
              }
           }
           else if ( number == 46 ) {
              if ( list[27] > 1 ) {
                 new->extlen=(list[27]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[27];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[29+k];
                    }
                 }
              }
           }
           else if ( number == 47 ) {
              if ( list[30] > 1 ) {
                 new->extlen=(list[30]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[30];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[32+k];
                    }
                 }
              }
           }
           else if ( number == 51 ) {
              new->extlen=list[15]*6; /* GDAL fix */
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<list[15];i++) {
                 l=i*6;
                 new->ext[l]=1;
                 new->ext[l+1]=1;
                 new->ext[l+2]=-1;
                 new->ext[l+3]=-4;
                 new->ext[l+4]=-1;
                 new->ext[l+5]=-4;
              }
           }
           else if ( number == 33 ) {
              new->extlen=list[9];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=1;
              }
           }
           else if ( number == 34 ) {
              new->extlen=(list[24] > 1 ? ((list[24]-1)*6) : 0)+list[9];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              if ( list[24] > 1 ) {
                 for (j=2;j<=list[24];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[26+k];
                    }
                 }
              }
              l=(list[24]-1)*6;
              if ( l<0 ) l=0;
              for (i=0;i<list[9];i++) {
                new->ext[l+i]=1;
              }
           }
           else if ( number == 53 ) {
              new->extlen=list[3];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=1;
              }
           }
           else if ( number == 54 ) {
              new->extlen=list[3];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=1;
              }
           }
           else if ( number == 91 ) {
              new->extlen=(list[28] > 1 ? ((list[28]-1)*6) : 0)+list[15];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              if ( list[28] > 1 ) {
                 for (j=2;j<=list[28];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[30+k];
                    }
                 }
              }
              l=(list[28]-1)*6; /* GDAL fix */
              if ( l<0 ) l=0;
              for (i=0;i<list[15];i++) {
                new->ext[l+i]=1;
              }
             }
// PDT 4.57  (10/07/2015)
           else if ( number == 57 ) {
              new->extlen=list[6]*15;
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<list[6];i++) {
                 l=i*15;
                 new->ext[l]=1;
                 new->ext[l+1]=-4;
                 new->ext[l+2]=1;
                 new->ext[l+3]=1;
                 new->ext[l+4]=1;
                 new->ext[l+5]=2;
                 new->ext[l+6]=1;
                 new->ext[l+7]=1;
                 new->ext[l+8]=-4;
                 new->ext[l+9]=1;
                 new->ext[l+10]=-1;
                 new->ext[l+11]=-4;
                 new->ext[l+12]=1;
                 new->ext[l+13]=-1;
                 new->ext[l+14]=-4;
              }
           }
// PDT 4.61  (10/07/2015)
           else if ( number == 61 ) {
              if ( list[30] > 1 ) {
                 new->extlen=(list[30]-1)*6;
                 new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
                 for (j=2;j<=list[30];j++) {
                    l=(j-2)*6;
                    for (k=0;k<6;k++) {
                       new->ext[l+k]=new->map[32+k];
                    }
                 }
              }
           }

           return(new);

}

