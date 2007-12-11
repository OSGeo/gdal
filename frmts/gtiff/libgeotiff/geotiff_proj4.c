/******************************************************************************
 * $Id: geotiff_proj4.c,v 1.23 2007/03/13 18:04:33 fwarmerdam Exp $
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
 * $Log: geotiff_proj4.c,v $
 * Revision 1.23  2007/03/13 18:04:33  fwarmerdam
 * added new zealand map grid support per bug 1519
 *
 * Revision 1.22  2005/03/04 04:32:37  fwarmerdam
 * added cylindricalequalarea support
 *
 * Revision 1.21  2003/08/21 18:42:39  warmerda
 * fixed support for ModelTypeGeographic as per email from Young Su, Cha
 *
 * Revision 1.20  2003/07/08 17:31:30  warmerda
 * cleanup various warnings
 *
 * Revision 1.19  2002/11/29 20:57:09  warmerda
 * added LCC1SP mapping
 *
 * Revision 1.18  2002/07/09 14:47:53  warmerda
 * fixed translation of polar stereographic
 *
 * Revision 1.17  2001/11/23 19:53:56  warmerda
 * free PROJ.4 definitions after use
 *
 * Revision 1.16  2000/12/05 19:21:45  warmerda
 * added cassini support
 *
 * Revision 1.15  2000/12/05 17:44:41  warmerda
 * Use +R_A for Miller and VanDerGrinten
 *
 * Revision 1.14  2000/10/13 18:06:51  warmerda
 * added econic support for PROJ.4 translation
 *
 * Revision 1.13  2000/09/15 19:30:48  warmerda
 * *** empty log message ***
 *
 * Revision 1.12  2000/09/15 18:21:07  warmerda
 * Fixed order of parameters for LCC 2SP.  When parameters
 * were read from EPSG CSV files the standard parallels and origin
 * were mixed up.  This affects alot of state plane zones!
 *
 * Revision 1.11  2000/06/06 17:39:45  warmerda
 * Modify to work with projUV version of library.
 *
 * Revision 1.10  1999/07/06 15:05:51  warmerda
 * Fixed up LCC_1SP notes.
 *
 * Revision 1.9  1999/05/04 16:24:49  warmerda
 * Fixed projection string formating with zones.
 *
 * Revision 1.8  1999/05/04 12:27:01  geotiff
 * only emit proj unsupported warning if DEBUG defined
 *
 * Revision 1.7  1999/05/04 03:14:59  warmerda
 * fixed use of foot instead of ft for units
 *
 * Revision 1.6  1999/05/03 17:50:31  warmerda
 * avoid warnings on IRIX
 *
 * Revision 1.5  1999/04/29 23:02:24  warmerda
 * added mapsys utm test.
 *
 * Revision 1.4  1999/03/18 21:35:42  geotiff
 * Added reprojection functions
 *
 * Revision 1.3  1999/03/10 18:11:17  geotiff
 * Removed comment about this not being the master ... now it is.
 *
 * Revision 1.2  1999/03/10 18:10:27  geotiff
 * Avoid use of cpl_serv.h and CPLStrdup().
 *
 * Revision 1.1  1999/03/10 15:20:43  geotiff
 * New
 *
 */

#include "cpl_serv.h"
#include "geotiff.h"
#include "geo_normalize.h"
#include "geovalues.h"

/************************************************************************/
/*                          GTIFGetProj4Defn()                          */
/************************************************************************/

char * GTIFGetProj4Defn( GTIFDefn * psDefn )

