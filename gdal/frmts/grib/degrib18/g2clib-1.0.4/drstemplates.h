#ifndef _drstemplates_H
#define _drstemplates_H
#include "grib2.h"

//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-26
//
// ABSTRACT: This Fortran Module contains info on all the available 
//   GRIB2 Data Representation Templates used in Section 5 (DRS).
//   The information decribing each template is stored in the
//   drstemplate structure defined below.
//
//   Each Template has three parts: The number of entries in the template
//   (mapdrslen);  A map of the template (mapdrs), which contains the
//   number of octets in which to pack each of the template values; and
//   a logical value (needext) that indicates whether the Template needs 
//   to be extended.  In some cases the number of entries in a template 
//   can vary depending upon values specified in the "static" part of 
//   the template.  ( See Template 5.1 as an example )
//
//   NOTE:  Array mapdrs contains the number of octets in which the 
//   corresponding template values will be stored.  A negative value in
//   mapdrs is used to indicate that the corresponding template entry can
//   contain negative values.  This information is used later when packing
//   (or unpacking) the template data values.  Negative data values in GRIB
//   are stored with the left most bit set to one, and a negative number
//   of octets value in mapdrs[] indicates that this possibility should
//   be considered.  The number of octets used to store the data value
//   in this case would be the absolute value of the negative value in 
//   mapdrs[].
//  
//
///////////////////////////////////////////////////////////////////////

      #define MAXDRSTEMP 9              // maximum number of templates
      #define MAXDRSMAPLEN 200          // maximum template map length

      struct drstemplate
      {
          g2int template_num;
          g2int mapdrslen;
          g2int needext;
          g2int mapdrs[MAXDRSMAPLEN];
      };

const struct drstemplate *get_templatesdrs();
g2int getdrsindex(g2int number);

#endif  /*  _drstemplates_H  */
