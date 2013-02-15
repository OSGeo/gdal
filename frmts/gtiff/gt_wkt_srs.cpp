/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.  This code is
 *           deliberately GDAL free, and it is intended to be moved into
 *           libgeotiff someday if possible.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "cpl_serv.h"
#include "geo_tiffp.h"
#define CPL_ERROR_H_INCLUDED

#include "geovalues.h"
#include "ogr_spatialref.h"
#include "gdal.h"
#include "xtiffio.h"
#include "cpl_multiproc.h"
#include "tifvsi.h"
#include "gt_wkt_srs.h"
#include "gt_wkt_srs_for_gdal.h"
#include "gt_citation.h"

CPL_CVSID("$Id$")

#define ProjLinearUnitsInterpCorrectGeoKey   3059

#ifndef CT_HotineObliqueMercatorAzimuthCenter
#  define CT_HotineObliqueMercatorAzimuthCenter 9815
#endif


CPL_C_START
void    LibgeotiffOneTimeInit();

// replicated from gdal_csv.h. 
const char * GDALDefaultCSVFilename( const char *pszBasename );

#ifndef CPL_SERV_H_INTERNAL
/* Make VSIL_STRICT_ENFORCE active in DEBUG builds */
#ifdef DEBUG
#define VSIL_STRICT_ENFORCE
#endif

#ifdef VSIL_STRICT_ENFORCE
typedef struct _VSILFILE VSILFILE;
#else
typedef FILE VSILFILE;
#endif

// ensure compatability with older libgeotiffs. 
#if !defined(GTIFAtof)
#  define GTIFAtof atof
#endif

int CPL_DLL VSIFCloseL( VSILFILE * );
int CPL_DLL VSIUnlink( const char * );
VSILFILE CPL_DLL *VSIFileFromMemBuffer( const char *pszFilename,
                                    GByte *pabyData, 
                                    GUIntBig nDataLength,
                                    int bTakeOwnership );
GByte CPL_DLL *VSIGetMemFileBuffer( const char *pszFilename, 
                                    GUIntBig *pnDataLength, 
                                    int bUnlinkAndSeize );

int CPL_DLL CSLTestBoolean( const char *pszValue );
const char CPL_DLL * CPL_STDCALL CPLGetConfigOption( const char *, const char * );
									
/* Those stuff are redefined in external libgeotiff cpl_serv.h */
/* as macros. Let's use GDAL functions instead */
/* E.Rouault : I'm wondering why we just don't #define CPL_SERV_H_INCLUDED */
/* at the beginning of this file to avoid cpl_serv.h to be used at all ??? */

#undef CSVReadParseLine
char CPL_DLL  **CSVReadParseLine( FILE *fp);
#undef CSLDestroy
void CPL_DLL CPL_STDCALL CSLDestroy(char **papszStrList);
#undef VSIFree
void CPL_DLL    VSIFree( void * );
#undef CPLFree
#define CPLFree VSIFree
#undef CPLMalloc
void CPL_DLL *CPLMalloc( size_t );
#undef CPLCalloc
void CPL_DLL *CPLCalloc( size_t, size_t );
#undef CPLStrdup
char CPL_DLL *CPLStrdup( const char * );

#endif /* CPL_SERV_H_INTERNAL */

const char CPL_DLL * CPL_STDCALL CPLGetConfigOption( const char *, const char * );
void CPL_DLL CPL_STDCALL CPLDebug( const char *, const char *, ... );

CPL_C_END

// To remind myself not to use CPLString in this file!
#define CPLString Please_do_not_use_CPLString_in_this_file

static const char *papszDatumEquiv[] =
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

// older libgeotiff's won't list this.
#ifndef CT_CylindricalEqualArea
# define CT_CylindricalEqualArea 28
#endif

/************************************************************************/
/*                       LibgeotiffOneTimeInit()                        */
/************************************************************************/

void LibgeotiffOneTimeInit() 
{
    static int bOneTimeInitDone = FALSE;
    static void* hMutex = NULL;
    CPLMutexHolder oHolder( &hMutex);

    if (bOneTimeInitDone)
        return;

    bOneTimeInitDone = TRUE;

    // If linking with an external libgeotiff we hope this will call the
    // SetCSVFilenameHook() in libgeotiff, not the one in gdal/port!
    SetCSVFilenameHook( GDALDefaultCSVFilename );
}

/************************************************************************/
/*                       GTIFToCPLRecyleString()                        */
/*                                                                      */
/*      This changes a string from the libgeotiff heap to the GDAL      */
/*      heap.                                                           */
/************************************************************************/

static void GTIFToCPLRecycleString( char **ppszTarget )

{
    if( *ppszTarget == NULL )
        return;

    char *pszTempString = CPLStrdup(*ppszTarget);
    GTIFFreeMemory( *ppszTarget );
    *ppszTarget = pszTempString;
}

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

    pszDatum = *ppszDatum;
    if (pszDatum[0] == '\0')
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[i] != '+'
            && !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
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
/*                      GTIFCleanupImageineNames()                      */
/*                                                                      */
/*      Erdas Imagine sometimes emits big copyright messages, and       */
/*      other stuff into citations.  These can be pretty messy when     */
/*      turned into WKT, so we try to trim and clean the strings        */
/*      somewhat.                                                       */
/************************************************************************/

/* For example:
   GTCitationGeoKey (Ascii,215): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision$ $Date$\nProjection Name = UTM\nUnits = meters\nGeoTIFF Units = meters"

   GeogCitationGeoKey (Ascii,267): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision$ $Date$\nUnable to match Ellipsoid (Datum) to a GeographicTypeGeoKey value\nEllipsoid = Clarke 1866\nDatum = NAD27 (CONUS)"

   PCSCitationGeoKey (Ascii,214): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision$ $Date$\nUTM Zone 10N\nEllipsoid = Clarke 1866\nDatum = NAD27 (CONUS)"
 
*/

