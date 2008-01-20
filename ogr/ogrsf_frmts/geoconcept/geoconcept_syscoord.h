/**********************************************************************
 * $Id: geoconcept_syscoord.h
 *
 * Name:     geoconcept_syscoord.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements translation between Geoconcept SysCoord
 *           and OGRSpatialRef format
 * Language: C
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/
#ifndef _GEOCONCEPT_SYSCOORD_H_INCLUDED
#define _GEOCONCEPT_SYSCOORD_H_INCLUDED

#include "ogr_srs_api.h"

#ifdef GCSRS_DLLEXPORT
#  define GCSRSAPI_CALL     __declspec(dllexport)
#  define GCSRSAPI_CALL1(x) __declspec(dllexport) x
#endif

#ifndef GCSRSAPI_CALL
#  define GCSRSAPI_CALL
#endif

#ifndef GCSRSAPI_CALL1
#  define GCSRSAPI_CALL1(x) x GCSRSAPI_CALL
#endif

/* -------------------------------------------------------------------- */
/*      Macros for controlling CVSID and ensuring they don't appear     */
/*      as unreferenced variables resulting in lots of warnings.        */
/* -------------------------------------------------------------------- */
#ifndef DISABLE_CVSID
#  define GCSRS_CVSID(string)     static char gcsrs_cvsid[] = string; \
static char *cvsid_aw() { return( cvsid_aw() ? ((char *) NULL) : gcsrs_cvsid ); }
#else
#  define GCSRS_CVSID(string)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/*      GCSRS API Types                                                 */
/* -------------------------------------------------------------------- */
typedef struct _tSysCoord_GCSRS GCSysCoord;

struct _tSysCoord_GCSRS {
  int coordSystemID;
  int timeZoneValue;
};

/* -------------------------------------------------------------------- */
/*      GCSRS API Prototypes                                            */
/* -------------------------------------------------------------------- */

GCSysCoord GCSRSAPI_CALL1(*) CreateSysCoord_GCSRS ( int srsid, int timezone );
void GCSRSAPI_CALL DestroySysCoord_GCSRS ( GCSysCoord** theSysCoord );
GCSysCoord GCSRSAPI_CALL1(*) OGRSpatialReference2SysCoord_GCSRS ( OGRSpatialReferenceH poSR );
OGRSpatialReferenceH GCSRSAPI_CALL SysCoord2OGRSpatialReference_GCSRS ( GCSysCoord* syscoord );

#define GetSysCoordSystemID_GCSRS(theSysCoord)   (theSysCoord)->coordSystemID
#define SetSysCoordSystemID_GCSRS(theSysCoord,v) (theSysCoord)->coordSystemID= (v)
#define GetSysCoordTimeZone_GCSRS(theSysCoord)   (theSysCoord)->timeZoneValue
#define SetSysCoordTimeZone_GCSRS(theSysCoord,v) (theSysCoord)->timeZoneValue= (v)

#ifdef __cplusplus
}
#endif


#endif /* ndef _GEOCONCEPT_SYSCOORD_H_INCLUDED */
