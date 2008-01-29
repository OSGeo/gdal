/**********************************************************************
 * $Id: geoconcept_syscoord.c
 *
 * Name:     geoconcept_syscoord.c
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

#include "geoconcept_syscoord.h"
#include "cpl_string.h"

GCSRS_CVSID("$Id: geoconcept_syscoord.c,v 1.0.0 2007-12-24 15:40:28 drichard Exp $")

/* -------------------------------------------------------------------- */
/*      GCSRS API Prototypes                                            */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
static void GCSRSAPI_CALL _InitSysCoord_GCSRS (
                                                GCSysCoord* theSysCoord
                                              )
{
  SetSysCoordSystemID_GCSRS(theSysCoord, -1);
  SetSysCoordTimeZone_GCSRS(theSysCoord, -1);
}/* _InitSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
GCSysCoord GCSRSAPI_CALL1(*) CreateSysCoord_GCSRS (
                                                    int srsid,
                                                    int timezone
                                                  )
{
  GCSysCoord* theSysCoord;

  if( !(theSysCoord= CPLMalloc(sizeof(GCSysCoord))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept coordinate system.\n"
              );
    return NULL;
  }
  _InitSysCoord_GCSRS(theSysCoord);
  SetSysCoordSystemID_GCSRS(theSysCoord, srsid);
  SetSysCoordTimeZone_GCSRS(theSysCoord, timezone);

  return theSysCoord;
}/* CreateSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
static void GCSRSAPI_CALL _ReInitSysCoord_GCSRS (
                                                  GCSysCoord* theSysCoord
                                                )
{
  _InitSysCoord_GCSRS(theSysCoord);
}/* _ReInitSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
void GCSRSAPI_CALL DestroySysCoord_GCSRS (
                                           GCSysCoord** theSysCoord
                                         )
{
  _ReInitSysCoord_GCSRS(*theSysCoord);
  CPLFree(*theSysCoord);
  *theSysCoord= NULL;
}/* DestroySysCoord_GCSRS */