static void GTIFCleanupImagineNames( char *pszCitation )

{
    if( strstr(pszCitation,"IMAGINE GeoTIFF") == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      First, we skip past all the copyright, and RCS stuff.  We       */
/*      assume that this will have a "$" at the end of it all.          */
/* -------------------------------------------------------------------- */
    char *pszSkip;
    
    for( pszSkip = pszCitation + strlen(pszCitation) - 1;
         pszSkip != pszCitation && *pszSkip != '$'; 
         pszSkip-- ) {}

    if( *pszSkip == '$' )
        pszSkip++;

    memmove( pszCitation, pszSkip, strlen(pszSkip)+1 );

/* -------------------------------------------------------------------- */
/*      Convert any newlines into spaces, they really gum up the        */
/*      WKT.                                                            */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; pszCitation[i] != '\0'; i++ )
    {
        if( pszCitation[i] == '\n' )
            pszCitation[i] = ' ';
    }
}

/************************************************************************/
/*                          GTIFGetOGISDefn()                           */
/************************************************************************/

char *GTIFGetOGISDefn( GTIF *hGTIF, GTIFDefn * psDefn )

{
    OGRSpatialReference	oSRS;

/* -------------------------------------------------------------------- */
/*  Handle non-standard coordinate systems where GTModelTypeGeoKey      */
/*  is not defined, but ProjectedCSTypeGeoKey is defined (ticket #3019) */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == KvUserDefined && psDefn->PCS != KvUserDefined)
    {
        psDefn->Model = ModelTypeProjected;
    }

/* -------------------------------------------------------------------- */
/*      Handle non-standard coordinate systems as LOCAL_CS.             */
/* -------------------------------------------------------------------- */
    if( psDefn->Model != ModelTypeProjected 
        && psDefn->Model != ModelTypeGeographic 
        && psDefn->Model != ModelTypeGeocentric )
    {
        char	*pszWKT;
        char    szPeStr[2400];

        /** check if there is a pe string citation key **/
        if( GTIFKeyGet( hGTIF, PCSCitationGeoKey, szPeStr, 0, sizeof(szPeStr) ) &&
            strstr(szPeStr, "ESRI PE String = " ) )
        {
            pszWKT = CPLStrdup( szPeStr + strlen("ESRI PE String = ") );
            return pszWKT;
        }
        else
        {
            char	*pszUnitsName = NULL;
            char    szPCSName[300];
            int     nKeyCount = 0;
            int     anVersion[3];

            if( hGTIF != NULL )
                GTIFDirectoryInfo( hGTIF, anVersion, &nKeyCount );

            if( nKeyCount > 0 ) // Use LOCAL_CS if we have any geokeys at all.
            {
                // Handle citation.
                strcpy( szPCSName, "unnamed" );
                if( !GTIFKeyGet( hGTIF, GTCitationGeoKey, szPCSName, 
                                 0, sizeof(szPCSName) ) )
                    GTIFKeyGet( hGTIF, GeogCitationGeoKey, szPCSName, 
                                0, sizeof(szPCSName) );

                GTIFCleanupImagineNames( szPCSName );
                oSRS.SetLocalCS( szPCSName );

                // Handle units
                GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );
              
                if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
                {
                    oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
                    oSRS.SetAuthority( "LOCAL_CS|UNIT", "EPSG", psDefn->UOMLength);
                }
                else
                    oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

                GTIFFreeMemory( pszUnitsName );
            }
            oSRS.exportToWkt( &pszWKT );

            return pszWKT;
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle Geocentric coordinate systems.                           */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeGeocentric )
    {
        char    szName[300];

        strcpy( szName, "unnamed" );
        if( !GTIFKeyGet( hGTIF, GTCitationGeoKey, szName, 
                         0, sizeof(szName) ) )
            GTIFKeyGet( hGTIF, GeogCitationGeoKey, szName, 
                        0, sizeof(szName) );

        oSRS.SetGeocCS( szName );

        char	*pszUnitsName = NULL;
          
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );
        
        if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
        {
            oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
            oSRS.SetAuthority( "GEOCCS|UNIT", "EPSG", psDefn->UOMLength );
        }
        else
            oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

        GTIFFreeMemory( pszUnitsName );
    }
    
/* -------------------------------------------------------------------- */
/*      #3901: In libgeotiff 1.3.0 and earlier we incorrectly           */
/*      interpreted linear projection parameter geokeys (false          */
/*      easting/northing) as being in meters instead of the             */
/*      coordinate system of the file.   The following code attempts    */
/*      to provide mechanisms for fixing the issue if we are linked     */
/*      with an older version of libgeotiff.                            */
/* -------------------------------------------------------------------- */
    int iParm;
    const char *pszLinearUnits = 
        CPLGetConfigOption( "GTIFF_LINEAR_UNITS", "DEFAULT" );

#if LIBGEOTIFF_VERSION <= 1300
    if( EQUAL(pszLinearUnits,"DEFAULT") && psDefn->Projection == KvUserDefined )
    {
        for( iParm = 0; iParm < psDefn->nParms; iParm++ )
        {
            switch( psDefn->ProjParmId[iParm] )
            {
              case ProjFalseEastingGeoKey:
              case ProjFalseNorthingGeoKey:
              case ProjFalseOriginEastingGeoKey:
              case ProjFalseOriginNorthingGeoKey:
              case ProjCenterEastingGeoKey:
              case ProjCenterNorthingGeoKey:
                if( psDefn->UOMLengthInMeters != 0 
                    && psDefn->UOMLengthInMeters != 1.0 )
                {
                    psDefn->ProjParm[iParm] *= psDefn->UOMLengthInMeters;
                    CPLDebug( "GTIFF", "converting geokey to meters to fix bug in old libgeotiff" );
                }
                break;

              default:
                break;
            }
        }
    }
