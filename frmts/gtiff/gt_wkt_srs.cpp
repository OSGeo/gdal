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
 * Revision 1.50  2005/02/07 13:30:30  dron
 * Memory leak removed.
 *
 * Revision 1.49  2004/10/18 21:12:44  fwarmerdam
 * Always emit angular units as degrees.
 *
 * Revision 1.48  2004/07/10 05:02:57  warmerda
 * Fixed improper projection parameters for false easting/northing for LCC.
 *
 * Revision 1.47  2004/04/29 19:58:43  warmerda
 * export GTIFGetOGISDefn, and GTIFSetFromOGISDefn
 *
 * Revision 1.46  2004/04/29 18:10:35  warmerda
 * try not to crash if GTIF is NULL
 *
 * Revision 1.45  2004/04/21 13:59:15  warmerda
 * try to preserve PROJCS and GEOGCS names in citations
 *
 * Revision 1.44  2004/03/18 09:58:07  dron
 * Use auxiliary functions from the libgeotiff instead of the ones from CPL.
 *
 * Revision 1.43  2003/11/25 15:26:08  warmerda
 * dont use GCS or PCS values that are out of range
 *
 * Revision 1.42  2003/06/23 14:09:52  warmerda
 * Use gdal_datum.csv and fallback to datum.csv.
 *
 * Revision 1.41  2003/06/20 19:33:54  warmerda
 * fixed use of const string as non-const argument - bugzilla 350
 *
 * Revision 1.40  2003/06/20 02:12:35  warmerda
 * set photometric tag
 *
 * Revision 1.39  2003/06/19 19:39:09  warmerda
 * when preparing a coordinate-system inmemory GeoTIFF also write image data
 *
 * Revision 1.38  2003/03/26 22:13:53  warmerda
 * Changed geod_datum.csv to datum.csv.
 *
 * Revision 1.37  2003/03/18 18:33:37  warmerda
 * fixed memory leaks
 *
 * Revision 1.36  2003/02/18 16:39:31  warmerda
 * Ensure GeographicTypeGeoKey, GeogGeodeticDatumGeoKey and GeogEllipsoidKey
 * are written out as KvUserDefined when they are not know for compatibility
 * with Erdas Imagine.
 *
 * Revision 1.35  2003/01/08 18:24:03  warmerda
 * ensure order is fixed up
 *
 * Revision 1.34  2002/12/03 03:32:06  warmerda
 * fixed GTIFGetPCSInfo arguments
 *
 * Revision 1.33  2002/11/30 17:36:41  warmerda
 * added support for writing ellipsoid and citation
 *
 * Revision 1.32  2002/11/30 16:44:43  warmerda
 * fixed to set authority info on various nodes
 *
 * Revision 1.31  2002/11/25 16:32:38  warmerda
 * improved support for using authority PCS/GCS/Datum info when writing
 *
 * Revision 1.30  2002/11/25 05:34:10  warmerda
 * implemented support for checking and writing linear units
 *
 * Revision 1.29  2002/11/24 02:44:42  warmerda
 * cleanup poSRS leak when writing geotiff info
 *
 * Revision 1.28  2002/11/23 18:56:56  warmerda
 * fixed GEOTIEPOINTS setting for mem dataset
 *
 * Revision 1.27  2002/10/10 21:06:08  warmerda
 * fine tuned the WktToMemBuf function ... works now
 *
 * Revision 1.26  2002/10/08 23:01:07  warmerda
 * Added code for building/consuming simple memory geotiff files for JPEG2000
 *
 * Revision 1.25  2002/09/25 13:08:57  warmerda
 * Fixed free of static PCS name as per bugzilla 207.
 *
 * Revision 1.24  2002/09/04 06:46:30  warmerda
 * fixed two memory leaks
 *
 * Revision 1.23  2002/02/07 20:29:23  warmerda
 * fixed bug if no DATUM node available
 *
 * Revision 1.22  2001/11/02 22:28:22  warmerda
 * fixed free of static spheroid name
 *
 * Revision 1.21  2001/10/12 15:06:05  warmerda
 * various build improvements to avoid internal/external conflicts
 *
 * Revision 1.20  2001/10/09 17:32:25  warmerda
 * Recognise "WGS 84" as a datum name.
 *
 * Revision 1.19  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.18  2000/12/15 14:48:18  warmerda
 * fixed handling of nongeographic/projected model types
 *
 * Revision 1.17  2000/12/15 13:57:08  warmerda
 * fixed handling of non-geographic/projected model types for Geotiff
 *
 * Revision 1.16  2000/12/05 23:04:12  warmerda
 * added write support for lots of projections
 *
 * Revision 1.15  2000/10/13 20:55:54  warmerda
 * improved support for writing GCS codes, and hardcode common datums
 *
 * Revision 1.14  2000/10/13 18:03:32  warmerda
 * econic fix
 *
 * Revision 1.13  2000/06/14 20:57:48  warmerda
 * Don't return a WKT coordinate system if Model wasn't defined.
 *
 * Revision 1.12  2000/06/09 21:19:05  warmerda
 * Set GTRasterTypeGeoKey.
 *
 * Revision 1.11  2000/06/09 13:26:47  warmerda
 * default spheroid information to WGS84 if we don't have any
 *
 * Revision 1.10  2000/02/14 20:21:08  warmerda
 * avoid reporting error for unknown datum
 *
 * Revision 1.9  2000/01/31 16:25:17  warmerda
 * handle zero semimajor gracefully
 *
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

