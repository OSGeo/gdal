/*****************************************************************************
 * grib2api.h
 *
 * DESCRIPTION
 *    This file contains the header information needed to call the grib2
 * decoder library.
 *
 * HISTORY
 *   12/2003 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef GRIB2API_H
#define GRIB2API_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "type.h"

void unpk_grib2 (sInt4 *kfildo, float *ain, sInt4 *iain, sInt4 *nd2x3,
                 sInt4 *idat, sInt4 *nidat, float *rdat, sInt4 *nrdat,
                 sInt4 *is0, sInt4 *ns0, sInt4 *is1, sInt4 *ns1, sInt4 *is2,
                 sInt4 *ns2, sInt4 *is3, sInt4 *ns3, sInt4 *is4, sInt4 *ns4,
                 sInt4 *is5, sInt4 *ns5, sInt4 *is6, sInt4 *ns6, sInt4 *is7,
                 sInt4 *ns7, sInt4 *ib, sInt4 *ibitmap, sInt4 *ipack,
                 sInt4 *nd5, float *xmissp, float *xmisss, sInt4 *inew,
                 sInt4 *iclean, sInt4 *l3264b, sInt4 *iendpk, sInt4 *jer,
                 sInt4 *ndjer, sInt4 *kjer);

int C_pkGrib2 (unsigned char *cgrib, sInt4 *sec0, sInt4 *sec1,
               unsigned char *csec2, sInt4 lcsec2,
               sInt4 *igds, sInt4 *igdstmpl, sInt4 *ideflist,
               sInt4 idefnum, sInt4 ipdsnum, sInt4 *ipdstmpl,
               float *coordlist, sInt4 numcoord, sInt4 idrsnum,
               sInt4 *idrstmpl, float *fld, sInt4 ngrdpts,
               sInt4 ibmap, sInt4 *bmap);

void pk_grib2 (sInt4 * kfildo, float * ain, sInt4 * iain, sInt4 * nx,
               sInt4 * ny, sInt4 * idat, sInt4 * nidat, float * rdat,
               sInt4 * nrdat, sInt4 * is0, sInt4 * ns0, sInt4 * is1,
               sInt4 * ns1, sInt4 * is3, sInt4 * ns3, sInt4 * is4,
               sInt4 * ns4, sInt4 * is5, sInt4 * ns5, sInt4 * is6,
               sInt4 * ns6, sInt4 * is7, sInt4 * ns7, sInt4 * ib,
               sInt4 * ibitmap, sInt4 * ipack, sInt4 * nd5, sInt4 * missp,
               float * xmissp, sInt4 * misss, float * xmisss, sInt4 * inew,
               sInt4 * minpk, sInt4 * iclean, sInt4 * l3264b, sInt4 * jer,
               sInt4 * ndjer, sInt4 * kjer);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* GRIB2API_H */
