#include <stdlib.h>
#include "grib2.h"
#include "drstemplates.h"

/* GDAL: in original g2clib, this is in drstemplates.h */
static const struct drstemplate templatesdrs[MAXDRSTEMP] = {
             // 5.0: Grid point data - Simple Packing
         { 0, 5, 0, {4,-2,-2,1,1} },
             // 5.2: Grid point data - Complex Packing
         { 2, 16, 0, {4,-2,-2,1,1,1,1,4,4,4,1,1,4,1,4,1} },
             // 5.3: Grid point data - Complex Packing and spatial differencing
         { 3, 18, 0, {4,-2,-2,1,1,1,1,4,4,4,1,1,4,1,4,1,1,1} },
             // 5.4: Grid point data - IEEE Floating Point Data
         { 4, 1, 0, {1} },
             // 5.50: Spectral Data - Simple Packing
         { 50, 5, 0, {4,-2,-2,1,4} },
             // 5.51: Spherical Harmonics data - Complex packing 
         { 51, 10, 0, {4,-2,-2,1,-4,2,2,2,4,1} },
//           // 5.1: Matrix values at gridpoint - Simple packing
//         { 1, 15, 1, {4,-2,-2,1,1,1,4,2,2,1,1,1,1,1,1} },
             // 5.40: Grid point data - JPEG2000 encoding
         { 40, 7, 0, {4,-2,-2,1,1,1,1} },
             // 5.41: Grid point data - PNG encoding
         { 41, 5, 0, {4,-2,-2,1,1} },
             // 5.40000: Grid point data - JPEG2000 encoding
         { 40000, 7, 0, {4,-2,-2,1,1,1,1} },
             // 5.40010: Grid point data - PNG encoding
         { 40010, 5, 0, {4,-2,-2,1,1} }
      } ;

const struct drstemplate *get_templatesdrs()
{
    return templatesdrs;
}


g2int getdrsindex(g2int number)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    getdrsindex
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2001-06-28
!
! ABSTRACT: This function returns the index of specified Data
!   Representation Template 5.NN (NN=number) in array templates.
!
! PROGRAM HISTORY LOG:
! 2001-06-28  Gilbert
!
! USAGE:    index=getdrsindex(number)
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Data Representation
!                Template 5.NN that is being requested.
!
! RETURNS:  Index of DRT 5.NN in array templates, if template exists.
!           = -1, otherwise.
!
! REMARKS: None
!
! ATTRIBUTES:
!   LANGUAGE: C
!   MACHINE:  IBM SP
!
!$$$*/
{
           g2int j,l_getdrsindex=-1;

           for (j=0;j<MAXDRSTEMP;j++) {
              if (number == templatesdrs[j].template_num) {
                 l_getdrsindex=j;
                 return(l_getdrsindex);
              }
           }

           return(l_getdrsindex);
}


gtemplate *getdrstemplate(g2int number)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    getdrstemplate
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-11
!
! ABSTRACT: This subroutine returns DRS template information for a
!   specified Data Representation Template 5.NN.
!   The number of entries in the template is returned along with a map
!   of the number of octets occupied by each entry.  Also, a flag is
!   returned to indicate whether the template would need to be extended.
!
! PROGRAM HISTORY LOG:
! 2000-05-11  Gilbert
! 2009-01-14  Vuong     Changed structure name template to gtemplate
!
! USAGE:    new=getdrstemplate(number);
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Data Representation
!                Template 5.NN that is being requested.
!
!   RETURN VALUE:
!        - Pointer to the returned template struct.
!          Returns NULL pointer, if template not found.
!
! REMARKS: None
!
! ATTRIBUTES:
!   LANGUAGE: C
!   MACHINE:  IBM SP
!
!$$$*/
{
           g2int l_index;
           gtemplate *new;

           l_index=getdrsindex(number);

           if (l_index != -1) {
              new=(gtemplate *)malloc(sizeof(gtemplate));
              new->type=5;
              new->num=templatesdrs[l_index].template_num;
              new->maplen=templatesdrs[l_index].mapdrslen;
              new->needext=templatesdrs[l_index].needext;
              new->map=(g2int *)templatesdrs[l_index].mapdrs;
              new->extlen=0;
              new->ext=0;        //NULL
              return(new);
           }
           else {
             printf("getdrstemplate: DRS Template 5.%d not defined.\n",(int)number);
             return(0);        //NULL
           }

         return(0);        //NULL
}

gtemplate *extdrstemplate(g2int number,g2int *list)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    extdrstemplate
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-11
!
! ABSTRACT: This subroutine generates the remaining octet map for a
!   given Data Representation Template, if required.  Some Templates can
!   vary depending on data values given in an earlier part of the
!   Template, and it is necessary to know some of the earlier entry
!   values to generate the full octet map of the Template.
!
! PROGRAM HISTORY LOG:
! 2000-05-11  Gilbert
! 2009-01-14  Vuong     Changed structure name template to gtemplate
!
! USAGE:    new=extdrstemplate(number,list);
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Data Representation
!                Template 5.NN that is being requested.
!     list()   - The list of values for each entry in the
!                the Data Representation Template 5.NN.
!
!   RETURN VALUE:
!        - Pointer to the returned template struct.
!          Returns NULL pointer, if template not found.
!
! ATTRIBUTES:
!   LANGUAGE: C
!   MACHINE:  IBM SP
!
!$$$*/
{
           gtemplate *new;
           g2int l_index,i;

           l_index=getdrsindex(number);
           if (l_index == -1) return(0);

           new=getdrstemplate(number);
           if (new == NULL) return NULL;

           if ( ! new->needext ) return(new);

           if ( number == 1 ) {
              new->extlen=list[10]+list[12];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                new->ext[i]=4;
              }
           }
           return(new);

}