#include "cpl_serv.h"
#include "geo_tiffp.h"
#define _CPL_ERROR_H_INCLUDED_

#include "geo_normalize.h"
#include "geovalues.h"
#include "ogr_spatialref.h"
#include "gdal.h"
#include "xtiffio.h"
#include "tif_memio.h"

CPL_CVSID("$Id$");

CPL_C_START
char CPL_DLL *  GTIFGetOGISDefn( GTIF *, GTIFDefn * );
int  CPL_DLL   GTIFSetFromOGISDefn( GTIF *, const char * );

CPLErr CPL_DLL GTIFMemBufFromWkt( const char *pszWKT, 
                                  const double *padfGeoTransform,
                                  int nGCPCount, const GDAL_GCP *pasGCPList,
                                  int *pnSize, unsigned char **ppabyBuffer );
CPLErr CPL_DLL GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer, 
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList );
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
    char	*pszDatum;

/* -------------------------------------------------------------------- */
/*      First copy string and allocate with our CPLStrdup() to so we    */
/*      know when we are done this function we will have a CPL          */
/*      string, not a GTIF one.                                         */
/* -------------------------------------------------------------------- */
    pszDatum = CPLStrdup(*ppszDatum);
    GTIFFreeMemory( *ppszDatum );
    *ppszDatum = pszDatum;

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
            return;
        }
    }
}

/************************************************************************/
/*                          GTIFGetOGISDefn()                           */
/************************************************************************/

char *GTIFGetOGISDefn( GTIF *hGTIF, GTIFDefn * psDefn )

{
    OGRSpatialReference	oSRS;

    if( psDefn->Model != ModelTypeProjected 
        && psDefn->Model != ModelTypeGeographic )
        return CPLStrdup("");
    
/* -------------------------------------------------------------------- */
/*      If this is a projected SRS we set the PROJCS keyword first      */
/*      to ensure that the GEOGCS will be a child.                      */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected )
    {
        char	*pszPCSName = "unnamed";
        int         bNeedFree = FALSE;

        if( psDefn->PCS != KvUserDefined )
        {

            if( GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL ) )
                bNeedFree = TRUE;
            
            oSRS.SetNode( "PROJCS", pszPCSName );
            if( bNeedFree )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetAuthority( "PROJCS", "EPSG", psDefn->PCS );
        }
        else
        {
            char szPCSName[200];
            strcpy( szPCSName, "unnamed" );
            if( hGTIF != NULL )
                GTIFKeyGet( hGTIF, GTCitationGeoKey, szPCSName, 0, sizeof(szPCSName) );
            oSRS.SetNode( "PROJCS", szPCSName );
        }
    }
    
