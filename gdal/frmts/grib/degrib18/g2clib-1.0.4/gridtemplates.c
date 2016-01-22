#include <stdlib.h>
#include "grib2.h"
#include "gridtemplates.h"


static const struct gridtemplate templatesgrid[MAXGRIDTEMP] = {
             // 3.0: Lat/Lon grid
         { 0, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.1: Rotated Lat/Lon grid
         { 1, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4} },
             // 3.2: Stretched Lat/Lon grid
         { 2, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,-4} },
             // 3.3: Stretched & Rotated Lat/Lon grid
         { 3, 25, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4,-4,4,-4} },
             // 3.10: Mercator
//       {10, 19, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,-4,4,1,4,4,4} },
         {10, 19, 0, {1,1,4,1,4,1,4,4,4,-4,-4,1,-4,-4,-4,1,4,4,4} },
             // 3.20: Polar Stereographic Projection
//       {20, 18, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,4,1,1} },
         {20, 18, 0, {1,1,4,1,4,1,4,4,4,-4,-4,1,-4,-4,4,4,1,1} },
             // 3.30: Lambert Conformal
//       {30, 22, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,4,1,1,-4,-4,-4,4} },
         {30, 22, 0, {1,1,4,1,4,1,4,4,4,-4,-4,1,-4,-4,4,4,1,1,-4,-4,-4,-4} },
             // 3.31: Albers equal area
         {31, 22, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,4,1,1,-4,-4,-4,4} },
             // 3.40: Guassian Lat/Lon
         {40, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.41: Rotated Gaussian Lat/Lon
         {41, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4} },
             // 3.42: Stretched Gaussian Lat/Lon
         {42, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,-4} },
             // 3.43: Stretched and Rotated Gaussian Lat/Lon
         {43, 25, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4,-4,4,-4} },
             // 3.50: Spherical Harmonic Coefficients
         {50, 5, 0, {4,4,4,1,1} },
             // 3.51: Rotated Spherical Harmonic Coefficients
         {51, 8, 0, {4,4,4,1,1,-4,4,4} },
             // 3.52: Stretched Spherical Harmonic Coefficients
         {52, 8, 0, {4,4,4,1,1,-4,4,-4} },
             // 3.53: Stretched and Rotated Spherical Harmonic Coefficients
         {53, 11, 0, {4,4,4,1,1,-4,4,4,-4,4,-4} },
             // 3.90: Space View Perspective or orthographic
         {90, 21, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,4,4,4,4,1,4,4,4,4} },
             // 3.100: Triangular grid based on an icosahedron
         {100, 11, 0, {1,1,2,1,-4,4,4,1,1,1,4} },
             // 3.110: Equatorial Azimuthal equidistant
         {110, 16, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,4,4,1,1} },
             // 3.120: Azimuth-range projection
         {120, 7, 1, {4,4,-4,4,4,4,1} },
             // 3.1000: Cross Section Grid
         {1000, 20, 1, {1,1,4,1,4,1,4,4,4,4,-4,4,1,4,4,1,2,1,1,2} },
             // 3.1100: Hovmoller Diagram Grid
         {1100, 28, 0, {1,1,4,1,4,1,4,4,4,4,-4,4,1,-4,4,1,4,1,-4,1,1,-4,2,1,1,1,1,1} },
             // 3.1200: Time Section Grid
         {1200, 16, 1, {4,1,-4,1,1,-4,2,1,1,1,1,1,2,1,1,2} }

      } ;

const struct gridtemplate *get_templatesgrid()

{
    return templatesgrid;
}

g2int getgridindex(g2int number)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    getgridindex
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2001-06-28
!
! ABSTRACT: This function returns the index of specified Grid
!   Definition Template 3.NN (NN=number) in array templates.
!
! PROGRAM HISTORY LOG:
! 2001-06-28  Gilbert
!
! USAGE:    index=getgridindex(number)
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Grid Definition
!                Template 3.NN that is being requested.
!
! RETURNS:  Index of GDT 3.NN in array templates, if template exists.
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
           g2int j,getgridindex=-1;

           for (j=0;j<MAXGRIDTEMP;j++) {
              if (number == templatesgrid[j].template_num) {
                 getgridindex=j;
                 return(getgridindex);
              }
           }

           return(getgridindex);
}

xxtemplate *getgridtemplate(g2int number)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    getgridtemplate 
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-09
!
! ABSTRACT: This subroutine returns grid template information for a 
!   specified Grid Definition Template 3.NN.
!   The number of entries in the template is returned along with a map
!   of the number of octets occupied by each entry.  Also, a flag is
!   returned to indicate whether the template would need to be extended.
!
! PROGRAM HISTORY LOG:
! 2000-05-09  Gilbert
!
! USAGE:    template *getgridtemplate(number)
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Grid Definition 
!                Template 3.NN that is being requested.
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
           xxtemplate *new;

           index=getgridindex(number);

           if (index != -1) {
              new=(xxtemplate *)malloc(sizeof(xxtemplate));
              new->type=3;
              new->num=templatesgrid[index].template_num;
              new->maplen=templatesgrid[index].mapgridlen;
              new->needext=templatesgrid[index].needext;
              new->map=(g2int *)templatesgrid[index].mapgrid;
              new->extlen=0;
              new->ext=0;        //NULL
              return(new);
           }
           else {
             printf("getgridtemplate: GDT Template 3.%d not defined.\n",(int)number);
             return(0);        //NULL
           }

         return(0);        //NULL
}


xxtemplate *extgridtemplate(g2int number,g2int *list)
/*!$$$  SUBPROGRAM DOCUMENTATION BLOCK
!                .      .    .                                       .
! SUBPROGRAM:    extgridtemplate 
!   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2000-05-09
!
! ABSTRACT: This subroutine generates the remaining octet map for a 
!   given Grid Definition Template, if required.  Some Templates can 
!   vary depending on data values given in an earlier part of the 
!   Template, and it is necessary to know some of the earlier entry
!   values to generate the full octet map of the Template.
!
! PROGRAM HISTORY LOG:
! 2000-05-09  Gilbert
!
! USAGE:    CALL extgridtemplate(number,list)
!   INPUT ARGUMENT LIST:
!     number   - NN, indicating the number of the Grid Definition 
!                Template 3.NN that is being requested.
!     list()   - The list of values for each entry in 
!                the Grid Definition Template.
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
           xxtemplate *new;
           g2int index,i;

           index=getgridindex(number);
           if (index == -1) return(0);

           new=getgridtemplate(number);

           if ( ! new->needext ) return(new);

           if ( number == 120 ) {
              new->extlen=list[1]*2;
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 if ( i%2 == 0 ) {
                    new->ext[i]=2;
                 }
                 else {
                    new->ext[i]=-2;
                 }
              }
           }
           else if ( number == 1000 ) {
              new->extlen=list[19];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
           }
           else if ( number == 1200 ) {
              new->extlen=list[15];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
           }

           return(new);

}
