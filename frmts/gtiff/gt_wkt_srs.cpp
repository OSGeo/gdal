/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.  This code is
 *           deliberately GDAL free, and it is intended to be moved into
 *           libgeotiff someday if possible.
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
 * Revision 1.4  1999/09/15 20:33:33  warmerda
 * Handle UOMAngle properly.  Translate PM and angular projection parms back
 * into the UOMAngle units from degrees.
 *
 * Revision 1.3  1999/09/10 13:41:34  warmerda
 * Handle OGC datum name exceptions, and massage projparm into proj units
 *
 * Revision 1.2  1999/09/09 13:56:23  warmerda
 * made some changes to order WKT like Adams
 *
 * Revision 1.1  1999/07/29 18:02:21  warmerda
 * New
 *
 */

#include "cpl_port.h"
#include "geo_normalize.h"
#include "geovalues.h"
#include "ogr_spatialref.h"
#include "cpl_serv.h"

CPL_C_START
char *  GTIFGetOGISDefn( GTIFDefn * );
CPL_C_END

static char *papszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    NULL
};

/************************************************************************/
/*                          WKTMassageDatum()                           */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/************************************************************************/

static void WKTMassageDatum( char ** ppszDatum )

{
    int		i, j;
    char	*pszDatum = *ppszDatum;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
    {
        if( EQUAL(*ppszDatum,papszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( papszDatumEquiv[i+1] );
            break;
        }
    }
}

/************************************************************************/
/*                          GTIFGetOGISDefn()                           */
/************************************************************************/

char *GTIFGetOGISDefn( GTIFDefn * psDefn )

{
    OGRSpatialReference	oSRS;
    
/* -------------------------------------------------------------------- */
/*      If this is a projected SRS we set the PROJCS keyword first      */
/*      to ensure that the GEOGCS will be a child.                      */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected )
    {
        char	*pszPCSName = "unnamed";

        GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL, NULL );
        oSRS.SetNode( "PROJCS", pszPCSName );
    }
    
/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char	*pszGeogName = NULL;
    char	*pszDatumName = NULL;
    char	*pszPMName = NULL;
    char	*pszSpheroidName = NULL;
    char	*pszAngularUnits = NULL;
    double	dfInvFlattening;
    
    GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL );
    GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
    GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
    GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
    GTIFGetUOMAngleInfo( psDefn->UOMAngle, &pszAngularUnits, NULL );
    if( pszAngularUnits == NULL )
        pszAngularUnits = CPLStrdup("unknown");

    WKTMassageDatum( &pszDatumName );

    if( (psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
        || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 )
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);
    else
        dfInvFlattening = 0.0; /* special flag for infinity */
    
    oSRS.SetGeogCS( pszGeogName, pszDatumName, pszSpheroidName,
                    psDefn->SemiMajor, dfInvFlattening,
                    pszPMName,
                    psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                    pszAngularUnits,
                    psDefn->UOMAngleInDegrees * 0.0174532925199433 );

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    CPLFree( pszPMName );
    CPLFree( pszSpheroidName );
    CPLFree( pszAngularUnits );
        
/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected )
    {
/* -------------------------------------------------------------------- */
/*      Make a local copy of parms, and convert back into the           */
/*      angular units of the GEOGCS and the linear units of the         */
/*      projection.                                                     */
/* -------------------------------------------------------------------- */
        double		adfParm[10];
        int		i;

        for( i = 0; i < MIN(10,psDefn->nParms); i++ )
            adfParm[i] = psDefn->ProjParm[i];

        adfParm[0] /= psDefn->UOMAngleInDegrees;
        adfParm[1] /= psDefn->UOMAngleInDegrees;
        adfParm[2] /= psDefn->UOMAngleInDegrees;
        adfParm[3] /= psDefn->UOMAngleInDegrees;
        
        adfParm[5] /= psDefn->UOMLengthInMeters;
        adfParm[6] /= psDefn->UOMLengthInMeters;
        
/* -------------------------------------------------------------------- */
/*      Translation the fundamental projection.                         */
/* -------------------------------------------------------------------- */
        switch( psDefn->CTProjection )
        {
          case CT_TransverseMercator:
            oSRS.SetTM( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_TransvMercator_SouthOriented:
            oSRS.SetTMSO( adfParm[0], adfParm[1],
                          adfParm[4],
                          adfParm[5], adfParm[6] );
            break;

          case CT_Mercator:
            oSRS.SetMercator( adfParm[0], adfParm[1],
                              adfParm[4],
                              adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueStereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_Stereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueMercator: /* hotine */
            oSRS.SetHOM( adfParm[0], adfParm[1],
                         adfParm[2], 0.0,
                         adfParm[4],
                         adfParm[5], adfParm[6] );
            break;
        
          case CT_CassiniSoldner:
            oSRS.SetCS( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Polyconic:
            oSRS.SetPolyconic( adfParm[0], adfParm[1],
                               adfParm[5], adfParm[6] );
            break;

          case CT_AzimuthalEquidistant:
            oSRS.SetAE( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_MillerCylindrical:
            oSRS.SetMC( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Equirectangular:
            oSRS.SetEquirectangular( adfParm[0], adfParm[1],
                                     adfParm[5], adfParm[6] );
            break;
        
          case CT_Gnomonic:
            oSRS.SetGnomonic( adfParm[0], adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertAzimEqualArea:
            oSRS.SetLAEA( adfParm[0], adfParm[1],
                          adfParm[5], adfParm[6] );
            break;
        
          case CT_Orthographic:
            oSRS.SetOrthographic( adfParm[0], adfParm[1],
                                  adfParm[5], adfParm[6] );
            break;
        
          case CT_Robinson:
            oSRS.SetRobinson( adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_Sinusoidal:
            oSRS.SetSinusoidal( adfParm[1],
                                adfParm[5], adfParm[6] );
            break;
        
          case CT_VanDerGrinten:
            oSRS.SetVDG( adfParm[1],
                         adfParm[5], adfParm[6] );
            break;

          case CT_PolarStereographic:
            oSRS.SetPS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertConfConic_2SP:
            oSRS.SetLCC( adfParm[0], adfParm[1],
                         adfParm[2], adfParm[3],
                         adfParm[5], adfParm[6] );
            break;

          case CT_LambertConfConic_1SP:
            oSRS.SetLCC1SP( adfParm[0], adfParm[1],
                            adfParm[4],
                            adfParm[5], adfParm[6] );
            break;
        
          case CT_AlbersEqualArea:
            oSRS.SetACEA( adfParm[0], adfParm[1],
                          adfParm[2], adfParm[3],
                          adfParm[5], adfParm[6] );
            break;
        }

/* -------------------------------------------------------------------- */
/*      Set projection units.                                           */
/* -------------------------------------------------------------------- */
        char	*pszUnitsName = NULL;
        
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );

        if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
            oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
        else
            oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );
    }
    
/* -------------------------------------------------------------------- */
/*      Return the WKT serialization of the object.                     */
/* -------------------------------------------------------------------- */
    char	*pszWKT;

    if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
        return pszWKT;
    else
        return NULL;
}