/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char	*pszGeogName = NULL;
    char	*pszDatumName = NULL;
    char	*pszPMName = NULL;
    char	*pszSpheroidName = NULL;
    char	*pszAngularUnits = NULL;
    double	dfInvFlattening, dfSemiMajor;
    char        szGCSName[200];
    
    if( hGTIF != NULL 
        && GTIFKeyGet( hGTIF, GeogCitationGeoKey, szGCSName, 0, sizeof(szGCSName)))
        pszGeogName = CPLStrdup(szGCSName);
    GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL );
    GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
    GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
    GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
    
    GTIFGetUOMAngleInfo( psDefn->UOMAngle, &pszAngularUnits, NULL );
    if( pszAngularUnits == NULL )
        pszAngularUnits = CPLStrdup("unknown");

    if( pszDatumName != NULL )
        WKTMassageDatum( &pszDatumName );

    dfSemiMajor = psDefn->SemiMajor;
    if( psDefn->SemiMajor == 0.0 )
    {
        pszSpheroidName = CPLStrdup("unretrievable - using WGS84");
        dfSemiMajor = SRS_WGS84_SEMIMAJOR;
        dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    else if( (psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
             || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 )
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);
    else
        dfInvFlattening = 0.0; /* special flag for infinity */

    oSRS.SetGeogCS( pszGeogName, pszDatumName, 
                    pszSpheroidName, dfSemiMajor, dfInvFlattening,
                    pszPMName,
                    psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                    pszAngularUnits,
                    psDefn->UOMAngleInDegrees * 0.0174532925199433 );

    if( psDefn->GCS != KvUserDefined )
        oSRS.SetAuthority( "GEOGCS", "EPSG", psDefn->GCS );

    if( psDefn->Datum != KvUserDefined )
        oSRS.SetAuthority( "DATUM", "EPSG", psDefn->Datum );

    if( psDefn->Ellipsoid != KvUserDefined )
        oSRS.SetAuthority( "SPHEROID", "EPSG", psDefn->Ellipsoid );

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    GTIFFreeMemory( pszPMName );
    GTIFFreeMemory( pszSpheroidName );
    GTIFFreeMemory( pszAngularUnits );
        
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
        
          case CT_EquidistantConic: 
            oSRS.SetEC( adfParm[0], adfParm[1],
                        adfParm[2], adfParm[3],
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
            oSRS.SetLCC( adfParm[2], adfParm[3],
                         adfParm[0], adfParm[1],
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
        {
            oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
            oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", psDefn->UOMLength );
        }
        else
            oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

        GTIFFreeMemory( pszUnitsName );
    }
    
/* -------------------------------------------------------------------- */
/*      Return the WKT serialization of the object.                     */
/* -------------------------------------------------------------------- */
    char	*pszWKT;

    oSRS.FixupOrdering();

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
/*      Do we know it as a built in?                                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszOGCName,"NAD27") 
        || EQUAL(pszOGCName,"North_American_Datum_1927") )
        return Datum_North_American_Datum_1927;
    else if( EQUAL(pszOGCName,"NAD83") 
        || EQUAL(pszOGCName,"North_American_Datum_1983") )
        return Datum_North_American_Datum_1983;
    else if( EQUAL(pszOGCName,"WGS84") || EQUAL(pszOGCName,"WGS_1984")
             || EQUAL(pszOGCName,"WGS 84"))
        return Datum_WGS84;
    else if( EQUAL(pszOGCName,"WGS72") || EQUAL(pszOGCName,"WGS_1972") )
        return Datum_WGS72;
    
/* -------------------------------------------------------------------- */
/*      Open the table if possible.                                     */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( CSVFilename("gdal_datum.csv"), "r" );
    if( fp == NULL )
        fp = VSIFOpen( CSVFilename("datum.csv"), "r" );

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
    OGRErr      eErr;

    GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
               RasterPixelIsArea);

/* -------------------------------------------------------------------- */
/*      Create an OGRSpatialReference object corresponding to the       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();

    if( poSRS->importFromWkt((char **) &pszOGCWKT) != OGRERR_NONE )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid definition.                                   */
/* -------------------------------------------------------------------- */
    short nSpheroid = KvUserDefined;
    double dfSemiMajor, dfInvFlattening;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID") != NULL
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID"),
                 "EPSG")) 
    {
        nSpheroid = (short)
            atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS|DATUM|SPHEROID"));
    }
    else if( poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID") != NULL
            && EQUAL(poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID"),"EPSG")) 
    {
        nSpheroid = (short)
            atoi(poSRS->GetAuthorityCode("GEOGCS|DATUM|SPHEROID"));
    }
    
    dfSemiMajor = poSRS->GetSemiMajor( &eErr );
    dfInvFlattening = poSRS->GetInvFlattening( &eErr );
    if( eErr != OGRERR_NONE )
    {
        dfSemiMajor = 0.0;
        dfInvFlattening = 0.0;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the Datum so we can special case a few PCS codes.           */
/* -------------------------------------------------------------------- */
    int		nDatum = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM") != NULL 
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM"),"EPSG") )
        nDatum = atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS|DATUM"));
    else if( poSRS->GetAuthorityName("GEOGCS|DATUM") != NULL 
             && EQUAL(poSRS->GetAuthorityName("GEOGCS|DATUM"),"EPSG") )
        nDatum = atoi(poSRS->GetAuthorityCode("GEOGCS|DATUM"));
    else if( poSRS->GetAttrValue("DATUM") != NULL )
        nDatum = OGCDatumName2EPSGDatumCode( poSRS->GetAttrValue("DATUM") );

/* -------------------------------------------------------------------- */
/*      Get the GCS if possible.                                        */
/* -------------------------------------------------------------------- */
    int         nGCS = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS") != NULL 
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS"),"EPSG") )
        nGCS = atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS"));
    else if( poSRS->GetAuthorityName("GEOGCS") != NULL 
             && EQUAL(poSRS->GetAuthorityName("GEOGCS"),"EPSG") )
        nGCS = atoi(poSRS->GetAuthorityCode("GEOGCS"));

    if( nGCS > 32767 )
        nGCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get the linear units.                                           */
/* -------------------------------------------------------------------- */
    char        *pszLinearUOMName = NULL;
    double	dfLinearUOM = poSRS->GetLinearUnits( &pszLinearUOMName );
    int         nUOMLengthCode = 9001; /* meters */

    if( (pszLinearUOMName != NULL
         && EQUAL(pszLinearUOMName,SRS_UL_FOOT))
        || dfLinearUOM == atof(SRS_UL_FOOT_CONV) )
        nUOMLengthCode = 9002; /* international foot */
    else if( (pszLinearUOMName != NULL
         && EQUAL(pszLinearUOMName,SRS_UL_US_FOOT))
             || ABS(dfLinearUOM-atof(SRS_UL_US_FOOT_CONV)) < 0.0000001 )
        nUOMLengthCode = 9003; /* us survey foot */
    else if( dfLinearUOM != 1.0 )
        nUOMLengthCode = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get some authority values.                                      */
/* -------------------------------------------------------------------- */
    if( poSRS->GetAuthorityName("PROJCS") != NULL 
        && EQUAL(poSRS->GetAuthorityName("PROJCS"),"EPSG") )
    {
        nPCS = atoi(poSRS->GetAuthorityCode("PROJCS"));
        if( nPCS > 32767 )
            nPCS = KvUserDefined;
    }

/* -------------------------------------------------------------------- */
/*      Handle the projection transformation.                           */
/* -------------------------------------------------------------------- */
    const char *pszProjection = poSRS->GetAttrValue( "PROJECTION" );

    if( nPCS != KvUserDefined )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
        GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
    }
    else if( pszProjection == NULL )
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
            && nZone <= 22 && bNorth && nUOMLengthCode == 9001 )
        {
            nPCS = 26900 + nZone;

            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
        else if( nDatum == Datum_North_American_Datum_1927 && nZone >= 3
            && nZone <= 22 && bNorth && nUOMLengthCode == 9001 )
        {
            nPCS = 26700 + nZone;

            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
        else if( nDatum == Datum_WGS84 && nUOMLengthCode == 9001 )
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
    
    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_TransvMercator_SouthOriented );

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
    
    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_2SP) 
             || EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )

    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Mercator );

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
    
    else if( EQUAL(pszProjection,SRS_PT_OBLIQUE_STEREOGRAPHIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_ObliqueStereographic );

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
    
    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Stereographic );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_PolarStereographic );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjStraightVertPoleLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_ObliqueMercator );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjAzimuthAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_AZIMUTH, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjRectifiedGridAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_CASSINI_SOLDNER) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_CassiniSoldner );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_EquidistantConic );

        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
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
    
    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Polyconic );

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
    
    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_AzimuthalEquidistant );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_MillerCylindrical );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Equirectangular );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_GNOMONIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Gnomonic );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_LambertAzimEqualArea );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_ORTHOGRAPHIC) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Orthographic );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_NewZealandMapGrid );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Robinson );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_Sinusoidal );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_VANDERGRINTEN) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_VanDerGrinten );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
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

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_LambertConfConic_2SP );

        GTIFKeySet(psGTIF, ProjFalseOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjFalseOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseOriginEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseOriginNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
	GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
	GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
	GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

	GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
		   CT_LambertConfConic_2SP );

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
    if( !poSRS->IsGeographic() )
    {
        GTIFKeySet(psGTIF, ProjLinearUnitsGeoKey, TYPE_SHORT, 1, 
                   nUOMLengthCode );
        if( nUOMLengthCode == KvUserDefined )
            GTIFKeySet( psGTIF, ProjLinearUnitSizeGeoKey, TYPE_DOUBLE, 1, 
                        dfLinearUOM);
    }
    
