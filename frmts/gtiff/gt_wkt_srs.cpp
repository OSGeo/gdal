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
    double	dfInvFlattening;
    int		i;
    
    GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL );
    GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
    GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
    GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );

    for( i = 0; pszDatumName[i] != '\0'; i++ )
    {
        if( pszDatumName[i] == ' ' )
            pszDatumName[i] = '_';
    }

    if( (psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
        || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 )
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);
    else
        dfInvFlattening = 100000.0; /* nearly a sphere */
    
    oSRS.SetGeogCS( pszGeogName, pszDatumName, pszSpheroidName,
                    psDefn->SemiMajor, dfInvFlattening,
                    pszPMName, psDefn->PMLongToGreenwich );

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    CPLFree( pszPMName );
    CPLFree( pszSpheroidName );
        
/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected )
    {
/* -------------------------------------------------------------------- */
/*      Translation the fundamental projection.                         */
/* -------------------------------------------------------------------- */
        switch( psDefn->CTProjection )
        {
          case CT_TransverseMercator:
            oSRS.SetTM( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[4],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_TransvMercator_SouthOriented:
            oSRS.SetTMSO( psDefn->ProjParm[0], psDefn->ProjParm[1],
                          psDefn->ProjParm[4],
                          psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_Mercator:
            oSRS.SetMercator( psDefn->ProjParm[0], psDefn->ProjParm[1],
                              psDefn->ProjParm[4],
                              psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_ObliqueStereographic:
            oSRS.SetOS( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[4],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_Stereographic:
            oSRS.SetOS( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[4],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_ObliqueMercator: /* hotine */
            oSRS.SetHOM( psDefn->ProjParm[0], psDefn->ProjParm[1],
                         psDefn->ProjParm[2], 0.0,
                         psDefn->ProjParm[4],
                         psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_CassiniSoldner:
            oSRS.SetCS( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Polyconic:
            oSRS.SetPolyconic( psDefn->ProjParm[0], psDefn->ProjParm[1],
                               psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_AzimuthalEquidistant:
            oSRS.SetAE( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_MillerCylindrical:
            oSRS.SetMC( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Equirectangular:
            oSRS.SetEquirectangular( psDefn->ProjParm[0], psDefn->ProjParm[1],
                                     psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Gnomonic:
            oSRS.SetGnomonic( psDefn->ProjParm[0], psDefn->ProjParm[1],
                              psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_LambertAzimEqualArea:
            oSRS.SetLAEA( psDefn->ProjParm[0], psDefn->ProjParm[1],
                          psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Orthographic:
            oSRS.SetOrthographic( psDefn->ProjParm[0], psDefn->ProjParm[1],
                                  psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Robinson:
            oSRS.SetRobinson( psDefn->ProjParm[1],
                              psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_Sinusoidal:
            oSRS.SetSinusoidal( psDefn->ProjParm[1],
                                psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_VanDerGrinten:
            oSRS.SetVDG( psDefn->ProjParm[1],
                         psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;

          case CT_PolarStereographic:
            oSRS.SetPS( psDefn->ProjParm[0], psDefn->ProjParm[1],
                        psDefn->ProjParm[4],
                        psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_LambertConfConic_2SP:
            oSRS.SetLCC( psDefn->ProjParm[0], psDefn->ProjParm[1],
                         psDefn->ProjParm[2], psDefn->ProjParm[3],
                         psDefn->ProjParm[5], psDefn->ProjParm[6] );
            break;
        
          case CT_AlbersEqualArea:
            oSRS.SetACEA( psDefn->ProjParm[0], psDefn->ProjParm[1],
                          psDefn->ProjParm[2], psDefn->ProjParm[3],
                          psDefn->ProjParm[5], psDefn->ProjParm[6] );
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