{
    char	szProjection[512];
    char	szUnits[24];
    double      dfFalseEasting, dfFalseNorthing;

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
        strcpy( szUnits, "+units=ft " );
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

/* -------------------------------------------------------------------- */
/*      false easting and northing are in meters and that is what       */
/*      PROJ.4 wants regardless of the linear units.                    */
/* -------------------------------------------------------------------- */
    dfFalseEasting = psDefn->ProjParm[5];
    dfFalseNorthing = psDefn->ProjParm[6];
    
/* ==================================================================== */
/*      Handle general projection methods.                              */
/* ==================================================================== */
 
/* -------------------------------------------------------------------- */
/*      Geographic.                                                     */
/* -------------------------------------------------------------------- */
    if(psDefn->Model==ModelTypeGeographic)
    {
        sprintf(szProjection+strlen(szProjection),"+proj=latlong ");
        
    }
 
/* -------------------------------------------------------------------- */
/*      UTM - special case override on transverse mercator so things    */
/*      will be more meaningful to the user.                            */
/* -------------------------------------------------------------------- */
    else if( psDefn->MapSys == MapSys_UTM_North )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=utm +zone=%d ",
                 psDefn->Zone );
    }
    
/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_TransverseMercator )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=tmerc +lat_0=%.9f +lon_0=%.9f +k=%f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
    }

/* -------------------------------------------------------------------- */
/*      Cassini/Soldner                                                 */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_CassiniSoldner )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=cass +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
    }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_PolarStereographic )
    {
        if( psDefn->ProjParm[0] > 0.0 )
            sprintf( szProjection+strlen(szProjection),
                     "+proj=stere +lat_0=90 +lat_ts=%.9f +lon_0=%.9f "
                     "+k=%.9f +x_0=%.3f +y_0=%.3f ",
                     psDefn->ProjParm[0],
                     psDefn->ProjParm[1],
                     psDefn->ProjParm[4],
                     dfFalseEasting,
                     dfFalseNorthing );
        else
            sprintf( szProjection+strlen(szProjection),
                     "+proj=stere +lat_0=-90 +lat_ts=%.9f +lon_0=%.9f "
                     "+k=%.9f +x_0=%.3f +y_0=%.3f ",
                     psDefn->ProjParm[0],
                     psDefn->ProjParm[1],
                     psDefn->ProjParm[4],
                     dfFalseEasting,
                     dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
    }

/* -------------------------------------------------------------------- */
/*      Miller Cylindrical                                              */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_MillerCylindrical )
    {
        sprintf( szProjection+strlen(szProjection),
           "+proj=mill +lat_0=%.9f +lon_0=%.9f +x_0=%.3f +y_0=%.3f +R_A ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
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
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      EquidistantConic                                                */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_EquidistantConic )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=eqdc +lat_1=%.9f +lat_2=%.9f +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[2],
                 psDefn->ProjParm[3],
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      Robinson                                                        */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Robinson )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=robin +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[1],
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      VanDerGrinten                                                   */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_VanDerGrinten )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=vandg +lon_0=%.9f +x_0=%.3f +y_0=%.3f +R_A ",
                 psDefn->ProjParm[1],
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      Sinusoidal                                                      */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_Sinusoidal )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=sinu +lon_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[1],
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      LambertConfConic_2SP                                            */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_LambertConfConic_2SP )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=lcc +lat_0=%.9f +lon_0=%.9f +lat_1=%.9f +lat_2=%.9f "
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[2],
                 psDefn->ProjParm[3],
                 dfFalseEasting,
                 dfFalseNorthing );
    }
    
/* -------------------------------------------------------------------- */
/*      LambertConfConic_1SP                                            */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_LambertConfConic_1SP )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=lcc +lat_0=%.9f +lat_1=%.9f +lon_0=%.9f"
                 " +k_0=%.9f +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[4],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      CT_CylindricalEqualArea                                         */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_CylindricalEqualArea )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=cea +lat_ts=%.9f +lon_0=%.9f "
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
    }
    
/* -------------------------------------------------------------------- */
/*      NewZealandMapGrid                                               */
/* -------------------------------------------------------------------- */
    else if( psDefn->CTProjection == CT_NewZealandMapGrid )
    {
        sprintf( szProjection+strlen(szProjection),
                 "+proj=nzmg +lat_0=%.9f +lon_0=%.9f"
                 " +x_0=%.3f +y_0=%.3f ",
                 psDefn->ProjParm[0],
                 psDefn->ProjParm[1],
                 psDefn->ProjParm[5],
                 psDefn->ProjParm[6] );
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

    return( strdup( szProjection ) );
}