/* -------------------------------------------------------------------- */
/*      Write angular units.  Always Degrees for now.                   */
/* -------------------------------------------------------------------- */
    GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1, 
               Angular_Degree );

/* -------------------------------------------------------------------- */
/*      Try to write a citation from the main coordinate system         */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    if( poSRS->GetRoot() != NULL
        && poSRS->GetRoot()->GetChild(0) != NULL 
        && poSRS->IsProjected() )
    {
        GTIFKeySet( psGTIF, GTCitationGeoKey, TYPE_ASCII, 0, 
                    poSRS->GetRoot()->GetChild(0)->GetValue() );
    }

/* -------------------------------------------------------------------- */
/*      Try to write a GCS citation.                                    */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poGCS = poSRS->GetAttrNode( "GEOGCS" );

    if( poGCS != NULL && poGCS->GetChild(0) != NULL )
    {
        GTIFKeySet( psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                    poGCS->GetChild(0)->GetValue() );
    }

/* -------------------------------------------------------------------- */
/*      Try to identify the GCS/datum, scanning the EPSG datum file for */
/*      a match.                                                        */
/* -------------------------------------------------------------------- */
    if( nPCS == KvUserDefined )
    {
        if( nGCS == KvUserDefined )
        {
            if( nDatum == Datum_North_American_Datum_1927 )
                nGCS = GCS_NAD27;
            else if( nDatum == Datum_North_American_Datum_1983 )
                nGCS = GCS_NAD83;
            else if( nDatum == Datum_WGS84 || nDatum == DatumE_WGS84 )
                nGCS = GCS_WGS_84;
        }
            
        if( nGCS != KvUserDefined )
        {
            GTIFKeySet( psGTIF, GeographicTypeGeoKey, TYPE_SHORT,
                        1, nGCS );
        }
        else if( nDatum != KvUserDefined )
        {
            GTIFKeySet( psGTIF, GeographicTypeGeoKey, TYPE_SHORT, 1, 
                        KvUserDefined );
            GTIFKeySet( psGTIF, GeogGeodeticDatumGeoKey, TYPE_SHORT,
                        1, nDatum );
        }
        else if( nSpheroid != KvUserDefined )
        {
            GTIFKeySet( psGTIF, GeographicTypeGeoKey, TYPE_SHORT, 1, 
                        KvUserDefined );
            GTIFKeySet( psGTIF, GeogGeodeticDatumGeoKey, TYPE_SHORT,
                        1, KvUserDefined );
            GTIFKeySet( psGTIF, GeogEllipsoidGeoKey, TYPE_SHORT, 1, 
                        nSpheroid );
        }
        else if( dfSemiMajor != 0.0 )
        {
            GTIFKeySet( psGTIF, GeographicTypeGeoKey, TYPE_SHORT, 1, 
                        KvUserDefined );
            GTIFKeySet( psGTIF, GeogGeodeticDatumGeoKey, TYPE_SHORT,
                        1, KvUserDefined );
            GTIFKeySet( psGTIF, GeogEllipsoidGeoKey, TYPE_SHORT, 1, 
                        KvUserDefined );
            GTIFKeySet( psGTIF, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1,
                        dfSemiMajor );
            GTIFKeySet( psGTIF, GeogInvFlatteningGeoKey, TYPE_DOUBLE, 1,
                        dfInvFlattening );
        }
        else if( poSRS->GetAttrValue("DATUM") != NULL
                 && strstr(poSRS->GetAttrValue("DATUM"),"unknown") == NULL
                 && strstr(poSRS->GetAttrValue("DATUM"),"unnamed") == NULL )
                 
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Couldn't translate `%s' to a GeoTIFF datum.\n",
                      poSRS->GetAttrValue("DATUM") );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    delete poSRS;

    return TRUE;
}