#endif /* LIBGEOTIFF_VERSION <= 1300 */

/* -------------------------------------------------------------------- */
/*      #3901: If folks have broken GeoTIFF files generated with        */
/*      older versions of GDAL+libgeotiff, then they may need a         */
/*      hack to allow them to be read properly.  This is that           */
/*      hack.  We basically try to undue the conversion applied by      */
/*      libgeotiff to meters (or above) to simulate the old             */
/*      behavior.                                                       */
/* -------------------------------------------------------------------- */
    short bLinearUnitsMarkedCorrect = FALSE;
    
    GTIFKeyGet(hGTIF, (geokey_t) ProjLinearUnitsInterpCorrectGeoKey, 
               &bLinearUnitsMarkedCorrect, 0, 1);

    if( EQUAL(pszLinearUnits,"BROKEN") 
        && psDefn->Projection == KvUserDefined 
        && !bLinearUnitsMarkedCorrect )
    {
        for( iParm = 0; iParm < psDefn->nParms; iParm++ )
        {
            switch( psDefn->ProjParmId[iParm] )
            {
              case ProjFalseEastingGeoKey:
              case ProjFalseNorthingGeoKey:
              case ProjFalseOriginEastingGeoKey:
              case ProjFalseOriginNorthingGeoKey:
              case ProjCenterEastingGeoKey:
              case ProjCenterNorthingGeoKey:
                if( psDefn->UOMLengthInMeters != 0 
                    && psDefn->UOMLengthInMeters != 1.0 )
                {
                    psDefn->ProjParm[iParm] /= psDefn->UOMLengthInMeters;
                    CPLDebug( "GTIFF", "converting geokey to accomodate old broken file due to GTIFF_LINEAR_UNITS=BROKEN setting." );
                }
                break;

              default:
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is a projected SRS we set the PROJCS keyword first      */
/*      to ensure that the GEOGCS will be a child.                      */
/* -------------------------------------------------------------------- */
    OGRBoolean linearUnitIsSet = FALSE;
    if( psDefn->Model == ModelTypeProjected )
    {
        char        szCTString[512];
        strcpy( szCTString, "unnamed" );
        if( psDefn->PCS != KvUserDefined )
        {
            char    *pszPCSName = NULL;

            GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL );
            
            oSRS.SetNode( "PROJCS", pszPCSName ? pszPCSName : "unnamed" );
            if ( pszPCSName )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetAuthority( "PROJCS", "EPSG", psDefn->PCS );
        }
        else if(hGTIF && GTIFKeyGet( hGTIF, PCSCitationGeoKey, szCTString, 0, 
                                     sizeof(szCTString)) )  
        {
            if (!SetCitationToSRS(hGTIF, szCTString, sizeof(szCTString),
                                  PCSCitationGeoKey, &oSRS, &linearUnitIsSet))
                oSRS.SetNode("PROJCS",szCTString);
        }
        else
        {
            if( hGTIF )
            {
                GTIFKeyGet( hGTIF, GTCitationGeoKey, szCTString, 0, sizeof(szCTString) );
                if(!SetCitationToSRS(hGTIF, szCTString, sizeof(szCTString),
                                     GTCitationGeoKey, &oSRS, &linearUnitIsSet))
                    oSRS.SetNode( "PROJCS", szCTString );
            }
            else
                oSRS.SetNode( "PROJCS", szCTString );
        }

        /* Handle ESRI/Erdas style state plane and UTM in citation key */
        if( CheckCitationKeyForStatePlaneUTM(hGTIF, psDefn, &oSRS, &linearUnitIsSet) )
        {
            char	*pszWKT;
            oSRS.morphFromESRI();
            oSRS.FixupOrdering();
            if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
                return pszWKT;
        }

        /* Handle ESRI PE string in citation */
        szCTString[0] = '\0';
        if( hGTIF && GTIFKeyGet( hGTIF, GTCitationGeoKey, szCTString, 0, sizeof(szCTString) ) )
            SetCitationToSRS(hGTIF, szCTString, sizeof(szCTString), GTCitationGeoKey, &oSRS, &linearUnitIsSet);
    }
    
/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char	*pszGeogName = NULL;
    char	*pszDatumName = NULL;
    char	*pszPMName = NULL;
    char	*pszSpheroidName = NULL;
    char	*pszAngularUnits = NULL;
    double	dfInvFlattening=0.0, dfSemiMajor=0.0;
    char  szGCSName[512];
    OGRBoolean aUnitGot = FALSE;
    
    if( !GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL )
        && hGTIF != NULL 
        && GTIFKeyGet( hGTIF, GeogCitationGeoKey, szGCSName, 0, 
                       sizeof(szGCSName)) )
    {
        GetGeogCSFromCitation(szGCSName, sizeof(szGCSName),
                              GeogCitationGeoKey, 
                              &pszGeogName, &pszDatumName,
                              &pszPMName, &pszSpheroidName,
                              &pszAngularUnits);
    }
    else
        GTIFToCPLRecycleString( &pszGeogName );
        
    if( !pszDatumName )
    {
        GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
        GTIFToCPLRecycleString( &pszDatumName );
    }
    if( !pszSpheroidName )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
        GTIFToCPLRecycleString( &pszSpheroidName );
    }
    else
    {
        GTIFKeyGet(hGTIF, GeogSemiMajorAxisGeoKey, &(psDefn->SemiMajor), 0, 1 );
        GTIFKeyGet(hGTIF, GeogInvFlatteningGeoKey, &dfInvFlattening, 0, 1 );
    }
    if( !pszPMName )
    {
        GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
        GTIFToCPLRecycleString( &pszPMName );
    }
    else
        GTIFKeyGet(hGTIF, GeogPrimeMeridianLongGeoKey, &(psDefn->PMLongToGreenwich), 0, 1 );
    
    if( !pszAngularUnits )
    {
        GTIFGetUOMAngleInfo( psDefn->UOMAngle, &pszAngularUnits, NULL );
        if( pszAngularUnits == NULL )
            pszAngularUnits = CPLStrdup("unknown");
        else
            GTIFToCPLRecycleString( &pszAngularUnits );
    }
    else
    {
        GTIFKeyGet(hGTIF, GeogAngularUnitSizeGeoKey, &(psDefn->UOMAngleInDegrees), 0, 1 );
        aUnitGot = TRUE;
    }

    if( pszDatumName != NULL )
        WKTMassageDatum( &pszDatumName );

    dfSemiMajor = psDefn->SemiMajor;
    if( dfSemiMajor == 0.0 )
    {
        pszSpheroidName = CPLStrdup("unretrievable - using WGS84");
        dfSemiMajor = SRS_WGS84_SEMIMAJOR;
        dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    else if( dfInvFlattening == 0.0 && ((psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
                                        || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 ) )
    {
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);

        /* Take official inverse flattening definition in the WGS84 case */
        if (fabs(dfSemiMajor-SRS_WGS84_SEMIMAJOR) < 1e-10 &&
            fabs(dfInvFlattening - SRS_WGS84_INVFLATTENING) < 1e-10)
            dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    if(!pszGeogName || strlen(pszGeogName) == 0)
    {
        CPLFree(pszGeogName);
        pszGeogName = CPLStrdup( pszDatumName ? pszDatumName : "unknown" );
    }

    if(aUnitGot)
        oSRS.SetGeogCS( pszGeogName, pszDatumName, 
                        pszSpheroidName, dfSemiMajor, dfInvFlattening,
                        pszPMName,
                        psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                        pszAngularUnits,
                        psDefn->UOMAngleInDegrees );
    else
        oSRS.SetGeogCS( pszGeogName, pszDatumName, 
                        pszSpheroidName, dfSemiMajor, dfInvFlattening,
                        pszPMName,
                        psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                        pszAngularUnits,
                        psDefn->UOMAngleInDegrees * 0.0174532925199433 );

    if( psDefn->GCS != KvUserDefined && psDefn->GCS > 0 )
        oSRS.SetAuthority( "GEOGCS", "EPSG", psDefn->GCS );

    if( psDefn->Datum != KvUserDefined )
        oSRS.SetAuthority( "DATUM", "EPSG", psDefn->Datum );

    if( psDefn->Ellipsoid != KvUserDefined )
        oSRS.SetAuthority( "SPHEROID", "EPSG", psDefn->Ellipsoid );

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    CPLFree( pszSpheroidName );
    CPLFree( pszPMName );
    CPLFree( pszAngularUnits );

#if LIBGEOTIFF_VERSION >= 1310 && !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    if( psDefn->TOWGS84Count > 0 )
        oSRS.SetTOWGS84( psDefn->TOWGS84[0],
                         psDefn->TOWGS84[1],
                         psDefn->TOWGS84[2],
                         psDefn->TOWGS84[3],
                         psDefn->TOWGS84[4],
                         psDefn->TOWGS84[5],
                         psDefn->TOWGS84[6] );
#endif
        
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

        for( ; i < 10; i++ )
            adfParm[i] = 0.0;

        if(!aUnitGot)
        {
            adfParm[0] *= psDefn->UOMAngleInDegrees;
            adfParm[1] *= psDefn->UOMAngleInDegrees;
            adfParm[2] *= psDefn->UOMAngleInDegrees;
            adfParm[3] *= psDefn->UOMAngleInDegrees; 
        }
        int unitCode = 0;
        GTIFKeyGet(hGTIF, ProjLinearUnitsGeoKey, &unitCode, 0, 1  );
        if(unitCode != KvUserDefined)
        {
            adfParm[5] /= psDefn->UOMLengthInMeters;
            adfParm[6] /= psDefn->UOMLengthInMeters;
        }
        
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
                              
            if (psDefn->Projection == 1024 || psDefn->Projection == 9841) // override hack for google mercator. 
            {
                oSRS.SetExtension( "PROJCS", "PROJ4",  
                                   "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs" ); 
            }
            break;

          case CT_ObliqueStereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_Stereographic:
            oSRS.SetStereographic( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueMercator: /* hotine */
            oSRS.SetHOM( adfParm[0], adfParm[1],
                         adfParm[2], adfParm[3],
                         adfParm[4],
                         adfParm[5], adfParm[6] );
            break;
        
          case CT_HotineObliqueMercatorAzimuthCenter:
            oSRS.SetHOMAC( adfParm[0], adfParm[1],
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
            oSRS.SetEquirectangular2( adfParm[0], adfParm[1],
                                      adfParm[2],
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

          case CT_CylindricalEqualArea:
            oSRS.SetCEA( adfParm[0], adfParm[1],
                         adfParm[5], adfParm[6] );
            break;
          default:
            if( oSRS.IsProjected() )
                oSRS.GetRoot()->SetValue( "LOCAL_CS" );
            break;
        }

/* -------------------------------------------------------------------- */
/*      Set projection units.                                           */
/* -------------------------------------------------------------------- */
        if(!linearUnitIsSet)
        {
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
    }

/* ==================================================================== */
/*      Handle vertical coordinate system information if we have it.    */
/* ==================================================================== */
    short verticalCSType = -1;
    short verticalDatum = -1;
    short verticalUnits = -1;
    const char *pszFilename = NULL;
    const char *pszValue;
    char szSearchKey[128];
    bool bNeedManualVertCS = false;
    char citation[2048];

    // Don't do anything if there is no apparent vertical information.
    GTIFKeyGet( hGTIF, VerticalCSTypeGeoKey, &verticalCSType, 0, 1 );
    GTIFKeyGet( hGTIF, VerticalDatumGeoKey, &verticalDatum, 0, 1 );
    GTIFKeyGet( hGTIF, VerticalUnitsGeoKey, &verticalUnits, 0, 1 );

    if( (verticalCSType != -1 || verticalDatum != -1 || verticalUnits != -1)
        && (oSRS.IsGeographic() || oSRS.IsProjected() || oSRS.IsLocal()) )
    {
        if( !GTIFKeyGet( hGTIF, VerticalCitationGeoKey, &citation, 
                         0, sizeof(citation) ) )
            strcpy( citation, "unknown" );

/* -------------------------------------------------------------------- */
/*      The original geotiff specification appears to have              */
/*      misconstrued the EPSG codes 5101 to 5106 to be vertical         */
/*      coordinate system codes, when in fact they are vertical         */
/*      datum codes.  So if these are found in the                      */
/*      VerticalCSTypeGeoKey move them to the VerticalDatumGeoKey       */
/*      and insert the "normal" corresponding VerticalCSTypeGeoKey      */
/*      value.                                                          */
/* -------------------------------------------------------------------- */
        if( (verticalCSType >= 5101 && verticalCSType <= 5112)
            && verticalDatum == -1 )
        {
            verticalDatum = verticalCSType;
            verticalCSType = verticalDatum + 600;
        }

/* -------------------------------------------------------------------- */
/*      This addresses another case where the EGM96 Vertical Datum code */
/*      is mis-used as a Vertical CS code (#4922)                       */
/* -------------------------------------------------------------------- */
        if( verticalCSType == 5171 )
        {
            verticalDatum = 5171;
            verticalCSType = 5773;
        }
 
/* -------------------------------------------------------------------- */
/*      Somewhat similarly, codes 5001 to 5033 were treated as          */
/*      vertical coordinate systems based on ellipsoidal heights.       */
/*      We use the corresponding 2d geodetic datum as the vertical      */
/*      datum and clear the vertical coordinate system code since       */
/*      there isn't one in epsg.                                        */
/* -------------------------------------------------------------------- */
        if( (verticalCSType >= 5001 && verticalCSType <= 5033)
            && verticalDatum == -1 )
        {
            verticalDatum = verticalCSType+1000;
            verticalCSType = -1;
        }

/* -------------------------------------------------------------------- */
/*      Promote to being a compound coordinate system.                  */
/* -------------------------------------------------------------------- */
        OGR_SRSNode *poOldRoot = oSRS.GetRoot()->Clone();

        oSRS.Clear();
        oSRS.SetNode( "COMPD_CS", "unknown" );
        oSRS.GetRoot()->AddChild( poOldRoot );

/* -------------------------------------------------------------------- */
/*      If we have the vertical cs, try to look it up using the         */
/*      vertcs.csv file, and use the definition provided by that.       */
/* -------------------------------------------------------------------- */
        bNeedManualVertCS = true;

        if( verticalCSType != KvUserDefined && verticalCSType > 0 )
        {
            OGRSpatialReference oVertSRS;
            if( oVertSRS.importFromEPSG( verticalCSType ) == OGRERR_NONE )
            {
                oSRS.GetRoot()->AddChild( oVertSRS.GetRoot()->Clone() );
                bNeedManualVertCS = false;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect some information from the VerticalCS if not provided    */
/*      via geokeys.                                                    */
/* -------------------------------------------------------------------- */
    if( bNeedManualVertCS )
    {
        if( verticalCSType > 0 && verticalCSType != KvUserDefined )
        {
            pszFilename = CSVFilename( "coordinate_reference_system.csv" );
            sprintf( szSearchKey, "%d", verticalCSType );

            if( verticalDatum < 1 || verticalDatum == KvUserDefined )
            {
                pszValue = CSVGetField( pszFilename, 
                                        "coord_ref_sys_code", 
                                        szSearchKey, CC_Integer, 
                                        "datum_code" );
                if( pszValue != NULL )
                    verticalDatum = (short) atoi(pszValue);
            }

            if( EQUAL(citation,"unknown") )
            {
                pszValue = CSVGetField( pszFilename, 
                                        "coord_ref_sys_code", 
                                        szSearchKey, CC_Integer, 
                                        "coord_ref_sys_name" );
                if( pszValue != NULL && *pszValue != '\0' )
                    strncpy( citation, pszValue, sizeof(citation) );
            }

            if( verticalUnits < 1 || verticalUnits == KvUserDefined )
            {
                pszValue = CSVGetField( pszFilename, 
                                        "coord_ref_sys_code", 
                                        szSearchKey, CC_Integer, 
                                        "coord_sys_code" );
                if( pszValue != NULL )
                {
                    pszFilename = CSVFilename( "coordinate_axis.csv" );
                    pszValue = CSVGetField( pszFilename, 
                                            "coord_sys_code", 
                                            pszValue, CC_Integer, 
                                            "uom_code" );
                    if( pszValue != NULL )
                        verticalUnits = (short) atoi(pszValue);
                }                
            }
        }

/* -------------------------------------------------------------------- */
/*      Setup VERT_CS with citation if present.                         */
/* -------------------------------------------------------------------- */
        oSRS.SetNode( "COMPD_CS|VERT_CS", citation );

/* -------------------------------------------------------------------- */
/*      Setup the vertical datum.                                       */
/* -------------------------------------------------------------------- */
        const char *pszVDatumName = "unknown";
        const char *pszVDatumType = "2005"; // CS_VD_GeoidModelDerived

        if( verticalDatum > 0 && verticalDatum != KvUserDefined )
        {
            pszFilename = CSVFilename( "datum.csv" );
            if( EQUAL(pszFilename,"datum.csv") )
                pszFilename = CSVFilename( "gdal_datum.csv" );

            sprintf( szSearchKey, "%d", verticalDatum );

            pszValue = CSVGetField( pszFilename,
                                    "DATUM_CODE", szSearchKey, CC_Integer,
                                    "DATUM_NAME" );
            if( pszValue != NULL && *pszValue != '\0' )
                pszVDatumName = pszValue;

            pszValue = CSVGetField( pszFilename,
                                    "DATUM_CODE", szSearchKey, CC_Integer,
                                    "DATUM_TYPE" );
            if( pszValue != NULL && EQUALN(pszValue,"geodetic",8) )
                pszVDatumType = "2002"; // CS_VD_Ellipsoidal

            // We unfortunately don't know how to identify other 
            // vertical datum types, particularly orthometric (2001). 
        }

        oSRS.SetNode( "COMPD_CS|VERT_CS|VERT_DATUM", pszVDatumName );
        oSRS.GetAttrNode( "COMPD_CS|VERT_CS|VERT_DATUM" )
            ->AddChild( new OGR_SRSNode( pszVDatumType ) );
        if( verticalDatum > 0 && verticalDatum != KvUserDefined )
            oSRS.SetAuthority( "COMPD_CS|VERT_CS|VERT_DATUM", "EPSG", 
                               verticalDatum );
    
/* -------------------------------------------------------------------- */
/*      Set the vertical units.                                         */
/* -------------------------------------------------------------------- */
        if( verticalUnits > 0 && verticalUnits != KvUserDefined 
            && verticalUnits != 9001 )
        {
            char szInMeters[128];

            pszFilename = CSVFilename("unit_of_measure.csv");
            
            // Name
            sprintf( szSearchKey, "%d", verticalUnits );
            pszValue = CSVGetField( pszFilename,
                                    "uom_code", szSearchKey, CC_Integer,
                                    "unit_of_meas_name" );
            if( pszValue == NULL )
                pszValue = "unknown";

            oSRS.SetNode( "COMPD_CS|VERT_CS|UNIT", pszValue );

            // Value
            double dfFactorB, dfFactorC;
            dfFactorB = GTIFAtof(
                CSVGetField( pszFilename, 
                             "uom_code", szSearchKey, CC_Integer,
                             "factor_b" ));
            dfFactorC = GTIFAtof(
                CSVGetField( pszFilename, 
                             "uom_code", szSearchKey, CC_Integer,
                             "factor_c" ));
            if( dfFactorB != 0.0 && dfFactorC != 0.0 )
                sprintf( szInMeters, "%.16g", dfFactorB / dfFactorC );
            else
                strcpy( szInMeters, "1" );


            oSRS.GetAttrNode( "COMPD_CS|VERT_CS|UNIT" )
                ->AddChild( new OGR_SRSNode( szInMeters ) );

            oSRS.SetAuthority( "COMPD_CS|VERT_CS|UNIT", "EPSG", verticalUnits);
        }
        else
        {
            oSRS.SetNode( "COMPD_CS|VERT_CS|UNIT", "metre" );
            oSRS.GetAttrNode( "COMPD_CS|VERT_CS|UNIT" )
                ->AddChild( new OGR_SRSNode( "1.0" ) );
            oSRS.SetAuthority( "COMPD_CS|VERT_CS|UNIT", "EPSG", 9001 );
        }

/* -------------------------------------------------------------------- */
/*      Set the axis and VERT_CS authority.                             */
/* -------------------------------------------------------------------- */
        oSRS.SetNode( "COMPD_CS|VERT_CS|AXIS", "Up" );
        oSRS.GetAttrNode( "COMPD_CS|VERT_CS|AXIS" )
            ->AddChild( new OGR_SRSNode( "UP" ) );
        
        if( verticalCSType > 0 && verticalCSType != KvUserDefined )
            oSRS.SetAuthority( "COMPD_CS|VERT_CS", "EPSG", verticalCSType );
    }
    
/* ==================================================================== */
/*      Return the WKT serialization of the object.                     */
/* ==================================================================== */
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
    OGRBoolean peStrStored = FALSE;    

    GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
               RasterPixelIsArea);

/* -------------------------------------------------------------------- */
/*      Create an OGRSpatialReference object corresponding to the       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();
    if( poSRS->importFromWkt((char **) &pszOGCWKT) != OGRERR_NONE )
    {
        delete poSRS;
        return FALSE;
    }

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

    if( poSRS->GetAuthorityName("PROJCS|UNIT") != NULL 
        && EQUAL(poSRS->GetAuthorityName("PROJCS|UNIT"),"EPSG")
        && poSRS->GetAttrNode( "PROJCS|UNIT" ) != poSRS->GetAttrNode("GEOGCS|UNIT") )
        nUOMLengthCode = atoi(poSRS->GetAuthorityCode("PROJCS|UNIT"));
    else if( (pszLinearUOMName != NULL
         && EQUAL(pszLinearUOMName,SRS_UL_FOOT))
        || fabs(dfLinearUOM-GTIFAtof(SRS_UL_FOOT_CONV)) < 0.0000001 )
        nUOMLengthCode = 9002; /* international foot */
    else if( (pszLinearUOMName != NULL
              && EQUAL(pszLinearUOMName,SRS_UL_US_FOOT))
             || ABS(dfLinearUOM-GTIFAtof(SRS_UL_US_FOOT_CONV)) < 0.0000001 )
        nUOMLengthCode = 9003; /* us survey foot */
    else if( fabs(dfLinearUOM-1.0) > 0.00000001 )
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
    int bWritePEString = FALSE;
    
    if( nPCS != KvUserDefined )
    {
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
        GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
    }
    else if( poSRS->IsGeocentric() )
    {
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1, 
                   ModelTypeGeocentric );
    }
    else if( pszProjection == NULL )
    {
        if( poSRS->IsGeographic() )
            GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                       ModelTypeGeographic);
        // otherwise, presumably something like LOCAL_CS.
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
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );

        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjStraightVertPoleLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjAzimuthAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_AZIMUTH, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjRectifiedGridAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtCenterGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }
    
    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
    {
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
        GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
        GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

        GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
                   CT_HotineObliqueMercatorAzimuthCenter );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjAzimuthAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_AZIMUTH, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjRectifiedGridAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtCenterGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );

        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        
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
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjFalseOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, 0.0 ) );
        
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
                   CT_LambertConfConic_1SP );

        GTIFKeySet(psGTIF, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }

    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
        GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
        GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

        GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1, 
                   CT_CylindricalEqualArea );

        GTIFKeySet(psGTIF, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) );
        
        GTIFKeySet(psGTIF, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) );
    }

    else
    {
        bWritePEString = TRUE;
    }

    // Note that VERTCS is an ESRI "spelling" of VERT_CS so we assume if
    // we find it that we should try to treat this as a PE string.
    bWritePEString |= (poSRS->GetAttrValue("VERTCS") != NULL);

    if( bWritePEString 
        && CSLTestBoolean( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                              "YES") ) )
    {
        /* Anyhing we can't map, we store as an ESRI PE string with a citation key */
        char *pszPEString = NULL;
        poSRS->morphToESRI();
        poSRS->exportToWkt( &pszPEString );
        int peStrLen = strlen(pszPEString);
        if(peStrLen > 0)
        {
            char *outPeStr = (char *) CPLMalloc( peStrLen + strlen("ESRI PE String = ")+1 );
            strcpy(outPeStr, "ESRI PE String = "); 
            strcat(outPeStr, pszPEString); 
            GTIFKeySet( psGTIF, PCSCitationGeoKey, TYPE_ASCII, 0, outPeStr ); 
            peStrStored = TRUE;
            CPLFree( outPeStr );
        }
        if(pszPEString)
            CPLFree( pszPEString );
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
    }
    
