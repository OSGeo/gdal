/******************************************************************************
 * $Id$
 *
 * Project:  libgeotiff
 * Purpose:  Code to convert a normalized GeoTIFF definition into a PROJ.4
 *           (OGDI) compatible projection string.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/03/02 21:11:30  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "geotiff.h"
#include "geovalues.h"
#include "geo_normalize.h"

/************************************************************************/
/*                          GTIFGetProj4Defn()                          */
/************************************************************************/

char * GTIFGetProj4Defn( GTIFDefn * psDefn )

{
    char	szProjection[512];
    char	szUnits[24];

    szProjection[0] = '\0';
    
/* ==================================================================== */
/*      Translate the units of measure.                                 */
/*                                                                      */
/*      Note that even with a +units, or +to_meter in effect, it is     */
/*      still assumed that all the projection parameters are in         */
/*      meters.                                                         */
/* ==================================================================== */
    if( psDefn->UOMLength == Linear_Meter )
    {
        strcpy( szUnits, "+units=m " ); 
    }
    else if( psDefn->UOMLength == Linear_Foot )
    {
        strcpy( szUnits, "+units=foot " );
    }
    else if( psDefn->UOMLength == Linear_Foot_US_Survey )
    {
        strcpy( szUnits, "+units=us-ft " );
    }
    else if( psDefn->UOMLength == Linear_Foot_Indian )
    {
        strcpy( szUnits, "+units=ind-ft " );
    }
    else if( psDefn->UOMLength == Linear_Link )
    {
        strcpy( szUnits, "+units=link " );
    }
    else if( psDefn->UOMLength == Linear_Yard_Indian)
    {
        strcpy( szUnits, "+units=ind-yd " );
    }
    else if( psDefn->UOMLength == Linear_Fathom )
    {
        strcpy( szUnits, "+units=fath " );
    }
    else if( psDefn->UOMLength == Linear_Mile_International_Nautical )
    {
        strcpy( szUnits, "+units=kmi " );
    }
    else
    {
        sprintf( szUnits, "+to_meter=%.10f", psDefn->UOMLengthInMeters );
    }
    
/* ==================================================================== */
/*      Handle general projection methods.                              */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    if( psDefn->CTProjection == CT_TransverseMercator )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=tmerc +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Mercator							*/
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Mercator )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=merc +lat_ts=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Oblique Stereographic - Should this really map onto             */
/*      Stereographic?                                                  */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_ObliqueStereographic )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Stereographic                                                   */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Stereographic )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_PolarStereographic )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=stere +lat_0=%.9f +lon_0=%.9f +k=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Equirectangular                                                 */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Equirectangular )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=eqc +lat_ts=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Gnomonic                                                        */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Gnomonic )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=gnom +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Orthographic                                                    */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Orthographic )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=ortho +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Lambert Azimuthal Equal Area                                    */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_LambertAzimEqualArea )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=laea +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Azimuthal Equidistant                                           */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_AzimuthalEquidistant )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=aeqd +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Miller Cylindrical                                              */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_MillerCylindrical )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=mill +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      Polyconic                                                       */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Polyconic )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=poly +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* -------------------------------------------------------------------- */
/*      AlbersEqualArea                                                 */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_AlbersEqualArea )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=aea +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[2],
                 psDefn->ProjParm[3],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      Robinson                                                        */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Robinson )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=robin +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      VanDerGrinten                                                   */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_VanDerGrinten )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=vandg +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      Sinusoidal                                                      */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Sinusoidal )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=sinu +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      LambertConfConic_2SP                                            */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_LambertConfConic_2SP )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=lcc +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[2],
                 psDefn->ProjParm[3],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      LambertConfConic_1SP                                            */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_LambertConfConic_2SP )
    {
        /* this appears to be an unsupported formulation with PROJ.4 */
    }
    
/* -------------------------------------------------------------------- */
/*      NewZealandMapGrid                                               */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_NewZealandMapGrid )
    {
        /* this appears to be an unsupported formulation with PROJ.4 */
    }
    
/* -------------------------------------------------------------------- */
/*      Transverse Mercator - south oriented.                           */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_TransvMercator_SouthOriented )
    {
        /* this appears to be an unsupported formulation with PROJ.4 */
    }
    
/* -------------------------------------------------------------------- */
/*      ObliqueMercator (Hotine)                                        */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_ObliqueMercator )
    {
        /* not clear how ProjParm[3] - angle from rectified to skewed grid -
           should be applied ... see the +not_rot flag for PROJ.4.
           Just ignoring for now. */

        sprintf( szProjection+strlen(szProjection),
                 "+proj=omerc +lat_0=%.9f +lonc=%.9f +alpha=%.9f"
                 " +k=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[2],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }

/* ==================================================================== */
/*      Handle ellipsoid information.                                   */
/* ==================================================================== */
    if( psDefn->Ellipsoid == Ellipse_WGS_84 )
        strcat( szProjection, "+ellps=WGS84 " );
    else if( psDefn->Ellipsoid == Ellipse_Clarke_1866 )
        strcat( szProjection, "+ellps=clrk66 " );
    else if( psDefn->Ellipsoid == Ellipse_Clarke_1880 )
        strcat( szProjection, "+ellps=clrk80 " );
    else if( psDefn->Ellipsoid == Ellipse_GRS_1980 )
        strcat( szProjection, "+ellps=GRS80 " );
    else
    {
        if( psDefn->SemiMajor != 0.0 && psDefn->SemiMinor != 0.0 )
        {
            sprintf( szProjection+strlen(szProjection),
                     "+a=%.3f +b=%.3f ",
                     psDefn->SemiMajor,
                     psDefn->SemiMinor );
        }
    }

    strcat( szProjection, szUnits );

    return( CPLStrdup( szProjection ) );
}

