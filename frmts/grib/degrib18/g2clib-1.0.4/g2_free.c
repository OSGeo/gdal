#include <stdlib.h>
#include  "grib2.h"

void g2_free(gribfield *gfld)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_free 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-28
//
// ABSTRACT: This routine frees up memory that was allocated for
//   struct gribfield.
//
// PROGRAM HISTORY LOG:
// 2002-10-28  Gilbert
//
// USAGE:    g2_free(gribfield *gfld)
//   ARGUMENT:
//     gfld - pointer to gribfield structure (defined in include file grib2.h)
//            returned from routine g2_getfld.
//
// REMARKS:  This routine must be called to free up memory used by
//           the decode routine, g2_getfld, when user no longer needs to
//           reference this data.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{ 

      if (gfld->idsect != 0 ) free(gfld->idsect);
      if (gfld->local != 0 ) free(gfld->local);
      if (gfld->list_opt != 0 ) free(gfld->list_opt);
      if (gfld->igdtmpl != 0 ) free(gfld->igdtmpl);
      if (gfld->ipdtmpl != 0 ) free(gfld->ipdtmpl);
      if (gfld->coord_list != 0 ) free(gfld->coord_list);
      if (gfld->idrtmpl != 0 ) free(gfld->idrtmpl);
      if (gfld->bmap != 0 ) free(gfld->bmap);
      if (gfld->fld != 0 ) free(gfld->fld);
      free(gfld);

      return;
}