/* -------------------------------------------------------------------- */
/*      Is there a false easting/northing set?  If so, write out a      */
/*      special geokey tag to indicate that GDAL has written these      */
/*      with the proper interpretation of the linear units.             */
/* -------------------------------------------------------------------- */
    double dfFE = 0.0, dfFN = 0.0;

    if( (GTIFKeyGet(psGTIF, ProjFalseEastingGeoKey, &dfFE, 0, 1)
         || GTIFKeyGet(psGTIF, ProjFalseNorthingGeoKey, &dfFN, 0, 1)
         || GTIFKeyGet(psGTIF, ProjFalseOriginEastingGeoKey, &dfFE, 0, 1)
         || GTIFKeyGet(psGTIF, ProjFalseOriginNorthingGeoKey, &dfFN, 0, 1))
        && (dfFE != 0.0 || dfFN != 0.0) 
        && nUOMLengthCode != 9001 )
    {
        GTIFKeySet(psGTIF, (geokey_t) ProjLinearUnitsInterpCorrectGeoKey, 
                   TYPE_SHORT, 1, (short) 1 );
    }

/* -------------------------------------------------------------------- */
/*      Write linear units information.                                 */
/* -------------------------------------------------------------------- */
    if( poSRS->IsGeocentric() )
    {
        GTIFKeySet(psGTIF, GeogLinearUnitsGeoKey, TYPE_SHORT, 1, 
                   nUOMLengthCode );
        if( nUOMLengthCode == KvUserDefined )
            GTIFKeySet( psGTIF, GeogLinearUnitSizeGeoKey, TYPE_DOUBLE, 1, 
                        dfLinearUOM);
    }
    else if( !poSRS->IsGeographic() )
    {
        GTIFKeySet(psGTIF, ProjLinearUnitsGeoKey, TYPE_SHORT, 1, 
                   nUOMLengthCode );
        if( nUOMLengthCode == KvUserDefined )
            GTIFKeySet( psGTIF, ProjLinearUnitSizeGeoKey, TYPE_DOUBLE, 1, 
                        dfLinearUOM);

        /* if linear units name is available and user defined, store it as citation */
        if(!peStrStored 
           && nUOMLengthCode == KvUserDefined 
           && pszLinearUOMName 
           && strlen(pszLinearUOMName)>0
           && CSLTestBoolean( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                                 "YES") ) )
        {
            SetLinearUnitCitation(psGTIF, pszLinearUOMName);
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Write angular units.  Always Degrees for now.                   */
/*   Changed to support different angular units                         */
/* -------------------------------------------------------------------- */

    char* angUnitName = NULL;
    double angUnitValue = poSRS->GetAngularUnits(&angUnitName);
    if(EQUAL(angUnitName, "Degree"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1, 
                   Angular_Degree );
    else if(angUnitName)
    {
        GTIFKeySet(psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0, 
                   angUnitName ); // it may be rewritten if the gcs is userdefined 
        GTIFKeySet(psGTIF, GeogAngularUnitSizeGeoKey, TYPE_DOUBLE, 1, 
                   angUnitValue );
    }

/* -------------------------------------------------------------------- */
/*      Try to write a citation from the main coordinate system         */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    if( poSRS->GetRoot() != NULL
        && poSRS->GetRoot()->GetChild(0) != NULL 
        && (poSRS->IsProjected() || poSRS->IsLocal() || poSRS->IsGeocentric()) )
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
            if( dfInvFlattening == 0.0 )
                GTIFKeySet( psGTIF, GeogSemiMinorAxisGeoKey, TYPE_DOUBLE, 1,
                            dfSemiMajor );
            else
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

        /* Always set InvFlattening if it is avaliable.  */
        /* So that it doesn'tneed to calculate from SemiMinor */
        if( dfInvFlattening != 0.0 )
            GTIFKeySet( psGTIF, GeogInvFlatteningGeoKey, TYPE_DOUBLE, 1,
                        dfInvFlattening );
        /* Always set SemiMajor to keep the precision and in case of editing */
        if( dfSemiMajor != 0.0 )
            GTIFKeySet( psGTIF, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1,
                        dfSemiMajor );

        if( nGCS == KvUserDefined 
            && CSLTestBoolean( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                                  "YES") ) )
            SetGeogCSCitation(psGTIF, poSRS, angUnitName, nDatum, nSpheroid);
    }

