#include <stdlib.h>
#include "grib2.h"
#include "pdstemplates.h"


static const struct pdstemplate templatespds[MAXPDSTEMP] = {
             // 4.0: Analysis or Forecast at Horizontal Level/Layer
             //      at a point in time
         {0,15,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4} },
             // 4.1: Individual Ensemble Forecast at Horizontal Level/Layer
             //      at a point in time
         {1,18,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1} },
             // 4.2: Derived Fcst based on whole Ensemble at Horiz Level/Layer
             //      at a point in time
         {2,17,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1} },
             // 4.3: Derived Fcst based on Ensemble cluster over rectangular
             //      area at Horiz Level/Layer at a point in time
         {3,31,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,-4,4,4,1,-1,4,-1,4} },
             // 4.4: Derived Fcst based on Ensemble cluster over circular
             //      area at Horiz Level/Layer at a point in time
         {4,30,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,4,4,1,-1,4,-1,4} },
             // 4.5: Probablility Forecast at Horiz Level/Layer
             //      at a point in time
         {5,22,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,-1,-4,-1,-4} },
             // 4.6: Percentile Forecast at Horiz Level/Layer
             //      at a point in time
         {6,16,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1} },
             // 4.7: Analysis or Forecast Error at Horizontal Level/Layer
             //      at a point in time
         {7,15,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4} },
             // 4.8: Ave/Accum/etc... at Horiz Level/Layer
             //      in a time interval
         {8,29,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.9: Probablility Forecast at Horiz Level/Layer
             //      in a time interval
         {9,36,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,-1,-4,-1,-4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.10: Percentile Forecast at Horiz Level/Layer
             //       in a time interval
         {10,30,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.11: Individual Ensemble Forecast at Horizontal Level/Layer
             //       in a time interval
         {11,32,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.12: Derived Fcst based on whole Ensemble at Horiz Level/Layer
             //       in a time interval
         {12,31,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.13: Derived Fcst based on Ensemble cluster over rectangular
             //       area at Horiz Level/Layer in a time interval
         {13,45,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,-4,4,4,1,-1,4,-1,4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.14: Derived Fcst based on Ensemble cluster over circular
             //       area at Horiz Level/Layer in a time interval
         {14,44,1, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,1,1,1,1,1,1,1,-4,4,4,1,-1,4,-1,4,2,1,1,1,1,1,1,4,1,1,1,4,1,4} },
             // 4.20: Radar Product
         {20,19,0, {1,1,1,1,1,-4,4,2,4,2,1,1,1,1,1,2,1,3,2} },
             // 4.30: Satellite Product
         {30,5,1, {1,1,1,1,1} },
             // 4.254: CCITT IA5 Character String
         {254,3,0, {1,1,4} },
             // 4.1000: Cross section of analysis or forecast
             //         at a point in time
         {1000,9,0, {1,1,1,1,1,2,1,1,4} },
             // 4.1001: Cross section of Ave/Accum/etc... analysis or forecast
             //         in a time interval
         {1001,16,0, {1,1,1,1,1,2,1,1,4,4,1,1,1,4,1,4} },
             // 4.1001: Cross section of Ave/Accum/etc... analysis or forecast
             //         over latitude or longitude
         {1002,15,0, {1,1,1,1,1,2,1,1,4,1,1,1,4,4,2} },
             // 4.1100: Hovmoller-type grid w/ no averaging or other
             //         statistical processing
         {1100,15,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4} },
             // 4.1100: Hovmoller-type grid with averaging or other
             //         statistical processing
         {1101,22,0, {1,1,1,1,1,2,1,1,4,1,-1,-4,1,-1,-4,4,1,1,1,4,1,4} }

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
//
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
           g2int j,getpdsindex=-1;

           for (j=0;j<MAXPDSTEMP;j++) {
              if (number == templatespds[j].template_num) {
                 getpdsindex=j;
                 return(getpdsindex);
              }
           }

           return(getpdsindex);
}


xxtemplate *getpdstemplate(g2int number)
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
           g2int index;
           xxtemplate *new;

           index=getpdsindex(number);

           if (index != -1) {
              new=(xxtemplate *)malloc(sizeof(xxtemplate));
              new->type=4;
              new->num=templatespds[index].template_num;
              new->maplen=templatespds[index].mappdslen;
              new->needext=templatespds[index].needext;
              new->map=(g2int *)templatespds[index].mappds;
              new->extlen=0;
              new->ext=0;        //NULL
              return(new);
           }
           else {
             printf("getpdstemplate: PDS Template 4.%d not defined.\n",(int)number);
             return(0);        //NULL
           }

         return(0);        //NULL
}
         
        
xxtemplate *extpdstemplate(g2int number,g2int *list)
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
           xxtemplate *new;
           g2int index,i,j,k,l;

           index=getpdsindex(number);
           if (index == -1) return(0);

           new=getpdstemplate(number);

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
              new->extlen=((list[37]-1)*6)+list[26];
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
              new->extlen=((list[36]-1)*6)+list[25];
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
           return(new);

}

