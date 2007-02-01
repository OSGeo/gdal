#ifndef _gridtemplates_H
#define _gridtemplates_H
#include "grib2.h"

//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2001-10-26
//
// ABSTRACT: This Fortran Module contains info on all the available 
//   GRIB2 Grid Definition Templates used in Section 3 (GDS).
//   The information decribing each template is stored in the
//   gridtemplate structure defined below.
//
//   Each Template has three parts: The number of entries in the template
//   (mapgridlen);  A map of the template (mapgrid), which contains the
//   number of octets in which to pack each of the template values; and
//   a logical value (needext) that indicates whether the Template needs 
//   to be extended.  In some cases the number of entries in a template 
//   can vary depending upon values specified in the "static" part of 
//   the template.  ( See Template 3.120 as an example )
//
//   NOTE:  Array mapgrid contains the number of octets in which the 
//   corresponding template values will be stored.  A negative value in
//   mapgrid is used to indicate that the corresponding template entry can
//   contain negative values.  This information is used later when packing
//   (or unpacking) the template data values.  Negative data values in GRIB
//   are stored with the left most bit set to one, and a negative number
//   of octets value in mapgrid[] indicates that this possibility should
//   be considered.  The number of octets used to store the data value
//   in this case would be the absolute value of the negative value in 
//   mapgrid[].
//  
//
////////////////////////////////////////////////////////////////////

      #define MAXGRIDTEMP 23              // maximum number of templates
      #define MAXGRIDMAPLEN 200           // maximum template map length

      struct gridtemplate
      {
          g2int template_num;
          g2int mapgridlen;
          g2int needext;
          g2int mapgrid[MAXGRIDMAPLEN];
      };

      const struct gridtemplate templatesgrid[MAXGRIDTEMP] = {
             // 3.0: Lat/Lon grid
         { 0, 19, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1} },
             // 3.1: Rotated Lat/Lon grid
         { 1, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4} },
             // 3.2: Stretched Lat/Lon grid
         { 2, 22, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,-4} },
             // 3.3: Stretched & Rotated Lat/Lon grid
         { 3, 25, 0, {1,1,4,1,4,1,4,4,4,4,4,-4,4,1,-4,4,4,4,1,-4,4,4,-4,4,-4} },
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
             // 3.1000: Cross Section Grid
         {1000, 20, 1, {1,1,4,1,4,1,4,4,4,4,-4,4,1,4,4,1,2,1,1,2} },
             // 3.1100: Hovmoller Diagram Grid
         {1100, 28, 0, {1,1,4,1,4,1,4,4,4,4,-4,4,1,-4,4,1,4,1,-4,1,1,-4,2,1,1,1,1,1} },
             // 3.1200: Time Section Grid
         {1200, 16, 1, {4,1,-4,1,1,-4,2,1,1,1,1,1,2,1,1,2} }

      } ;


#endif  /*  _gridtemplates_H  */