#if !defined(HAVE_LIBPROJ) || !defined(HAVE_PROJECTS_H)

int GTIFProj4ToLatLong( GTIFDefn * psDefn, int nPoints,
                        double *padfX, double *padfY )
{
    (void) psDefn;
    (void) nPoints;
    (void) padfX;
    (void) padfY;
#ifdef DEBUG    
    fprintf( stderr,
             "GTIFProj4ToLatLong() - PROJ.4 support not compiled in.\n" );
#endif    
    return FALSE;
}

int GTIFProj4FromLatLong( GTIFDefn * psDefn, int nPoints,
                          double *padfX, double *padfY )
{
    (void) psDefn;
    (void) nPoints;
    (void) padfX;
    (void) padfY;
#ifdef DEBUG    
    fprintf( stderr,
             "GTIFProj4FromLatLong() - PROJ.4 support not compiled in.\n" );
#endif    
    return FALSE;
}
#else

#include "projects.h"

#ifdef USE_PROJUV
#  define UV projUV
#endif

/************************************************************************/
/*                        GTIFProj4FromLatLong()                        */
/*                                                                      */
/*      Convert lat/long values to projected coordinate for a           */
/*      particular definition.                                          */
/************************************************************************/

int GTIFProj4FromLatLong( GTIFDefn * psDefn, int nPoints,
                          double *padfX, double *padfY )

{
    char	*pszProjection, **papszArgs;
    PJ		*psPJ;
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Get a projection definition.                                    */
/* -------------------------------------------------------------------- */
    pszProjection = GTIFGetProj4Defn( psDefn );

    if( pszProjection == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Parse into tokens for pj_init(), and initialize the projection. */
/* -------------------------------------------------------------------- */
    
    papszArgs = CSLTokenizeStringComplex( pszProjection, " +", TRUE, FALSE );
    free( pszProjection );

    psPJ = pj_init( CSLCount(papszArgs), papszArgs );

    CSLDestroy( papszArgs );

    if( psPJ == NULL )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Process each of the points.                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nPoints; i++ )
    {
        UV	sUV;

        sUV.u = padfX[i] * DEG_TO_RAD;
        sUV.v = padfY[i] * DEG_TO_RAD;

        sUV = pj_fwd( sUV, psPJ );

        padfX[i] = sUV.u;
        padfY[i] = sUV.v;
    }

    pj_free( psPJ );

    return TRUE;
}

/************************************************************************/
/*                         GTIFProj4ToLatLong()                         */
/*                                                                      */
/*      Convert projection coordinates to lat/long for a particular     */
/*      definition.                                                     */
/************************************************************************/

int GTIFProj4ToLatLong( GTIFDefn * psDefn, int nPoints,
                        double *padfX, double *padfY )

{
    char	*pszProjection, **papszArgs;
    PJ		*psPJ;
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Get a projection definition.                                    */
/* -------------------------------------------------------------------- */
    pszProjection = GTIFGetProj4Defn( psDefn );

    if( pszProjection == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Parse into tokens for pj_init(), and initialize the projection. */
/* -------------------------------------------------------------------- */
    
    papszArgs = CSLTokenizeStringComplex( pszProjection, " +", TRUE, FALSE );
    free( pszProjection );

    psPJ = pj_init( CSLCount(papszArgs), papszArgs );

    CSLDestroy( papszArgs );

    if( psPJ == NULL )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Process each of the points.                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nPoints; i++ )
    {
        UV	sUV;

        sUV.u = padfX[i];
        sUV.v = padfY[i];

        sUV = pj_inv( sUV, psPJ );

        padfX[i] = sUV.u * RAD_TO_DEG;
        padfY[i] = sUV.v * RAD_TO_DEG;
    }

    pj_free( psPJ );

    return TRUE;
}


#endif /* has projects.h and -lproj */

