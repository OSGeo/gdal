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
int GCSRSAPI_CALL OGRSpatialReference2SysCoord_GCSRS ( OGRSpatialReferenceH poSR )
{
  const char *pszProjection;
  char* pszProj4;
  double a, rf, pm, lat_0, lon_0, lat_1, lat_2, x_0, y_0, k_0;

  if( !poSR ) return -1;

  pszProj4= NULL;
  OSRExportToProj4(poSR, &pszProj4);
  if( !pszProj4 ) pszProj4= CPLStrdup("");

  CPLDebug("GEOCONCEPT", "SRS :\n%s\n", pszProj4);

  pszProjection = OSRGetAttrValue(poSR, "PROJECTION", 0);
  if( pszProjection == NULL )
  {
    CPLError(CE_Warning, CPLE_IllegalArg,
             "Failed parsing spatial reference system '%s'.\n",
             pszProj4);
    CPLFree(pszProj4);
    return -1;
  }

  if( !OSRIsProjected(poSR) )
  {
    CPLError(CE_Warning, CPLE_IllegalArg,
             "Unknown spatial reference system '%s'.\n"
             "Geoconcept only supports projections.\n"
             "This driver supports only few of them.\n",
             pszProj4);
    CPLFree(pszProj4);
    return -1;
  }

  a= OSRGetSemiMajor(poSR,NULL);
  rf= OSRGetInvFlattening(poSR,NULL);
  pm= OSRGetPrimeMeridian(poSR,NULL);
  lat_0= OSRGetProjParm(poSR,SRS_PP_LATITUDE_OF_ORIGIN,0.0,NULL);
  lon_0= OSRGetProjParm(poSR,SRS_PP_CENTRAL_MERIDIAN,0.0,NULL);
  lat_1= OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_1,0.0,NULL);
  lat_2= OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_2,0.0,NULL);
  x_0= OSRGetProjParm(poSR,SRS_PP_FALSE_EASTING,0.0,NULL);
  y_0= OSRGetProjParm(poSR,SRS_PP_FALSE_NORTHING,0.0,NULL);
  k_0= OSRGetProjParm(poSR,SRS_PP_SCALE_FACTOR,1.0,NULL);

  if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
  {
    /* IGNF:IGN72LAM ? */
    if( fabs(a-6378388.0000)<=1e-4 && rf==297.0 )
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
          return 2;
        }
        if( y_0==1200000.000 )
        {
          /* IGNF:LAMB1C */
          return 1002;
        }
      }
      else if( fabs(lat_0-46.8)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987742)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==200000.000 )
        {
          /* IGNF:LAMB2 */
          return 3;
        }
        if( y_0==2200000.000 )
        {
          /* IGNF:LAMB2C */
          return 1003;
        }
      }
      else if( fabs(lat_0-44.1)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987750)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==200000.000 )
        {
          /* IGNF:LAMB3 */
          return 4;
        }
        if( y_0==3200000.000 )
        {
          /* IGNF:LAMB3C */
          return 1004;
        }
      }
      else if( fabs(lat_0-42.165)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99994471)<=1e-8 && fabs(x_0-234.358)<=1e-3 )
      {
        if( fabs(y_0-185861.369)<=1e-3 )
        {
          /* IGNF:LAMB4 */
          return 5;
        }
        if( fabs(y_0-4185861.369)<=1e-3 )
        {
          /* IGNF:LAMB4C */
          return 1005;
        }
      }
      else if( fabs(lat_0-46.8)<=1e-4 && lon_0==0.0 && lat_1==lat_0 &&
               fabs(k_0-0.99987742)<=1e-8 && x_0==600000.000 )
      {
        if( y_0==2200000.000 )
        {
          /* IGNF:LAMBE */
          return 1;
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
      /* LAMBGC ? */
      goto onError;
    }

    if( fabs(a-6378137.0000)<=1e-4)
    {
      if( fabs(lon_0-3.0)<=1.e-4 )
      {
        if( fabs(lat_0-46.5)<=1e-4 && fabs(lat_1-44.0)<=1e-4 && fabs(lat_2-49.0)<=1e-4 &&
            x_0==700000.000 && y_0==6600000.000 )
        {
          /* IGNF:LAMB93 */
          return 9999;
        }
        else if( fabs(lat_0-42.0)<=1e-4 && fabs(lat_1-41.25)<=1e-4 && fabs(lat_2-42.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==1200000.000 )
        {
          /* IGNF:LAMBCC42 */
          return 9999;
        }
        else if( fabs(lat_0-43.0)<=1e-4 && fabs(lat_1-42.25)<=1e-4 && fabs(lat_2-43.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==2200000.000 )
        {
          /* IGNF:LAMBCC43 */
          return 9999;
        }
        else if( fabs(lat_0-44.0)<=1e-4 && fabs(lat_1-43.25)<=1e-4 && fabs(lat_2-44.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==3200000.000 )
        {
          /* IGNF:LAMBCC44 */
          return 9999;
        }
        else if( fabs(lat_0-45.0)<=1e-4 && fabs(lat_1-44.25)<=1e-4 && fabs(lat_2-45.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==4200000.000 )
        {
          /* IGNF:LAMBCC45 */
          return 9999;
        }
        else if( fabs(lat_0-46.0)<=1e-4 && fabs(lat_1-45.25)<=1e-4 && fabs(lat_2-46.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==5200000.000 )
        {
          /* IGNF:LAMBCC46 */
          return 9999;
        }
        else if( fabs(lat_0-47.0)<=1e-4 && fabs(lat_1-46.25)<=1e-4 && fabs(lat_2-47.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==6200000.000 )
        {
          /* IGNF:LAMBCC47 */
          return 9999;
        }
        else if( fabs(lat_0-48.0)<=1e-4 && fabs(lat_1-47.25)<=1e-4 && fabs(lat_2-48.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==7200000.000 )
        {
          /* IGNF:LAMBCC48 */
          return 9999;
        }
        else if( fabs(lat_0-49.0)<=1e-4 && fabs(lat_1-48.25)<=1e-4 && fabs(lat_2-49.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==8200000.000 )
        {
          /* IGNF:LAMBCC49 */
          return 9999;
        }
        else if( fabs(lat_0-50.0)<=1e-4 && fabs(lat_1-49.25)<=1e-4 && fabs(lat_2-50.75)<=1e-4 &&
                 x_0==1700000.000 && y_0==9200000.000 )
        {
          /* IGNF:LAMBCC50 */
          return 9999;
        }
        goto onError;
      }
      else if ( fabs(lon_0-166.0)<=1e-4 )
      {
        if( fabs(lat_0+21.3)<=1e-4 && fabs(lat_1+20.4)<=1e-4 && fabs(lat_2+22.2)<=1e-4 &&
                 x_0==400000.000 && y_0==300000.000 )
        {
          /* IGNF:RGNCLAM */
          return 9999;
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
      return 9999;
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
        /* IGNF:UTM20W84GUAD, IGNF:UTM20W84MART */
        if( y_0==0.000 ) return 501; /* 502 */
      }
      if( lon_0==45.0 )
      {
        /* IGNF:RGM04UTM38S */
        if( y_0>0.000 ) return 503;
      }
      if( lon_0==57.0 )
      {
        /* IGNF:RGR92UTM40S */
        if( y_0>0.000 ) return 504;
      }
      if( lon_0==-51.0 )
      {
        /* IGNF:UTM22RGFG95 */
        if( y_0==0.000 ) return 505;
      }
      if( lon_0==-177.0 )
      {
        /* IGNF:UTM01SW84 */
        if( y_0>0.000 ) return 506;
      }
      if( lon_0==-153.0 || lon_0==-147.0 || lon_0==-141.0 )
      {
        /* IGNF:RGPFUTM5S, IGNF:RGPFUTM6S, IGNF:RGPFUTM7S */
        if( y_0>0.000 ) return 508;
      }
      if( lon_0==-57.0 )
      {
        /* IGNF:RGSPM06U21 */
        if( y_0==0.000 ) return 507;
      }
      if( lon_0==159.0 || lon_0==165.0 || lon_0==171.0 )
      {
        /* IGNF:RGNCUTM57S, IGNF:RGNCUTM58S, IGNF:RGNCUTM59S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==51.0 )
      {
        /* IGNF:UTM39SW84 */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==75.0 )
      {
        /* IGNF:UTM43SW84 */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==69.0 )
      {
        /* IGNF:UTM42SW84 */
        if( y_0>0.000 ) return 9999;
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
        /* IGNF:MART38UTM20, IGNF:GUAD48UTM20 */
        /* FIXME : use OSRGetTOWGS84 to distinguish them */
        if( y_0==0.000 ) return 9999; /* 9999 */
      }
      if( lon_0==45.0 )
      {
        /* IGNF:MAYO50UTM38S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==-51.0 )
      {
        /* IGNF:CSG67UTM22 */
        if( y_0==0.000 ) return 9999;
      }
      if( lon_0==-177.0 )
      {
        /* IGNF:WALL78UTM1S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==-153.0 )
      {
        /* IGNF:TAHAAUTM05S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==-147.0 )
      {
        /* IGNF:MOOREA87U6S, IGNF:TAHI79UTM6S, */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==-141.0 )
      {
        /* IGNF:NUKU72U7S, IGNF:IGN63UTM7S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==-57.0 )
      {
        /* IGNF:STPM50UTM21 */
        if( y_0==0.000 ) return 9999;
      }
      if( lon_0==-63.0 )
      {
        /* IGNF:GUADFM49U20 */
        if( y_0==0.000 ) return 9999;
      }
      if( lon_0==165.0 )
      {
        /* IGNF:IGN72UTM58S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==51.0 )
      {
        /* IGNF:CROZ63UTM39S */
        if( y_0>0.000 ) return 9999;
      }
      if( lon_0==69.0 )
      {
        /* IGNF:KERG62UTM42S */
        if( y_0>0.000 ) return 9999;
      }
      goto onError;
    }
    goto onError;
  }

  else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CYLINDRICAL_SHERE) )
  {
    if( fabs(a-6378137.0000)<=1e-4 && lon_0==0.0 && x_0==0.0 && y_0==0.0 && k_0==a )
    {
      if( fabs(lat_0-15.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALANF */
        return 9999;
      }
      else if( fabs(lat_0+46.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALCRZ */
        return 9999;
      }
      else if( fabs(lat_0-46.5)<=1e-4 )
      {
        /* IGNF:GEOPORTALFXX */
        return 9999;
      }
      else if( fabs(lat_0-4.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALGUF */
        return 9999;
      }
      else if( fabs(lat_0+49.5)<=1e-4 )
      {
        /* IGNF:GEOPORTALKER */
        return 9999;
      }
      else if( fabs(lat_0+12.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALMYT */
        return 9999;
      }
      else if( fabs(lat_0+22.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALNCL */
        return 9999;
      }
      else if( fabs(lat_0+15.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALPYF */
        return 9999;
      }
      else if( fabs(lat_0+21.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALREU */
        return 9999;
      }
      else if( fabs(lat_0-47.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALSPM */
        return 9999;
      }
      else if( fabs(lat_0+14.0)<=1e-4 )
      {
        /* IGNF:GEOPORTALWLF */
        return 9999;
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
      return 9999;
    }
    goto onError;
  }

onError:
  CPLError(CE_Warning, CPLE_IllegalArg,
           "Unknown spatial reference system '%s'.\n"
           "Geoconcept only supports projections.\n"
           "This driver supports only few of them.\n",
           pszProj4);
  CPLFree(pszProj4);
  return -1;
}/* OGRSpatialReference2SysCoord_GCSRS */
