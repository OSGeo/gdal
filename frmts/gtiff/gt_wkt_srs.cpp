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
 * Revision 1.8  2000/01/06 19:45:22  warmerda
 * added special case for writing UTM projections
 *
 * Revision 1.7  1999/12/07 17:50:17  warmerda
 * Fixed bug in datum handling.
 *
 * Revision 1.6  1999/10/29 17:28:43  warmerda
 * OGC to GeoTIFF conversion
 *
 * Revision 1.5  1999/10/01 14:50:37  warmerda
 * added newzealandmapgrid and gridskewangle
 *
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
#include "cpl_csv.h"
#include "geo_normalize.h"
#include "geovalues.h"
#include "ogr_spatialref.h"
#include "cpl_serv.h"

CPL_C_START
char *  GTIFGetOGISDefn( GTIFDefn * );
int     GTIFSetFromOGISDefn( GTIF *, const char * );
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

    if( pszDatumName != NULL )
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
                         adfParm[2], adfParm[3],
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

          case CT_NewZealandMapGrid:
            oSRS.SetNZMG( adfParm[0], adfParm[1],
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

/************************************************************************/
/*                     OGCDatumName2EPSGDatumCode()                     */
/************************************************************************/

static int OGCDatumName2EPSGDatumCode( const char * pszOGCName )

{
    FILE	*fp;
    char	**papszTokens;
    int		nReturn = KvUserDefined;
    
/* -------------------------------------------------------------------- */
/*      Open the table if possible.                                     */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( CSVFilename("geod_datum.csv"), "r" );
    if( fp == NULL )
        return nReturn;

/* -------------------------------------------------------------------- */
/*	Discard the first line with field names.			*/
/* -------------------------------------------------------------------- */
    CSLDestroy( CSVReadParseLine( fp ) );

/* -------------------------------------------------------------------- */
/*      Read lines looking for our datum.                               */
/* -------------------------------------------------------------------- */
    for( papszTokens = CSVReadParseLine( fp );
         CSLCount(papszTokens) > 2 && nReturn == KvUserDefined;
         papszTokens = CSVReadParseLine( fp ) )
    {
        WKTMassageDatum( papszTokens + 1 );

        if( EQUAL(papszTokens[1], pszOGCName) )
            nReturn = atoi(papszTokens[0]);

        CSLDestroy( papszTokens );
    }

    CSLDestroy( papszTokens );
    VSIFClose( fp );

    return nReturn;
}

/************************************************************************/
/*                        GTIFSetFromOGISDefn()                         */
/*                                                                      */
/*      Write GeoTIFF projection tags from an OGC WKT definition.       */
/************************************************************************/

int GTIFSetFromOGISDefn( GTIF * psGTIF, const char *pszOGCWKT )

{
    OGRSpatialReference *poSRS;
    int		nPCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Create an OGRSpatialReference object corresponding to the       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();

    if( poSRS->importFromWkt((char **) &pszOGCWKT) != OGRERR_NONE )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the Datum so we can special case a few PCS codes.           */
/* -------------------------------------------------------------------- */
    int		nDatum;

    if( poSRS->GetAttrValue("DATUM") != NULL )
        nDatum = OGCDatumName2EPSGDatumCode( poSRS->GetAttrValue("DATUM") );
    else
        nDatum = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Handle the projection transformation.                           */
/* -------------------------------------------------------------------- */
    const char *pszProjection = poSRS->GetAttrValue( "PROJECTION" );

    if( pszProjection == NULL )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeGeographic);
    }

    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_AlbersEqualArea );

        GTIFKeySet(psGTIF, ProjStdParallelGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );

        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }

    else if( poSRS->GetUTMZone() != 0 )
    {
        int		bNorth, nZone, nProjection;

	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);

        nZone = poSRS->GetUTMZone( &bNorth );

        if( nDatum == Datum_North_American_Datum_1983 && nZone >= 3
            && nZone <= 22 && bNorth )
        {
            nPCS = 26900 + nZone;

            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
        else if( nDatum == Datum_North_American_Datum_1927 && nZone >= 3
            && nZone <= 22 && bNorth )
        {
            nPCS = 26700 + nZone;

            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
        else if( nDatum == Datum_WGS84 )
        {
            if( bNorth )
                nPCS = 32600 + nZone;
            else
                nPCS = 32700 + nZone;

            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
        else
        {
            if( bNorth )
                nProjection = 16000 + nZone;
            else
                nProjection = 16100 + nZone;

        
            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                       KvUserDefined );
            
            GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1, nProjection );
        }
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_TransverseMercator );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
    }
    
/* -------------------------------------------------------------------- */
/*      Write linear units information.                                 */
/* -------------------------------------------------------------------- */
    
    
/* -------------------------------------------------------------------- */
/*	Try to identify the datum, scanning the EPSG datum file for	*/
/*	a match.							*/    
/* -------------------------------------------------------------------- */
    if( nPCS == KvUserDefined )
    {
        if( nDatum != KvUserDefined )
        {
            GTIFKeySet( psGTIF, GeogGeodeticDatumGeoKey, TYPE_SHORT,
                        1, nDatum );
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Couldn't translate `%s' to a GeoTIFF datum.\n",
                      poSRS->GetAttrValue("DATUM") );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the GCS information.                                      */
/* -------------------------------------------------------------------- */
    

    return TRUE;
}
