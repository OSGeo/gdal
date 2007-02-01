#include <stdlib.h>
#include "grib2.h"
#include "drstemplates.h"

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
           g2int j,getdrsindex=-1;

           for (j=0;j<MAXDRSTEMP;j++) {
              if (number == templatesdrs[j].template_num) {
                 getdrsindex=j;
                 return(getdrsindex);
              }
           }

           return(getdrsindex);
}


template *getdrstemplate(g2int number)
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
           g2int index;
           template *new;

           index=getdrsindex(number);

           if (index != -1) {
              new=(template *)malloc(sizeof(template));
              new->type=5;
              new->num=templatesdrs[index].template_num;
              new->maplen=templatesdrs[index].mapdrslen;
              new->needext=templatesdrs[index].needext;
              new->map=(g2int *)templatesdrs[index].mapdrs;
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

template *extdrstemplate(g2int number,g2int *list)
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
           template *new;
           g2int index,i;

           index=getdrsindex(number);
           if (index == -1) return(0);

           new=getdrstemplate(number);

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