/* -------------------------------------------------------------------- */
/*      Do we have TOWGS84 parameters?                                  */
/* -------------------------------------------------------------------- */

#if LIBGEOTIFF_VERSION >= 1310 && !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    double adfTOWGS84[7];

    if( poSRS->GetTOWGS84( adfTOWGS84 ) == OGRERR_NONE )
    {
        if( adfTOWGS84[3] == 0.0 && adfTOWGS84[4] == 0.0
            && adfTOWGS84[5] == 0.0 && adfTOWGS84[6] == 0.0 )
        {
            if( nGCS == GCS_WGS_84 && adfTOWGS84[0] == 0.0
                && adfTOWGS84[1] == 0.0 && adfTOWGS84[2] == 0.0 )
            {
                ; /* do nothing */
            }
            else
                GTIFKeySet( psGTIF, GeogTOWGS84GeoKey, TYPE_DOUBLE, 3,
                            adfTOWGS84 );
        }
        else
            GTIFKeySet( psGTIF, GeogTOWGS84GeoKey, TYPE_DOUBLE, 7, 
                        adfTOWGS84 );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Do we have vertical datum information to set?                   */
/* -------------------------------------------------------------------- */
    if( poSRS->GetAttrValue( "COMPD_CS|VERT_CS" ) != NULL )
    {
        const char *pszValue;

        GTIFKeySet( psGTIF, VerticalCitationGeoKey, TYPE_ASCII, 0, 
                    poSRS->GetAttrValue( "COMPD_CS|VERT_CS" ) );

        pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS" );
        if( pszValue && atoi(pszValue) )
            GTIFKeySet( psGTIF, VerticalCSTypeGeoKey, TYPE_SHORT, 1,
                        atoi(pszValue) );
        
        pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS|VERT_DATUM" );
        if( pszValue && atoi(pszValue) )
            GTIFKeySet( psGTIF, VerticalDatumGeoKey, TYPE_SHORT, 1,
                        atoi(pszValue) );
        
        pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS|UNIT" );
        if( pszValue && atoi(pszValue) )
            GTIFKeySet( psGTIF, VerticalUnitsGeoKey, TYPE_SHORT, 1,
                        atoi(pszValue) );
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
    char szFilename[100];

    sprintf( szFilename, "/vsimem/wkt_from_mem_buf_%ld.tif",
             (long) CPLGetPID() );

/* -------------------------------------------------------------------- */
/*      Create a memory file from the buffer.                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFileFromMemBuffer( szFilename, pabyBuffer, nSize, FALSE );
    if( fp == NULL )
        return CE_Failure;
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    TIFF        *hTIFF;
    hTIFF = VSI_TIFFOpen( szFilename, "rc" );

    if( hTIFF == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        VSIUnlink( szFilename );
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Get the projection definition.                                  */
/* -------------------------------------------------------------------- */
    GTIF 	*hGTIF;
    GTIFDefn    *psGTIFDefn;

    hGTIF = GTIFNew(hTIFF);

#if LIBGEOTIFF_VERSION >= 1410
    psGTIFDefn = GTIFAllocDefn();
#else
    psGTIFDefn = (GTIFDefn *) CPLCalloc(1,sizeof(GTIFDefn));
#endif    

    if( hGTIF != NULL && GTIFGetDefn( hGTIF, psGTIFDefn ) )
        *ppszWKT = GTIFGetOGISDefn( hGTIF, psGTIFDefn );
    else
        *ppszWKT = NULL;
    
    if( hGTIF )
        GTIFFree( hGTIF );
    
#if LIBGEOTIFF_VERSION >= 1410
    GTIFFreeDefn(psGTIFDefn);
#else
    CPLFree(psGTIFDefn);
#endif    

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
            psGCP->pszInfo = CPLStrdup("");
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

    VSIUnlink( szFilename );

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
    TIFF        *hTIFF;
    GTIF 	*hGTIF;
    char        szFilename[100];

    sprintf( szFilename, "/vsimem/wkt_from_mem_buf_%ld.tif", 
             (long) CPLGetPID() );

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    hTIFF = VSI_TIFFOpen( szFilename, "w" );

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

/* -------------------------------------------------------------------- */
/*      Read back from the memory buffer.  It would be preferrable      */
/*      to be able to "steal" the memory buffer, but there isn't        */
/*      currently any support for this.                                 */
/* -------------------------------------------------------------------- */
    GUIntBig nBigLength;

    *ppabyBuffer = VSIGetMemFileBuffer( szFilename, &nBigLength, TRUE );
    *pnSize = (int) nBigLength;

    return CE_None;
}