/* -------------------------------------------------------------------- */
GCSysCoord GCSRSAPI_CALL1(*) OGRSpatialReference2SysCoord_GCSRS ( OGRSpatialReferenceH poSR )
{
  const char *pszProjection;
  char* pszProj4;
  double a, rf, pm, lat_0, lon_0, lat_1, lat_2, lat_ts, x_0, y_0, k_0;
  double p[7];
  GCSysCoord* syscoord= NULL;

  if( !poSR ) return NULL;

  pszProj4= NULL;
  OSRExportToProj4(poSR, &pszProj4);
  if( !pszProj4 ) pszProj4= CPLStrdup("");

  CPLDebug("GEOCONCEPT", "SRS :\n%s", pszProj4);

  if( !OSRIsProjected(poSR) && !OSRIsGeographic(poSR) )
  {
    goto onError;
  }

  if( !(syscoord= CreateSysCoord_GCSRS(-1,-1)) )
  {
    goto onError;
  }

  a= OSRGetSemiMajor(poSR,NULL);
  rf= OSRGetInvFlattening(poSR,NULL);
  pm= OSRGetPrimeMeridian(poSR,NULL);
  OSRGetTOWGS84(poSR,p,7);

  if( OSRIsGeographic(poSR) )
  {
    if( fabs(a-6378137.0000)<=1e-4 && fabs(rf-298.2572)<=1e-4 )
    {
      if( p[0]==0.0 &&
          p[1]==0.0 &&
          p[2]==0.0 &&
          p[3]==0.0 &&
          p[4]==0.0 &&
          p[5]==0.0 &&
          p[6]==0.0 )
      {
        /* epsg:4326, IGNF:RGM04GEO, IGNF:RGFG95GEO */ /* FIXME : SRS name ? */
        SetSysCoordSystemID_GCSRS(syscoord, 101);
        goto end_proc;
      }
      goto onError;
    }
    else if( fabs(a-6378388.0000)<=1e-4 && fabs(rf-297.0000)<=1e-4 )
    {
      if( fabs(p[0]+84.0000)<=1e-4 &&
          fabs(p[1]+97.0000)<=1e-4 &&
          fabs(p[2]+117.0000)<=1e-4 &&
          p[3]==0.0 &&
          p[4]==0.0 &&
          p[5]==0.0 &&
          p[6]==0.0 )
      {
        /* epsg:4230 */
        SetSysCoordSystemID_GCSRS(syscoord, 102);
        goto end_proc;
      }
      goto onError;
    }
    else if( fabs(a-6378135.0000)<=1e-4 && fabs(rf-298.2600)<=1e-4 )
    {
      if( p[0]==0.0 &&
          fabs(p[1]-12.0000)<=1e-4 &&
          fabs(p[2]-6.0000)<=1e-4 &&
          p[3]==0.0 &&
          p[4]==0.0 &&
          p[5]==0.0 &&
          p[6]==0.0 )
      {
        /* epsg:4322 */
        SetSysCoordSystemID_GCSRS(syscoord, 107);
        goto end_proc;
      }
      goto onError;
    }
    else if( fabs(a-6378249.2000)<=1e-4 && fabs(rf-293.4660)<=1e-4 )
    {
      if( fabs(pm-2.3372)<=1e-4 )
      {
        /* IGNF:NTFP */ /* FIXME : nadgrids and towgs84 */
        SetSysCoordSystemID_GCSRS(syscoord, 105);
        goto end_proc;
      }
      goto onError;
    }
    goto onError;
  }

  lat_0= OSRGetProjParm(poSR,SRS_PP_LATITUDE_OF_ORIGIN,0.0,NULL);
  lon_0= OSRGetProjParm(poSR,SRS_PP_CENTRAL_MERIDIAN,0.0,NULL);
  lat_1= OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_1,0.0,NULL);
  lat_2= OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_2,0.0,NULL);
  lat_ts= OSRGetProjParm(poSR,SRS_PP_PSEUDO_STD_PARALLEL_1,0.0,NULL);
  x_0= OSRGetProjParm(poSR,SRS_PP_FALSE_EASTING,0.0,NULL);
  y_0= OSRGetProjParm(poSR,SRS_PP_FALSE_NORTHING,0.0,NULL);
  k_0= OSRGetProjParm(poSR,SRS_PP_SCALE_FACTOR,1.0,NULL);

  pszProjection = OSRGetAttrValue(poSR, "PROJECTION", 0);
  if( pszProjection == NULL )
  {
    CPLError(CE_Warning, CPLE_IllegalArg,
             "Failed parsing spatial reference system '%s'.\n",
             pszProj4);
    goto onError;
  }

  if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
  {
    /* IGNF:IGN72LAM ? */
    if( fabs(a-6378388.0000)<=1e-4 && fabs(rf-297.0)<=1e-4 )
    {
      goto onError;
    }

    if( fabs(a-6378249.2000)<=1e-4 && fabs(rf-293.4660)<=1e-4 &&
        fabs(pm-2.3372)<=1e-4)
    {
      if( fabs(lat_0-49.5)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
          fabs(k_0-0.99987734)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==200000.000 )
        {
          /* IGNF:LAMB1 */
          SetSysCoordSystemID_GCSRS(syscoord, 2);
          goto end_proc;
        }
        if( y_0==1200000.000 )
        {
          /* IGNF:LAMB1C */
          SetSysCoordSystemID_GCSRS(syscoord, 1002);
          goto end_proc;
        }
      }
      else if( fabs(lat_0-46.8)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987742)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==200000.000 )
        {
          /* IGNF:LAMB2 */
          SetSysCoordSystemID_GCSRS(syscoord, 3);
          goto end_proc;
        }
        if( y_0==2200000.000 )
        {
          /* IGNF:LAMB2C */
          SetSysCoordSystemID_GCSRS(syscoord, 1003);
          goto end_proc;
        }
      }
      else if( fabs(lat_0-44.1)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987750)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==200000.000 )
        {
          /* IGNF:LAMB3 */
          SetSysCoordSystemID_GCSRS(syscoord, 4);
          goto end_proc;
        }
        if( y_0==3200000.000 )
        {
          /* IGNF:LAMB3C */
          SetSysCoordSystemID_GCSRS(syscoord, 1004);
          goto end_proc;
        }
      }
      else if( fabs(lat_0-42.165)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99994471)<=1e-8 && fabs(x_0-234.358)<=1e-3 )
      {
        if( fabs(y_0-185861.369)<=1e-3 )
        {
          /* IGNF:LAMB4 */
          SetSysCoordSystemID_GCSRS(syscoord, 5);
          goto end_proc;
        }
        if( fabs(y_0-4185861.369)<=1e-3 )
        {
          /* IGNF:LAMB4C */
          SetSysCoordSystemID_GCSRS(syscoord, 1005);
          goto end_proc;
        }
      }
      else if( fabs(lat_0-46.8)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987742)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==2200000.000 )
        {
          /* IGNF:LAMBE */
          SetSysCoordSystemID_GCSRS(syscoord, 1);
          goto end_proc;
        }
      }
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
  {
    if( fabs(a-6378249.2000)<=1e-4 && fabs(rf-293.4660)<=1e-4 &&
        fabs(pm-2.3372)<=1e-4)
    {
      /* IGNF:LAMBGC ? */
      goto onError;
    }

    if( fabs(a-6378137.0000)<=1e-4 && fabs(rf-298.2572)<=1e-4)
    {
      if( fabs(lon_0-3.0)<=1.e-4 )
      {
        if( fabs(lat_0-46.5)<=1e-4 && fabs(lat_1-44.0)<=1e-4 && fabs(lat_2-49.0)<=1e-4 &&
            x_0==700000.000 && y_0==6600000.000 )
        {
          /* IGNF:LAMB93 */
          SetSysCoordSystemID_GCSRS(syscoord, 1006);
          goto end_proc;
        }
        else if( fabs(lat_0-42.0)<=1e-4 && fabs(lat_1-41.25)<=1e-4 && fabs(lat_2-42.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==1200000.000 )
        {
          /* IGNF:LAMBCC42 */
          SetSysCoordSystemID_GCSRS(syscoord, 2501);
          goto end_proc;
        }
        else if( fabs(lat_0-43.0)<=1e-4 && fabs(lat_1-42.25)<=1e-4 && fabs(lat_2-43.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==2200000.000 )
        {
          /* IGNF:LAMBCC43 */
          SetSysCoordSystemID_GCSRS(syscoord, 2502);
          goto end_proc;
        }
        else if( fabs(lat_0-44.0)<=1e-4 && fabs(lat_1-43.25)<=1e-4 && fabs(lat_2-44.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==3200000.000 )
        {
          /* IGNF:LAMBCC44 */
          SetSysCoordSystemID_GCSRS(syscoord, 2503);
          goto end_proc;
        }
        else if( fabs(lat_0-45.0)<=1e-4 && fabs(lat_1-44.25)<=1e-4 && fabs(lat_2-45.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==4200000.000 )
        {
          /* IGNF:LAMBCC45 */
          SetSysCoordSystemID_GCSRS(syscoord, 2504);
          goto end_proc;
        }
        else if( fabs(lat_0-46.0)<=1e-4 && fabs(lat_1-45.25)<=1e-4 && fabs(lat_2-46.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==5200000.000 )
        {
          /* IGNF:LAMBCC46 */
          SetSysCoordSystemID_GCSRS(syscoord, 2505);
          goto end_proc;
        }
        else if( fabs(lat_0-47.0)<=1e-4 && fabs(lat_1-46.25)<=1e-4 && fabs(lat_2-47.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==6200000.000 )
        {
          /* IGNF:LAMBCC47 */
          SetSysCoordSystemID_GCSRS(syscoord, 2506);
          goto end_proc;
        }
        else if( fabs(lat_0-48.0)<=1e-4 && fabs(lat_1-47.25)<=1e-4 && fabs(lat_2-48.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==7200000.000 )
        {
          /* IGNF:LAMBCC48 */
          SetSysCoordSystemID_GCSRS(syscoord, 2507);
          goto end_proc;
        }
        else if( fabs(lat_0-49.0)<=1e-4 && fabs(lat_1-48.25)<=1e-4 && fabs(lat_2-49.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==8200000.000 )
        {
          /* IGNF:LAMBCC49 */
          SetSysCoordSystemID_GCSRS(syscoord, 2508);
          goto end_proc;
        }
        else if( fabs(lat_0-50.0)<=1e-4 && fabs(lat_1-49.25)<=1e-4 && fabs(lat_2-50.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==9200000.000 )
        {
          /* IGNF:LAMBCC50 */
          SetSysCoordSystemID_GCSRS(syscoord, 2509);
          goto end_proc;
        }
        goto onError;
      }
      else if ( fabs(lon_0-166.0)<=1e-4 )
      {
        if( fabs(lat_0+21.3)<=1e-4 && fabs(lat_1+20.4)<=1e-4 && fabs(lat_2+22.2)<=1e-4 &&
                 x_0==400000.000 && y_0==300000.000 )
        {
          /* IGNF:RGNCLAM */
          SetSysCoordSystemID_GCSRS(syscoord, 1007);
          goto end_proc;
        }
        goto onError;
      }
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
  {
    if( fabs(a-6378137.0000)<=1e-4 && lon_0==0.0 && x_0==0.0 && y_0==0.0 )
    {
      /* IGNF:MILLER */
      SetSysCoordSystemID_GCSRS(syscoord, 222);
      goto end_proc;
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
  {
    if( fabs(a-6378137.0000)<=1e-4 &&
        lat_0==0.0 &&
        x_0==500000.000 && (y_0==0.000 || y_0==10000000.000) &&
        fabs(k_0-0.99960000)<=1e-8 )
    {
      if( lon_0==-63.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:UTM20W84GUAD, IGNF:UTM20W84MART */
          SetSysCoordSystemID_GCSRS(syscoord, 501); /* 502 */
          goto end_proc;
        }
      }
      if( lon_0==45.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGM04UTM38S */
          SetSysCoordSystemID_GCSRS(syscoord, 503);
          goto end_proc;
        }
      }
      if( lon_0==57.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGR92UTM40S */
          SetSysCoordSystemID_GCSRS(syscoord, 504);
          goto end_proc;
        }
      }
      if( lon_0==-51.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:UTM22RGFG95 */
          SetSysCoordSystemID_GCSRS(syscoord, 505);
          goto end_proc;
        }
      }
      if( lon_0==-177.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:UTM01SW84 */
          SetSysCoordSystemID_GCSRS(syscoord, 506);
          goto end_proc;
        }
      }
      if( lon_0==-153.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGPFUTM5S */
          SetSysCoordSystemID_GCSRS(syscoord, 508);
          goto end_proc;
        }
      }
      if( lon_0==-147.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGPFUTM6S */
          SetSysCoordSystemID_GCSRS(syscoord, 509);
          goto end_proc;
        }
      }
      if( lon_0==-141.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGPFUTM7S */
          SetSysCoordSystemID_GCSRS(syscoord, 510);
          goto end_proc;
        }
      }
      if( lon_0==-57.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:RGSPM06U21 */
          SetSysCoordSystemID_GCSRS(syscoord, 507);
          goto end_proc;
        }
      }
      if( lon_0==159.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGNCUTM57S */
          SetSysCoordSystemID_GCSRS(syscoord, 513);
          goto end_proc;
        }
      }
      if( lon_0==165.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGNCUTM58S */
          SetSysCoordSystemID_GCSRS(syscoord, 514);
          goto end_proc;
        }
      }
      if( lon_0==171.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:RGNCUTM59S */
          SetSysCoordSystemID_GCSRS(syscoord, 515);
          goto end_proc;
        }
      }
      if( lon_0==51.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:UTM39SW84 */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 39);
          goto end_proc;
        }
      }
      if( lon_0==75.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:UTM43SW84*/
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 43);
          goto end_proc;
        }
      }
      if( lon_0==69.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:UTM42SW84 */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 42);
          goto end_proc;
        }
      }
      goto onError;
    }
    if( fabs(a-6378388.0000)<=1e-4 &&
        lat_0==0.0 &&
        x_0==500000.000 && (y_0==0.000 || y_0==10000000.000) &&
        fabs(k_0-0.99960000)<=1e-8 )
    {
      if( lon_0==-63.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:GUAD48UTM20, IGNF:MART38UTM20 */
          SetSysCoordSystemID_GCSRS(syscoord, 17);
          SetSysCoordTimeZone_GCSRS(syscoord, 20);
          goto end_proc;
        }
      }
      if( lon_0==45.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:MAYO50UTM38S */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 38);
          goto end_proc;
        }
      }
      if( lon_0==-51.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:CSG67UTM22 */
          SetSysCoordSystemID_GCSRS(syscoord, 17);
          SetSysCoordTimeZone_GCSRS(syscoord, 22);
          goto end_proc;
        }
      }
      if( lon_0==-177.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:WALL78UTM1S */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 1);
          goto end_proc;
        }
      }
      if( lon_0==-153.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:TAHAAUTM05S */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 5);
          goto end_proc;
        }
      }
      if( lon_0==-147.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:MOOREA87U6S, IGNF:TAHI79UTM6S, */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 6);
          goto end_proc;
        }
      }
      if( lon_0==-141.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:NUKU72U7S, IGNF:IGN63UTM7S */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 7);
          goto end_proc;
        }
      }
      if( lon_0==-57.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:STPM50UTM21 */
          SetSysCoordSystemID_GCSRS(syscoord, 17);
          SetSysCoordTimeZone_GCSRS(syscoord, 21);
          goto end_proc;
        }
      }
      if( lon_0==-63.0 )
      {
        if( y_0==0.000 ) {
          /* IGNF:GUADFM49U20 */
          SetSysCoordSystemID_GCSRS(syscoord, 17);
          SetSysCoordTimeZone_GCSRS(syscoord, 20);
          goto end_proc;
        }
      }
      if( lon_0==165.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:IGN72UTM58S */
          SetSysCoordSystemID_GCSRS(syscoord, 15);
          SetSysCoordTimeZone_GCSRS(syscoord, 58);
          goto end_proc;
        }
      }
      if( lon_0==51.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:CROZ63UTM39S */
          SetSysCoordSystemID_GCSRS(syscoord, 511);
          goto end_proc;
        }
      }
      if( lon_0==69.0 )
      {
        if( y_0>0.000 ) {
          /* IGNF:KERG62UTM42S */
          SetSysCoordSystemID_GCSRS(syscoord, 516);
          goto end_proc;
        }
      }
      goto onError;
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
  {
    if( fabs(a-6378137.0000)<=1e-4 && lon_0==0.0 && x_0==0.0 && y_0==0.0 )
    {
      if( lat_ts==0.0 )
      {
        /* epsg:32662 */
        SetSysCoordSystemID_GCSRS(syscoord, 13);
        goto end_proc;
      }
      else if( fabs(lat_ts-15.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALANF */
        SetSysCoordSystemID_GCSRS(syscoord, 2016);
        goto end_proc;
      }
      else if( fabs(lat_ts+46.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALCRZ */
        SetSysCoordSystemID_GCSRS(syscoord, 2040);
        goto end_proc;
      }
      else if( fabs(lat_ts-46.5)<=1e-4 )
      {
        /* IGNF:GEOPORTALFXX */
        SetSysCoordSystemID_GCSRS(syscoord, 2012);
        goto end_proc;
      }
      else if( fabs(lat_ts-4.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALGUF */
        SetSysCoordSystemID_GCSRS(syscoord, 2017);
        goto end_proc;
      }
      else if( fabs(lat_ts+49.5)<=1e-4 )
      {
        /* IGNF:GEOPORTALKER */
        SetSysCoordSystemID_GCSRS(syscoord, 2042);
        goto end_proc;
      }
      else if( fabs(lat_ts+12.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALMYT */
        SetSysCoordSystemID_GCSRS(syscoord, 2019);
        goto end_proc;
      }
      else if( fabs(lat_ts+22.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALNCL */
        SetSysCoordSystemID_GCSRS(syscoord, 2021);
        goto end_proc;
      }
      else if( fabs(lat_ts+15.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALPYF */
        SetSysCoordSystemID_GCSRS(syscoord, 2023);
        goto end_proc;
      }
      else if( fabs(lat_ts+21.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALREU */
        SetSysCoordSystemID_GCSRS(syscoord, 2018);
        goto end_proc;
      }
      else if( fabs(lat_ts-47.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALSPM */
        SetSysCoordSystemID_GCSRS(syscoord, 2020);
        goto end_proc;
      }
      else if( fabs(lat_ts+14.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALWLF */
        SetSysCoordSystemID_GCSRS(syscoord, 2022);
        goto end_proc;
      }
      goto onError;
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_GAUSSLABORDEREUNION) )
  {
    if( fabs(a-6378388.0000)<=1e-4 &&
        fabs(lat_0+21.117)<=1e-4 && fabs(lon_0-55.5333)<=1e-4 &&
        x_0==160000.000 && y_0==50000.000 )
    {
      /* IGNF:REUN47GAUSSL */
      SetSysCoordSystemID_GCSRS(syscoord, 520);
      goto end_proc;
    }
    goto onError;
  }

end_proc:
  if( pszProj4 )
  {
    CPLFree(pszProj4);
  }
  return syscoord;

onError:
  if( pszProj4 )
  {
    CPLError(CE_Warning, CPLE_IllegalArg,
             "Unknown spatial reference system '%s'.\n"
             "Geoconcept only supports projections.\n"
             "This driver supports only few of them.\n",
             pszProj4);
    CPLFree(pszProj4);
  }
  if( syscoord )
  {
    CPLFree(syscoord);
  }
  return NULL;
}/* OGRSpatialReference2SysCoord_GCSRS */

/* -------------------------------------------------------------------- */
OGRSpatialReferenceH GCSRSAPI_CALL SysCoord2OGRSpatialReference_GCSRS ( GCSysCoord* syscoord )
{
  OGRSpatialReferenceH poSR;

  poSR= OSRNewSpatialReference(NULL);
  switch( GetSysCoordSystemID_GCSRS(syscoord) )
  {
    case    1 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBE +wkext");  break;
    case    2 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB1 +wkext");  break;
    case    3 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB2 +wkext");  break;
    case    4 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB3 +wkext");  break;
    case    5 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB4 +wkext");  break;
    case   13 : OSRImportFromProj4(poSR,"+init=epsg:32662");         break;
    case   15 : switch( GetSysCoordTimeZone_GCSRS(syscoord) )
                {
                  case  1 : OSRImportFromProj4(poSR,"+init=IGNF:WALL78UTM1S"); break;
                  case  5 : OSRImportFromProj4(poSR,"+init=IGNF:TAHAAUTM05S"); break;
                  case  6 : OSRImportFromProj4(poSR,"+init=IGNF:MOOREA87U6S"); break;/* TAHI79UTM6S ? */
                  case  7 : OSRImportFromProj4(poSR,"+init=IGNF:NUKU72U7S");   break;/* IGN63UTM7S  ? */
                  case 38 : OSRImportFromProj4(poSR,"+init=epsg:32738");       break;
                  case 39 : OSRImportFromProj4(poSR,"+init=epsg:32739");       break;
                  case 42 : OSRImportFromProj4(poSR,"+init=epsg:32742");       break;
                  case 43 : OSRImportFromProj4(poSR,"+init=epsg:32743");       break;
                  case 58 : OSRImportFromProj4(poSR,"+init=epsg:32758");       break;
                  default :                                                    break;
                }
                break;
    case   17 : switch( GetSysCoordTimeZone_GCSRS(syscoord) )
                {
                  case 20 : OSRImportFromProj4(poSR,"+init=IGNF:MART38UTM20"); break;/* GUAD48UTM20 ? GUADFM49U20 ? */
                  case 21 : OSRImportFromProj4(poSR,"+init=IGNF:STPM50UTM21"); break;
                  case 22 : OSRImportFromProj4(poSR,"+init=IGNF:CSG67UTM22");  break;
                  default :                                                    break;
                }
                break;
    case  101 : OSRImportFromProj4(poSR,"+init=epsg:4326");          break;
    case  102 : OSRImportFromProj4(poSR,"+init=epsg:4230");          break;
    case  105 : OSRImportFromProj4(poSR,"+init=IGNF:NTFP +wkext");   break;
    case  107 : OSRImportFromProj4(poSR,"+init=epsg:4322");          break;
    case  222 : OSRImportFromProj4(poSR,"+init=IGNF:MILLER");        break;
    case  501 : OSRImportFromProj4(poSR,"+init=IGNF:RRAFGUADU20");   break;
    case  502 : OSRImportFromProj4(poSR,"+init=IGNF:RRAFMARTU20");   break;
    case  503 : OSRImportFromProj4(poSR,"+init=IGNF:RGM04UTM38S");   break;
    case  504 : OSRImportFromProj4(poSR,"+init=IGNF:RGR92UTM40S");   break;
    case  505 : OSRImportFromProj4(poSR,"+init=IGNF:UTM22RGFG95");   break;
    case  506 : OSRImportFromProj4(poSR,"+init=epsg:32701");         break;
    case  507 : OSRImportFromProj4(poSR,"+init=IGNF:RGSPM06U21");    break;
    case  508 : OSRImportFromProj4(poSR,"+init=IGNF:RGPFUTM5S");     break;
    case  509 : OSRImportFromProj4(poSR,"+init=IGNF:RGPFUTM6S");     break;
    case  510 : OSRImportFromProj4(poSR,"+init=IGNF:RGPFUTM7S");     break;
    case  511 : OSRImportFromProj4(poSR,"+init=IGNF:CROZ63UTM39S");  break;
    case  513 : OSRImportFromProj4(poSR,"+init=IGNF:RGNCUTM57S");    break;
    case  514 : OSRImportFromProj4(poSR,"+init=IGNF:RGNCUTM58S");    break;
    case  515 : OSRImportFromProj4(poSR,"+init=IGNF:RGNCUTM59S");    break;
    case  516 : OSRImportFromProj4(poSR,"+init=IGNF:KERG62UTM42S");  break;
    case  520 : OSRImportFromProj4(poSR,"+init=IGNF:REUN47GAUSSL");  break;
    case 1002 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB1C +wkext"); break;
    case 1003 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB2C +wkext"); break;
    case 1004 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB3C +wkext"); break;
    case 1005 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB4C +wkext"); break;
    case 1006 : OSRImportFromProj4(poSR,"+init=IGNF:LAMB93");        break;
    case 1007 : OSRImportFromProj4(poSR,"+init=IGNF:RGNCLAM");       break;
    case 2012 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALFXX");  break;
    case 2016 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALANF");  break;
    case 2017 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALGUF");  break;
    case 2018 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALREU");  break;
    case 2019 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALMYT");  break;
    case 2020 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALSPM");  break;
    case 2021 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALNCL");  break;
    case 2022 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALWLF");  break;
    case 2023 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALPYF");  break;
    case 2040 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALCRZ");  break;
    case 2042 : OSRImportFromProj4(poSR,"+init=IGNF:GEOPORTALKER");  break;
    case 2501 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC42");      break;
    case 2502 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC43");      break;
    case 2503 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC44");      break;
    case 2504 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC45");      break;
    case 2505 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC46");      break;
    case 2506 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC47");      break;
    case 2507 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC48");      break;
    case 2508 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC49");      break;
    case 2509 : OSRImportFromProj4(poSR,"+init=IGNF:LAMBCC50");      break;
    case 5030 : OSRImportFromProj4(poSR,"+init=IGNF:RGM04GEO");      break;
    case 5031 : OSRImportFromProj4(poSR,"+init=IGNF:RGFG95GEO");     break;

    default   :
      break;
  }

/* -------------------------------------------------------------------- */
/*      Report on translation.                                          */
/* -------------------------------------------------------------------- */
  {
    char* pszWKT;

    OSRExportToWkt(poSR,&pszWKT);
    if( pszWKT!=NULL )
    {
        CPLDebug( "GEOCONCEPT",
                  "This SysCoord value:\n%d:%d\nwas translated to:\n%s",
                  GetSysCoordSystemID_GCSRS(syscoord),
                  GetSysCoordTimeZone_GCSRS(syscoord),
                  pszWKT );
        CPLFree( pszWKT );
    }
  }

  return poSR;
}/* SysCoord2OGRSpatialReference_GCSRS */
