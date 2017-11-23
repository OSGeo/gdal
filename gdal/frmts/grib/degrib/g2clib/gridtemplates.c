#include <stdlib.h>
#include "grib2.h"
#include "gridtemplates.h"

/* GDAL: in original g2clib, this is in gridtemplates.h */
static const struct gridtemplate templatesgrid[MAXGRIDTEMP] = {
             // 3.0: Lat/Lon grid
         { 0, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.1: Rotated Lat/Lon grid
         { 1, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4} },
             // 3.2: Stretched Lat/Lon grid
         { 2, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,-4} },
             // 3.3: Stretched & Rotated Lat/Lon grid
         { 3, 25, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4,-4,4,-4} },
// Added GDT 3.4,3.5    (08/05/2013)
             // 3.4: Variable resolution Latitude/Longitude
         { 4, 13, 1, {1,1,4,1,4,1,4,4,4,4,4,1,1} },
             // 3.5: Variable resolution rotate Latitude/Longitude
         { 5, 16, 1, {1,1,4,1,4,1,4,4,4,4,4,1,1,-4,4,4} },
             // 3.12: Transverse Mercator
         {12, 22, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,1,4,4,-4,-4,-4,-4} },
             // 3.101: General unstructured grid
         {101, 4, 0, {1,4,1,-4} },
             // 3.140: Lambert Azimuthal Equal Area Projection
         {140, 17, 0, {1,1,4,1,4,1,4,4,4,-4,4,4,4,1,4,4,1} },
//
             // 3.10: Mercator
         {10, 19, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,-4,4,1,4,4,4} },
             // 3.20: Polar Stereographic Projection
         {20, 18, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,4,1,1} },
             // 3.30: Lambert Conformal
         {30, 22, 0, {1,1,4,1,4,1,4,4,4,-4,4,1,-4,4,4,4,1,1,-4,-4,-4,4} },
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
             // 3.204: Curvilinear Orthogonal Grid
         {204, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.32768: Rot Lat/Lon E-grid (Arakawa)
         {32768, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.32769: Rot Lat/Lon Non-E Staggered grid (Arakawa)
         {32769, 21, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,4,4} },
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
! 2007-08-16  Vuong     -  Added GDT 3.204  Curvilinear Orthogonal Grid
! 2008-07-08  Vuong     -  Added GDT 3.32768 Rotate Lat/Lon E-grid (Arakawa)
! 2009-01-14  Vuong     -  Changed structure name template to gtemplate
! 2010-05-11  Vuong     -  Added GDT 3.32769 Rotate Lat/Lon Non-E Staggered grid (Arakawa)
! 2013-08-06  Vuong     -  Added GDT 3.4,3.5,3.12,3.101,3.140
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
           g2int j,l_getgridindex=-1;

           for (j=0;j<MAXGRIDTEMP;j++) {
              if (number == templatesgrid[j].template_num) {
                 l_getgridindex=j;
                 return(l_getgridindex);
              }
           }

           return(l_getgridindex);
}

gtemplate *getgridtemplate(g2int number)
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
! 2007-08-16  Vuong     -  Added GDT 3.204  Curvilinear Orthogonal Grid
! 2008-07-08  Vuong     -  Added GDT 3.32768 Rotate Lat/Lon E-grid (Arakawa)
! 2010-05-11  Vuong     -  Added GDT 3.32769 Rotate Lat/Lon Non-E Staggered grid (Arakawa)
! 2009-01-14  Vuong     -  Changed structure name template to gtemplate
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
           g2int l_index;
           gtemplate *new;

           l_index=getgridindex(number);

           if (l_index != -1) {
              new=(gtemplate *)malloc(sizeof(gtemplate));
              new->type=3;
              new->num=templatesgrid[l_index].template_num;
              new->maplen=templatesgrid[l_index].mapgridlen;
              new->needext=templatesgrid[l_index].needext;
              new->map=(g2int *)templatesgrid[l_index].mapgrid;
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


gtemplate *extgridtemplate(g2int number,g2int *list)
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
! 2008-07-08  Vuong     -  Added GDT 3.32768 Rotate Lat/Lon E-grid (Arakawa)
! 2009-01-14  Vuong     -  Changed structure name template to gtemplate
! 2010-05-11  Vuong     -  Added GDT 3.32769 Rotate Lat/Lon Non-E Staggered grid (Arakawa)
! 2013-08-06  Vuong     -  Added GDT 3.4,3.5,3.12,3.101,3.140
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
           gtemplate *new;
           g2int l_index,i;

           l_index=getgridindex(number);
           if (l_index == -1) return(0);

           new=getgridtemplate(number);
           if( new == NULL ) return(NULL);

           if ( ! new->needext ) return(new);

           if ( number == 120 ) {
              /* Not sure of the threshold, but 100000 looks to be large */
              /* enough */
              if( list[1] < 0 || list[1] > 100000 )
                return new;
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
#if 0
           /* Commented out by GDAL: memory leaks... */
           else if ( number == 4 ) {
              new->extlen=list[7];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
              new->extlen=list[8];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=-4;
              }
           }
           else if ( number == 5 ) {
              new->extlen=list[7];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
              new->extlen=list[8];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=-4;
              }
           }
#endif
           else if ( number == 1000 ) {
               /* Not sure of the threshold, but 100000 looks to be large */
              /* enough */
              if( list[19] < 0 || list[19] > 100000 )
                return new;
              new->extlen=list[19];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
           }
           else if ( number == 1200 ) {
              /* Not sure of the threshold, but 100000 looks to be large */
              /* enough */
              if( list[15] < 0 || list[15] > 100000 )
                return new;
              new->extlen=list[15];
              new->ext=(g2int *)malloc(sizeof(g2int)*new->extlen);
              for (i=0;i<new->extlen;i++) {
                 new->ext[i]=4;
              }
           }

           return(new);

}
