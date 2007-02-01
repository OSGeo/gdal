#include <stdlib.h>
#include "grib2.h"
#include "pdstemplates.h"

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


template *getpdstemplate(g2int number)
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
           template *new;

           index=getpdsindex(number);

           if (index != -1) {
              new=(template *)malloc(sizeof(template));
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
         
        
template *extpdstemplate(g2int number,g2int *list)
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
           template *new;
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