/************************************************************************/
/*                         GTIFWktFromMemBuf()                          */
/************************************************************************/

CPLErr GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer, 
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList )

{
    MemIOBuf	sIOBuf;
    TIFF        *hTIFF;
    GTIF 	*hGTIF;
    GTIFDefn	sGTIFDefn;

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    MemIO_InitBuf( &sIOBuf, nSize, pabyBuffer );
    
    hTIFF = XTIFFClientOpen( "membuf", "r", (thandle_t) &sIOBuf, 
                             MemIO_ReadProc, MemIO_WriteProc, MemIO_SeekProc, 
                             MemIO_CloseProc, MemIO_SizeProc, 
                             MemIO_MapProc, MemIO_UnmapProc );

    if( hTIFF == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the projection definition.                                  */
/* -------------------------------------------------------------------- */
    hGTIF = GTIFNew(hTIFF);

    if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
        *ppszWKT = GTIFGetOGISDefn( hGTIF, &sGTIFDefn );
    else
        *ppszWKT = NULL;
    
    GTIFFree( hGTIF );

/* -------------------------------------------------------------------- */
/*      Get geotransform or tiepoints.                                  */
/* -------------------------------------------------------------------- */
    double	*padfTiePoints, *padfScale, *padfMatrix;
    int16	nCount;

    padfGeoTransform[0] = 0.0;
    padfGeoTransform[1] = 1.0;
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = 0.0;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = 1.0;

    *pnGCPCount = 0;
    *ppasGCPList = NULL;
    
    if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
        && nCount >= 2 )
    {
        padfGeoTransform[1] = padfScale[0];
        padfGeoTransform[5] = - ABS(padfScale[1]);

        if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
        {
            padfGeoTransform[0] =
                padfTiePoints[3] - padfTiePoints[0] * padfGeoTransform[1];
            padfGeoTransform[3] =
                padfTiePoints[4] - padfTiePoints[1] * padfGeoTransform[5];
        }
    }

    else if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
    {
        *pnGCPCount = nCount / 6;
        *ppasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),*pnGCPCount);
        
        for( int iGCP = 0; iGCP < *pnGCPCount; iGCP++ )
        {
            char	szID[32];
            GDAL_GCP	*psGCP = *ppasGCPList + iGCP;

            sprintf( szID, "%d", iGCP+1 );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = "";
            psGCP->dfGCPPixel = padfTiePoints[iGCP*6+0];
            psGCP->dfGCPLine = padfTiePoints[iGCP*6+1];
            psGCP->dfGCPX = padfTiePoints[iGCP*6+3];
            psGCP->dfGCPY = padfTiePoints[iGCP*6+4];
            psGCP->dfGCPZ = padfTiePoints[iGCP*6+5];
        }
    }

    else if( TIFFGetField(hTIFF,TIFFTAG_GEOTRANSMATRIX,&nCount,&padfMatrix ) 
             && nCount == 16 )
    {
        padfGeoTransform[0] = padfMatrix[3];
        padfGeoTransform[1] = padfMatrix[0];
        padfGeoTransform[2] = padfMatrix[1];
        padfGeoTransform[3] = padfMatrix[7];
        padfGeoTransform[4] = padfMatrix[4];
        padfGeoTransform[5] = padfMatrix[5];
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    XTIFFClose( hTIFF );

    MemIO_DeinitBuf( &sIOBuf );

    if( *ppszWKT == NULL )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                         GTIFMemBufFromWkt()                          */
/************************************************************************/

CPLErr GTIFMemBufFromWkt( const char *pszWKT, const double *padfGeoTransform,
                          int nGCPCount, const GDAL_GCP *pasGCPList,
                          int *pnSize, unsigned char **ppabyBuffer )

{
    MemIOBuf	sIOBuf;
    TIFF        *hTIFF;
    GTIF 	*hGTIF;

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    MemIO_InitBuf( &sIOBuf, 0, NULL );
    
    hTIFF = XTIFFClientOpen( "membuf", "w", (thandle_t) &sIOBuf, 
                             MemIO_ReadProc, MemIO_WriteProc, MemIO_SeekProc, 
                             MemIO_CloseProc, MemIO_SizeProc, 
                             MemIO_MapProc, MemIO_UnmapProc );

    if( hTIFF == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Write some minimal set of image parameters.                     */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, 1 );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, 1 );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, 8 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    
/* -------------------------------------------------------------------- */
/*      Get the projection definition.                                  */
/* -------------------------------------------------------------------- */

    if( pszWKT != NULL )
    {
        hGTIF = GTIFNew(hTIFF);
        GTIFSetFromOGISDefn( hGTIF, pszWKT );
        GTIFWriteKeys( hGTIF );
        GTIFFree( hGTIF );
    }

/* -------------------------------------------------------------------- */
/*      Set the geotransform, or GCPs.                                  */
/* -------------------------------------------------------------------- */
    if( padfGeoTransform[0] != 0.0 || padfGeoTransform[1] != 1.0
        || padfGeoTransform[2] != 0.0 || padfGeoTransform[3] != 0.0
        || padfGeoTransform[4] != 0.0 || ABS(padfGeoTransform[5]) != 1.0 )
    {

        if( padfGeoTransform[2] == 0.0 && padfGeoTransform[4] == 0.0 )
        {
            double	adfPixelScale[3], adfTiePoints[6];

            adfPixelScale[0] = padfGeoTransform[1];
            adfPixelScale[1] = fabs(padfGeoTransform[5]);
            adfPixelScale[2] = 0.0;

            TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
            
            adfTiePoints[0] = 0.0;
            adfTiePoints[1] = 0.0;
            adfTiePoints[2] = 0.0;
            adfTiePoints[3] = padfGeoTransform[0];
            adfTiePoints[4] = padfGeoTransform[3];
            adfTiePoints[5] = 0.0;
        
            TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
        }
        else
        {
            double	adfMatrix[16];

            memset(adfMatrix,0,sizeof(double) * 16);

            adfMatrix[0] = padfGeoTransform[1];
            adfMatrix[1] = padfGeoTransform[2];
            adfMatrix[3] = padfGeoTransform[0];
            adfMatrix[4] = padfGeoTransform[4];
            adfMatrix[5] = padfGeoTransform[5];
            adfMatrix[7] = padfGeoTransform[3];
            adfMatrix[15] = 1.0;

            TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( nGCPCount > 0 )
    {
        double	*padfTiePoints;

        padfTiePoints = (double *) CPLMalloc(6*sizeof(double)*nGCPCount);

        for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {

            padfTiePoints[iGCP*6+0] = pasGCPList[iGCP].dfGCPPixel;
            padfTiePoints[iGCP*6+1] = pasGCPList[iGCP].dfGCPLine;
            padfTiePoints[iGCP*6+2] = 0;
            padfTiePoints[iGCP*6+3] = pasGCPList[iGCP].dfGCPX;
            padfTiePoints[iGCP*6+4] = pasGCPList[iGCP].dfGCPY;
            padfTiePoints[iGCP*6+5] = pasGCPList[iGCP].dfGCPZ;
        }

        TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6*nGCPCount, padfTiePoints);
        CPLFree( padfTiePoints );
    } 

/* -------------------------------------------------------------------- */
/*      Cleanup and return the created memory buffer.                   */
/* -------------------------------------------------------------------- */
    GByte bySmallImage = 0;

    TIFFWriteEncodedStrip( hTIFF, 0, (char *) &bySmallImage, 1 );
    TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTIFMemBufFromWkt");
    TIFFWriteDirectory( hTIFF );

    XTIFFClose( hTIFF );

    *pnSize = sIOBuf.size;
    *ppabyBuffer = (unsigned char *) CPLMalloc(*pnSize);
    memcpy( *ppabyBuffer, sIOBuf.data, *pnSize );
    
    MemIO_DeinitBuf( &sIOBuf );
    
    return CE_None;
}
