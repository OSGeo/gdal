/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gt_wkt_srs.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#if HAVE_CXX11 && !defined(__MINGW32__)
#define HAVE_CXX11_MUTEX 1
#endif
#if HAVE_CXX11_MUTEX
#include <mutex>
#endif

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_csv.h"
#include "gt_citation.h"
#include "gt_wkt_srs_for_gdal.h"
#include "gt_wkt_srs_priv.h"
#include "gtiff.h"
#include "gdal.h"
#include "geokeys.h"
#include "geovalues.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "tiff.h"
#include "tiffio.h"
#include "tifvsi.h"
#include "xtiffio.h"

CPL_CVSID("$Id$")

static const geokey_t ProjLinearUnitsInterpCorrectGeoKey =
    static_cast<geokey_t>(3059);

#ifndef CT_HotineObliqueMercatorAzimuthCenter
#  define CT_HotineObliqueMercatorAzimuthCenter 9815
#endif

#if !defined(GTIFAtof)
#  define GTIFAtof CPLAtof
#endif

CPL_C_START
#ifndef INTERNAL_LIBGEOTIFF
void CPL_DLL gtSetCSVFilenameHook( const char *(*)(const char *) );
#define SetCSVFilenameHook gtSetCSVFilenameHook
#endif
CPL_C_END

// To remind myself not to use CPLString in this file!
#define CPLString Please_do_not_use_CPLString_in_this_file

static const char * const papszDatumEquiv[] =
{
  "Militar_Geographische_Institut", "Militar_Geographische_Institute",
  "World_Geodetic_System_1984", "WGS_1984",
  "WGS_72_Transit_Broadcast_Ephemeris", "WGS_1972_Transit_Broadcast_Ephemeris",
  "World_Geodetic_System_1972", "WGS_1972",
  "European_Terrestrial_Reference_System_89", "European_Reference_System_1989",
  "D_North_American_1927", "North_American_Datum_1927", // #6863
  NULL
};

// Older libgeotiff's won't list this.
#ifndef CT_CylindricalEqualArea
# define CT_CylindricalEqualArea 28
#endif

/************************************************************************/
/*                       LibgeotiffOneTimeInit()                        */
/************************************************************************/

#if HAVE_CXX11_MUTEX
static std::mutex oDeleteMutex;
#else
static CPLMutex* hMutex = NULL;
#endif  // HAVE_CXX11_MUTEX

void LibgeotiffOneTimeInit()
{
#if HAVE_CXX11_MUTEX
    std::lock_guard<std::mutex> oLock(oDeleteMutex);
#else
    CPLMutexHolder oHolder( &hMutex);
#endif  // HAVE_CXX11_MUTEX

    static bool bOneTimeInitDone = false;

    if( bOneTimeInitDone )
        return;

    bOneTimeInitDone = true;

    // If linking with an external libgeotiff we hope this will call the
    // SetCSVFilenameHook() in libgeotiff, not the one in gdal/port!
    SetCSVFilenameHook( GDALDefaultCSVFilename );

    // This isn't thread-safe, so better do it now
    XTIFFInitialize();
}

/************************************************************************/
/*                   LibgeotiffOneTimeCleanupMutex()                    */
/************************************************************************/

void LibgeotiffOneTimeCleanupMutex()
{
#if !HAVE_CXX11_MUTEX
    // >= C++11 uses a lock_guard that does not need cleanup.
    if( hMutex == NULL )
        return;

    CPLDestroyMutex(hMutex);
    hMutex = NULL;
#endif
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
    char *pszDatum = *ppszDatum;
    if( !pszDatum || pszDatum[0] == '\0' )
        return;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( int i = 0; pszDatum[i] != '\0'; i++ )
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
    int j = 0;  // Used after for.
    for( int i = 1; pszDatum[i] != '\0'; i++ )
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
/*      Search for datum equivalences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszDatumEquiv[i] != NULL; i += 2 )
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
   GTCitationGeoKey (Ascii,215): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision: 34309 $ $Date: 2016-05-29 11:29:40 -0700 (Sun, 29 May 2016) $\nProjection Name = UTM\nUnits = meters\nGeoTIFF Units = meters"

   GeogCitationGeoKey (Ascii,267): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision: 34309 $ $Date: 2016-05-29 11:29:40 -0700 (Sun, 29 May 2016) $\nUnable to match Ellipsoid (Datum) to a GeographicTypeGeoKey value\nEllipsoid = Clarke 1866\nDatum = NAD27 (CONUS)"

   PCSCitationGeoKey (Ascii,214): "IMAGINE GeoTIFF Support\nCopyright 1991 - 2001 by ERDAS, Inc. All Rights Reserved\n@(#)$RCSfile$ $Revision: 34309 $ $Date: 2016-05-29 11:29:40 -0700 (Sun, 29 May 2016) $\nUTM Zone 10N\nEllipsoid = Clarke 1866\nDatum = NAD27 (CONUS)"
*/

static void GTIFCleanupImagineNames( char *pszCitation )

{
    if( strstr(pszCitation,"IMAGINE GeoTIFF") == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      First, we skip past all the copyright, and RCS stuff.  We       */
/*      assume that this will have a "$" at the end of it all.          */
/* -------------------------------------------------------------------- */
    char *pszSkip = pszCitation + strlen(pszCitation) - 1;

    for( ;
         pszSkip != pszCitation && *pszSkip != '$';
         pszSkip-- ) {}

    if( *pszSkip == '$' )
        pszSkip++;
    if( *pszSkip == '\n' )
        pszSkip++;

    memmove( pszCitation, pszSkip, strlen(pszSkip)+1 );

/* -------------------------------------------------------------------- */
/*      Convert any newlines into spaces, they really gum up the        */
/*      WKT.                                                            */
/* -------------------------------------------------------------------- */
    for( int i = 0; pszCitation[i] != '\0'; i++ )
    {
        if( pszCitation[i] == '\n' )
            pszCitation[i] = ' ';
    }
}

/************************************************************************/
/*                       GDALGTIFKeyGet()                               */
/************************************************************************/

static int GDALGTIFKeyGet( GTIF *hGTIF, geokey_t key,
                           void* pData,
                           int nIndex,
                           int nCount,
                           tagtype_t expected_tagtype )
{
    tagtype_t tagtype = TYPE_UNKNOWN;
    if( !GTIFKeyInfo(hGTIF, key, NULL, &tagtype) )
        return 0;
    if( tagtype != expected_tagtype )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Expected key %s to be of type %s. Got %s",
                  GTIFKeyName(key), GTIFTypeName(expected_tagtype),
                  GTIFTypeName(tagtype) );
        return 0;
    }
    return GTIFKeyGet( hGTIF, key, pData, nIndex, nCount );
}

/************************************************************************/
/*                       GDALGTIFKeyGetASCII()                          */
/************************************************************************/

int GDALGTIFKeyGetASCII( GTIF *hGTIF, geokey_t key,
                         char* szStr,
                         int nIndex,
                         int szStrMaxLen )
{
    CPLAssert(nIndex == 0);
    return GDALGTIFKeyGet( hGTIF, key, szStr, nIndex, szStrMaxLen, TYPE_ASCII );
}

/************************************************************************/
/*                       GDALGTIFKeyGetSHORT()                          */
/************************************************************************/

int GDALGTIFKeyGetSHORT( GTIF *hGTIF, geokey_t key,
                         short* pnVal,
                         int nIndex,
                         int nCount )
{
    return GDALGTIFKeyGet( hGTIF, key, pnVal, nIndex, nCount, TYPE_SHORT );
}

/************************************************************************/
/*                        GDALGTIFKeyGetDOUBLE()                        */
/************************************************************************/

int GDALGTIFKeyGetDOUBLE( GTIF *hGTIF, geokey_t key,
                          double* pdfVal,
                          int nIndex,
                          int nCount )
{
    return GDALGTIFKeyGet( hGTIF, key, pdfVal, nIndex, nCount, TYPE_DOUBLE );
}

/************************************************************************/
/*                          GTIFGetOGISDefn()                           */
/************************************************************************/

char *GTIFGetOGISDefn( GTIF *hGTIF, GTIFDefn * psDefn )

{
    OGRSpatialReference oSRS;

/* -------------------------------------------------------------------- */
/*      Make sure we have hooked CSVFilename().                         */
/* -------------------------------------------------------------------- */
    LibgeotiffOneTimeInit();

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
        char *pszWKT = NULL;
        char szPeStr[2400] = { '\0' };

        /** check if there is a pe string citation key **/
        if( GDALGTIFKeyGetASCII( hGTIF, PCSCitationGeoKey, szPeStr,
                                 0, sizeof(szPeStr) ) &&
            strstr(szPeStr, "ESRI PE String = " ) )
        {
            pszWKT = CPLStrdup( szPeStr + strlen("ESRI PE String = ") );

            if( strstr( pszWKT,
                        "PROJCS[\"WGS_1984_Web_Mercator_Auxiliary_Sphere\"" ) )
            {
                oSRS.SetFromUserInput(pszWKT);
                oSRS.SetExtension(
                    "PROJCS", "PROJ4",
                    "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                    "+x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null "
                    "+wktext  +no_defs" );  // TODO(schwehr): Why 2 spaces?
                oSRS.FixupOrdering();
                CPLFree(pszWKT);
                pszWKT = NULL;
                oSRS.exportToWkt(&pszWKT);
            }

            return pszWKT;
        }
        else
        {
            char *pszUnitsName = NULL;
            char szPCSName[300] = { '\0' };
            int nKeyCount = 0;
            int anVersion[3] = { 0 };

            GTIFDirectoryInfo( hGTIF, anVersion, &nKeyCount );

            if( nKeyCount > 0 ) // Use LOCAL_CS if we have any geokeys at all.
            {
                // Handle citation.
                strcpy( szPCSName, "unnamed" );
                if( !GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szPCSName,
                                          0, sizeof(szPCSName) ) )
                    GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szPCSName,
                                         0, sizeof(szPCSName) );

                GTIFCleanupImagineNames( szPCSName );
                oSRS.SetLocalCS( szPCSName );

                // Handle units
                GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );

                if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
                {
                    oSRS.SetLinearUnits( pszUnitsName,
                                         psDefn->UOMLengthInMeters );
                    oSRS.SetAuthority( "LOCAL_CS|UNIT", "EPSG",
                                       psDefn->UOMLength );
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
        char szName[300] = { '\0' };

        strcpy( szName, "unnamed" );
        if( !GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szName,
                                  0, sizeof(szName) ) )
            GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szName,
                                 0, sizeof(szName) );

        oSRS.SetGeocCS( szName );

        char *pszUnitsName = NULL;

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
    const char *pszLinearUnits =
        CPLGetConfigOption( "GTIFF_LINEAR_UNITS", "DEFAULT" );

#if LIBGEOTIFF_VERSION <= 1300
    if( EQUAL(pszLinearUnits,"DEFAULT") && psDefn->Projection == KvUserDefined )
    {
        for( int iParm = 0; iParm < psDefn->nParms; iParm++ )
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
                    CPLDebug(
                        "GTIFF",
                        "Converting geokey to meters to fix bug in "
                        "old libgeotiff" );
                }
                break;

              default:
                break;
            }
        }
    }
#endif  // LIBGEOTIFF_VERSION <= 1300

/* -------------------------------------------------------------------- */
/*      #3901: If folks have broken GeoTIFF files generated with        */
/*      older versions of GDAL+libgeotiff, then they may need a         */
/*      hack to allow them to be read properly.  This is that           */
/*      hack.  We basically try to undue the conversion applied by      */
/*      libgeotiff to meters (or above) to simulate the old             */
/*      behavior.                                                       */
/* -------------------------------------------------------------------- */
    short bLinearUnitsMarkedCorrect = FALSE;

    GDALGTIFKeyGetSHORT(hGTIF, ProjLinearUnitsInterpCorrectGeoKey,
               &bLinearUnitsMarkedCorrect, 0, 1);

    if( EQUAL(pszLinearUnits,"BROKEN")
        && psDefn->Projection == KvUserDefined
        && !bLinearUnitsMarkedCorrect )
    {
        for( int iParm = 0; iParm < psDefn->nParms; iParm++ )
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
                    CPLDebug(
                        "GTIFF",
                        "Converting geokey to accommodate old broken file "
                        "due to GTIFF_LINEAR_UNITS=BROKEN setting." );
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
        char szCTString[512] = { '\0' };
        if( psDefn->PCS != KvUserDefined )
        {
            char *pszPCSName = NULL;

            GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL );

            oSRS.SetNode( "PROJCS", pszPCSName ? pszPCSName : "unnamed" );
            if ( pszPCSName )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetAuthority( "PROJCS", "EPSG", psDefn->PCS );
        }
        else
        {
            bool bTryGTCitationGeoKey = true;
            if( GDALGTIFKeyGetASCII( hGTIF, PCSCitationGeoKey,
                                              szCTString, 0,
                                              sizeof(szCTString)) )
            {
                bTryGTCitationGeoKey = false;
                if (!SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                                       PCSCitationGeoKey, &oSRS,
                                       &linearUnitIsSet) )
                {
                    if( !STARTS_WITH_CI(szCTString, "LUnits = ") )
                    {
                        oSRS.SetNode( "PROJCS",szCTString );
                    }
                    else
                    {
                        bTryGTCitationGeoKey = true;
                    }
                }
            }

            if( bTryGTCitationGeoKey &&
                GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                                     0, sizeof(szCTString) ) )
            {
                if( !SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                                       GTCitationGeoKey, &oSRS,
                                       &linearUnitIsSet ) )
                    oSRS.SetNode( "PROJCS", szCTString );
            }
            else
            {
                oSRS.SetNode( "PROJCS", "unnamed" );
            }
        }

        /* Handle ESRI/Erdas style state plane and UTM in citation key */
        if( CheckCitationKeyForStatePlaneUTM( hGTIF, psDefn, &oSRS,
                                              &linearUnitIsSet ) )
        {
            oSRS.morphFromESRI();
            oSRS.FixupOrdering();
            char *pszWKT = NULL;
            if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
                return pszWKT;
        }

        /* Handle ESRI PE string in citation */
        szCTString[0] = '\0';
        if( GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                                 0, sizeof(szCTString) ) )
            SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                              GTCitationGeoKey, &oSRS, &linearUnitIsSet );
    }

/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char *pszGeogName = NULL;
    char *pszDatumName = NULL;
    char *pszPMName = NULL;
    char *pszSpheroidName = NULL;
    char *pszAngularUnits = NULL;
    char szGCSName[512] = { '\0' };

    if( !GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL )
        && GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szGCSName, 0,
                       sizeof(szGCSName)) )
    {
        GetGeogCSFromCitation(szGCSName, sizeof(szGCSName),
                              GeogCitationGeoKey,
                              &pszGeogName, &pszDatumName,
                              &pszPMName, &pszSpheroidName,
                              &pszAngularUnits);
    }
    else
    {
        GTIFToCPLRecycleString( &pszGeogName );
    }

    if( !pszDatumName )
    {
        GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
        GTIFToCPLRecycleString( &pszDatumName );
    }

    double dfSemiMajor = 0.0;
    double dfInvFlattening = 0.0;
    if( !pszSpheroidName )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
        GTIFToCPLRecycleString( &pszSpheroidName );
    }
    else
    {
        GDALGTIFKeyGetDOUBLE( hGTIF, GeogSemiMajorAxisGeoKey,
                              &(psDefn->SemiMajor), 0, 1 );
        GDALGTIFKeyGetDOUBLE( hGTIF, GeogInvFlatteningGeoKey,
                              &dfInvFlattening, 0, 1 );
    }
    if( !pszPMName )
    {
        GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
        GTIFToCPLRecycleString( &pszPMName );
    }
    else
    {
        GDALGTIFKeyGetDOUBLE( hGTIF, GeogPrimeMeridianLongGeoKey,
                              &(psDefn->PMLongToGreenwich), 0, 1 );
    }

    bool aUnitGot = false;
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
        double dfRadians = 0.0;
        if( GDALGTIFKeyGetDOUBLE(hGTIF, GeogAngularUnitSizeGeoKey, &dfRadians,
                                 0, 1) )
        {
            aUnitGot = true;
            psDefn->UOMAngleInDegrees = dfRadians / CPLAtof(SRS_UA_DEGREE_CONV);
        }
    }

    if( pszDatumName != NULL )
        WKTMassageDatum( &pszDatumName );

    dfSemiMajor = psDefn->SemiMajor;
    if( dfSemiMajor == 0.0 )
    {
        CPLFree(pszSpheroidName);
        pszSpheroidName = CPLStrdup("unretrievable - using WGS84");
        dfSemiMajor = SRS_WGS84_SEMIMAJOR;
        dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    else if( dfInvFlattening == 0.0
             && ((psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
                 || (psDefn->SemiMinor / psDefn->SemiMajor) >
                 1.00000000000000001 ) )
    {
        dfInvFlattening = OSRCalcInvFlattening( psDefn->SemiMajor,
                                                psDefn->SemiMinor );

        /* Take official inverse flattening definition in the WGS84 case */
        if (fabs(dfSemiMajor - SRS_WGS84_SEMIMAJOR) < 1e-10 &&
            fabs(dfInvFlattening - SRS_WGS84_INVFLATTENING) < 1e-10)
            dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    if(!pszGeogName || strlen(pszGeogName) == 0)
    {
        CPLFree(pszGeogName);
        pszGeogName = CPLStrdup( pszDatumName ? pszDatumName : "unknown" );
    }

    oSRS.SetGeogCS( pszGeogName, pszDatumName,
                    pszSpheroidName, dfSemiMajor, dfInvFlattening,
                    pszPMName,
                    psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                    pszAngularUnits,
                    psDefn->UOMAngleInDegrees * CPLAtof(SRS_UA_DEGREE_CONV) );

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

/* -------------------------------------------------------------------- */
/*      Set projection units if not yet done                            */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected && !linearUnitIsSet )
    {
        char *pszUnitsName = NULL;

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

/* ==================================================================== */
/*      Try to import PROJCS from ProjectedCSTypeGeoKey if we           */
/*      have essentially only it. We could relax a bit the constraints  */
/*      but that should do for now. This may mask shortcomings in the   */
/*      libgeotiff GTIFGetDefn() function.                              */
/* ==================================================================== */
    short tmp = 0;
    bool bGotFromEPSG = false;
    if( psDefn->Model == ModelTypeProjected &&
        psDefn->PCS != KvUserDefined &&
        GDALGTIFKeyGetSHORT(hGTIF, ProjectionGeoKey, &tmp, 0, 1) == 0 &&
        GDALGTIFKeyGetSHORT(hGTIF, ProjCoordTransGeoKey, &tmp, 0, 1) == 0 &&
        GDALGTIFKeyGetSHORT(hGTIF, GeographicTypeGeoKey, &tmp, 0, 1) == 0 &&
        GDALGTIFKeyGetSHORT(hGTIF, GeogGeodeticDatumGeoKey, &tmp, 0, 1) == 0 &&
        GDALGTIFKeyGetSHORT(hGTIF, GeogEllipsoidGeoKey, &tmp, 0, 1) == 0 &&
        CPLTestBool(CPLGetConfigOption("GTIFF_IMPORT_FROM_EPSG", "YES")) )
    {
        // Save error state as importFromEPSGA() will call CPLReset()
        CPLErrorNum errNo = CPLGetLastErrorNo();
        CPLErr eErr = CPLGetLastErrorType();
        const char* pszTmp = CPLGetLastErrorMsg();
        char* pszLastErrorMsg = CPLStrdup(pszTmp ? pszTmp : "");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRSpatialReference oSRSTmp;
        OGRErr eImportErr = oSRSTmp.importFromEPSG(psDefn->PCS);
        CPLPopErrorHandler();
        // Restore error state
        CPLErrorSetState( eErr, errNo, pszLastErrorMsg);
        CPLFree(pszLastErrorMsg);
        bGotFromEPSG = eImportErr == OGRERR_NONE;

        if( bGotFromEPSG )
        {
            // See #6210. In case there's an overridden linear units, take it
            // into account
            char* pszUnitsName = NULL;
            double dfUOMLengthInMeters = oSRS.GetLinearUnits( &pszUnitsName );
            if( dfUOMLengthInMeters != oSRSTmp.GetLinearUnits(NULL) )
            {
                CPLDebug( "GTiff", "Modify EPSG:%d to have %s linear units...",
                          psDefn->PCS,
                          pszUnitsName ? pszUnitsName : "unknown" );

                if( pszUnitsName )
                    oSRSTmp.SetLinearUnitsAndUpdateParameters(
                        pszUnitsName, dfUOMLengthInMeters );

                const char* pszAuthorityCode =
                    oSRS.GetAuthorityCode( "PROJCS|UNIT" );
                const char* pszAuthorityName =
                    oSRS.GetAuthorityName( "PROJCS|UNIT" );
                if( pszAuthorityCode && pszAuthorityName )
                    oSRSTmp.SetAuthority( "PROJCS|UNIT", pszAuthorityName,
                                          atoi(pszAuthorityCode) );

                if( oSRSTmp.GetRoot()->FindChild( "AUTHORITY" ) != -1 )
                    oSRSTmp.GetRoot()->DestroyChild( oSRSTmp.GetRoot()->
                                                     FindChild( "AUTHORITY" ) );
            }

            oSRS = oSRSTmp;
        }
    }

/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected && !bGotFromEPSG )
    {
/* -------------------------------------------------------------------- */
/*      Make a local copy of parms, and convert back into the           */
/*      angular units of the GEOGCS and the linear units of the         */
/*      projection.                                                     */
/* -------------------------------------------------------------------- */
        double adfParm[10] = { 0.0 };
        int i = 0;  // Used after for.

        for( ; i < std::min(10, psDefn->nParms); i++ )
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
            // If a lat_ts was specified use 2SP, otherwise use 1SP.
            if (psDefn->ProjParmId[2] == ProjStdParallel1GeoKey)
            {
                if (psDefn->ProjParmId[4] == ProjScaleAtNatOriginGeoKey)
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Mercator projection should not define "
                        "both StdParallel1 and ScaleAtNatOrigin.  "
                        "Using StdParallel1 and ignoring ScaleAtNatOrigin." );
                oSRS.SetMercator2SP( adfParm[2],
                                     adfParm[0], adfParm[1],
                                     adfParm[5], adfParm[6]);
            }
            else
                oSRS.SetMercator( adfParm[0], adfParm[1],
                                  adfParm[4],
                                  adfParm[5], adfParm[6] );

            // Override hack for google mercator.
            if (psDefn->Projection == 1024 || psDefn->Projection == 9841)
            {
                oSRS.SetExtension(
                    "PROJCS", "PROJ4",
                    "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                    "+x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null "
                    "+wktext  +no_defs" );  // TODO(schwehr): Why 2 spaces?
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

          case CT_ObliqueMercator:  // Hotine.
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
    }

    if( oSRS.IsProjected())
    {
        // Hack to be able to read properly what we have written for
        // EPSG:102113 (ESRI ancient WebMercator).
        if( EQUAL(oSRS.GetAttrValue("PROJCS"), "WGS_1984_Web_Mercator") )
            oSRS.importFromEPSG(102113);
        // And for EPSG:900913
        else if( EQUAL( oSRS.GetAttrValue("PROJCS"),
                        "Google Maps Global Mercator" ) )
            oSRS.importFromEPSG(900913);
    }

/* ==================================================================== */
/*      Handle vertical coordinate system information if we have it.    */
/* ==================================================================== */
    short verticalCSType = -1;
    short verticalDatum = -1;
    short verticalUnits = -1;
    const char *pszFilename = NULL;
    const char *pszValue = NULL;
    char szSearchKey[128] = { '\0' };
    bool bNeedManualVertCS = false;
    char citation[2048] = { '\0' };

    // Don't do anything if there is no apparent vertical information.
    GDALGTIFKeyGetSHORT( hGTIF, VerticalCSTypeGeoKey, &verticalCSType, 0, 1 );
    GDALGTIFKeyGetSHORT( hGTIF, VerticalDatumGeoKey, &verticalDatum, 0, 1 );
    GDALGTIFKeyGetSHORT( hGTIF, VerticalUnitsGeoKey, &verticalUnits, 0, 1 );

    if( (verticalCSType != -1 || verticalDatum != -1 || verticalUnits != -1)
        && (oSRS.IsGeographic() || oSRS.IsProjected() || oSRS.IsLocal()) )
    {
        if( !GDALGTIFKeyGetASCII( hGTIF, VerticalCitationGeoKey, citation,
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
/*      is misused as a Vertical CS code (#4922).                       */
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
/*      there isn't one in EPSG.                                        */
/* -------------------------------------------------------------------- */
        if( (verticalCSType >= 5001 && verticalCSType <= 5033)
            && verticalDatum == -1 )
        {
            verticalDatum = verticalCSType + 1000;
            verticalCSType = -1;
        }

/* -------------------------------------------------------------------- */
/*      Promote to being a compound coordinate system.                  */
/* -------------------------------------------------------------------- */
        OGR_SRSNode *poOldRoot = oSRS.GetRoot()->Clone();

        oSRS.Clear();

/* -------------------------------------------------------------------- */
/*      Set COMPD_CS name.                                              */
/* -------------------------------------------------------------------- */
        char szCTString[512];
        szCTString[0] = '\0';
        if( GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                                 0, sizeof(szCTString) ) &&
            strstr( szCTString, " = " ) == NULL )
        {
            oSRS.SetNode( "COMPD_CS", szCTString );
        }
        else
        {
            oSRS.SetNode( "COMPD_CS", "unknown" );
        }

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
            snprintf( szSearchKey, sizeof(szSearchKey), "%d", verticalCSType );

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
                    snprintf( citation, sizeof(citation), "%s", pszValue );
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
            pszFilename = CSVFilename( "gdal_datum.csv" );
            if( EQUAL(pszFilename,"gdal_datum.csv") )
            {
                // Fallback to see if libgeotiff datum.csv is available.
                // TODO(schwehr): Can we drop searching for datum.csv?
                // See #6531.
                pszFilename = CSVFilename( "datum.csv" );
            }

            snprintf( szSearchKey, sizeof(szSearchKey), "%d", verticalDatum );

            pszValue = CSVGetField( pszFilename,
                                    "DATUM_CODE", szSearchKey, CC_Integer,
                                    "DATUM_NAME" );
            if( pszValue != NULL && *pszValue != '\0' )
                pszVDatumName = pszValue;

            pszValue = CSVGetField( pszFilename,
                                    "DATUM_CODE", szSearchKey, CC_Integer,
                                    "DATUM_TYPE" );
            if( pszValue != NULL && STARTS_WITH_CI(pszValue, "geodetic") )
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
            char szInMeters[128] = {};

            pszFilename = CSVFilename("unit_of_measure.csv");

            // Name.
            snprintf( szSearchKey, sizeof(szSearchKey), "%d", verticalUnits );
            pszValue = CSVGetField( pszFilename,
                                    "uom_code", szSearchKey, CC_Integer,
                                    "unit_of_meas_name" );
            if( pszValue == NULL )
                pszValue = "unknown";

            oSRS.SetNode( "COMPD_CS|VERT_CS|UNIT", pszValue );

            // Value.
            const double dfFactorB = GTIFAtof(
                CSVGetField( pszFilename,
                             "uom_code", szSearchKey, CC_Integer,
                             "factor_b" ));
            const double dfFactorC = GTIFAtof(
                CSVGetField( pszFilename,
                             "uom_code", szSearchKey, CC_Integer,
                             "factor_c" ));
            if( dfFactorB != 0.0 && dfFactorC != 0.0 )
                CPLsnprintf( szInMeters, sizeof(szInMeters),
                             "%.16g", dfFactorB / dfFactorC );
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
    oSRS.FixupOrdering();

    char *pszWKT = NULL;
    if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
        return pszWKT;

    return NULL;
}

/************************************************************************/
/*                     OGCDatumName2EPSGDatumCode()                     */
/************************************************************************/

static int OGCDatumName2EPSGDatumCode( const char * pszOGCName )

{
    char **papszTokens = NULL;
    int nReturn = KvUserDefined;

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
    VSILFILE *fp = VSIFOpenL( CSVFilename("gdal_datum.csv"), "r" );
    if( fp == NULL )
        fp = VSIFOpenL( CSVFilename("datum.csv"), "r" );

    if( fp == NULL )
        return nReturn;

/* -------------------------------------------------------------------- */
/*      Discard the first line with field names.                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( CSVReadParseLineL( fp ) );

/* -------------------------------------------------------------------- */
/*      Read lines looking for our datum.                               */
/* -------------------------------------------------------------------- */
    for( papszTokens = CSVReadParseLineL( fp );
         CSLCount(papszTokens) > 2 && nReturn == KvUserDefined;
         papszTokens = CSVReadParseLineL( fp ) )
    {
        WKTMassageDatum( papszTokens + 1 );

        CPLAssert(papszTokens[1] != NULL);  // Silence clang static analyzer.
        if( EQUAL(papszTokens[1], pszOGCName) )
            nReturn = atoi(papszTokens[0]);

        CSLDestroy( papszTokens );
    }

    CSLDestroy( papszTokens );
    VSIFCloseL( fp );

    return nReturn;
}

/************************************************************************/
/*                        GTIFSetFromOGISDefn()                         */
/*                                                                      */
/*      Write GeoTIFF projection tags from an OGC WKT definition.       */
/************************************************************************/

int GTIFSetFromOGISDefn( GTIF * psGTIF, const char *pszOGCWKT )

{
    return GTIFSetFromOGISDefnEx(psGTIF, pszOGCWKT, GEOTIFF_KEYS_STANDARD);
}

int GTIFSetFromOGISDefnEx( GTIF * psGTIF, const char *pszOGCWKT,
                           GTIFFKeysFlavorEnum eFlavor )
{
    GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);

/* -------------------------------------------------------------------- */
/*      Create an OGRSpatialReference object corresponding to the       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = new OGRSpatialReference();
    if( poSRS->importFromWkt((char **) &pszOGCWKT) != OGRERR_NONE )
    {
        delete poSRS;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid definition.                                   */
/* -------------------------------------------------------------------- */
    short nSpheroid = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID") != NULL
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID"),
                 "EPSG"))
    {
        nSpheroid = static_cast<short>(
            atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS|DATUM|SPHEROID")) );
    }
    else if( poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID") != NULL
             && EQUAL(poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID"),"EPSG"))
    {
        nSpheroid = static_cast<short>(
            atoi(poSRS->GetAuthorityCode("GEOGCS|DATUM|SPHEROID")) );
    }

    OGRErr eErr = OGRERR_NONE;
    double dfSemiMajor = poSRS->GetSemiMajor( &eErr );
    double dfInvFlattening = poSRS->GetInvFlattening( &eErr );
    if( eErr != OGRERR_NONE )
    {
        dfSemiMajor = 0.0;
        dfInvFlattening = 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Get the Datum so we can special case a few PCS codes.           */
/* -------------------------------------------------------------------- */
    int nDatum = KvUserDefined;

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
    int nGCS = KvUserDefined;

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
    char *pszLinearUOMName = NULL;
    const double dfLinearUOM = poSRS->GetLinearUnits( &pszLinearUOMName );
    int nUOMLengthCode = 9001;  // Meters.

    if( poSRS->GetAuthorityName("PROJCS|UNIT") != NULL
        && EQUAL(poSRS->GetAuthorityName("PROJCS|UNIT"),"EPSG")
        && poSRS->GetAttrNode( "PROJCS|UNIT" ) !=
        poSRS->GetAttrNode("GEOGCS|UNIT") )
        nUOMLengthCode = atoi(poSRS->GetAuthorityCode("PROJCS|UNIT"));
    else if( (pszLinearUOMName != NULL
         && EQUAL(pszLinearUOMName,SRS_UL_FOOT))
        || fabs(dfLinearUOM - GTIFAtof(SRS_UL_FOOT_CONV)) < 0.0000001 )
        nUOMLengthCode = 9002;  // International foot.
    else if( (pszLinearUOMName != NULL
              && EQUAL(pszLinearUOMName,SRS_UL_US_FOOT)) ||
             std::abs(dfLinearUOM - GTIFAtof(SRS_UL_US_FOOT_CONV)) <
             0.0000001 )
        nUOMLengthCode = 9003;  // US survey foot.
    else if( fabs(dfLinearUOM - 1.0) > 0.00000001 )
        nUOMLengthCode = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get some authority values.                                      */
/* -------------------------------------------------------------------- */
    int nPCS = KvUserDefined;

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
    bool bWritePEString = false;

    if( nPCS != KvUserDefined )
    {
        // If ESRI_PE flavor is explicitly required, then for EPSG:3857
        // we will have to write a completely non-standard definition
        // that requires not setting GTModelTypeGeoKey to ProjectedCSTypeGeoKey.
        if( eFlavor == GEOTIFF_KEYS_ESRI_PE && nPCS == 3857 )
        {
            bWritePEString = true;
        }
        else
        {
            GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                    ModelTypeProjected);
            GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, nPCS );
        }
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
        // Otherwise, presumably something like LOCAL_CS.
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
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);

        int bNorth = 0;
        const int nZone = poSRS->GetUTMZone( &bNorth );

        int nProjection = 0;

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

        if( EQUAL(pszProjection,SRS_PT_MERCATOR_2SP) )
            GTIFKeySet(psGTIF, ProjStdParallel1GeoKey, TYPE_DOUBLE, 1,
                       poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0));
        else
            GTIFKeySet(psGTIF, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                       poSRS->GetNormProjParm( SRS_PP_SCALE_FACTOR, 1.0 ));

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

    else if( EQUAL(pszProjection,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
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
        bWritePEString = true;
    }

    // Note that VERTCS is an ESRI "spelling" of VERT_CS so we assume if
    // we find it that we should try to treat this as a PE string.
    bWritePEString |= (poSRS->GetAttrValue("VERTCS") != NULL);

    bWritePEString |= (eFlavor == GEOTIFF_KEYS_ESRI_PE);

    bWritePEString &=
        CPLTestBool( CPLGetConfigOption("GTIFF_ESRI_CITATION", "YES") );

    bool peStrStored = false;

    if( bWritePEString )
    {
        // Anyhing we can't map, store as an ESRI PE string with a citation key.
        char *pszPEString = NULL;
        poSRS->morphToESRI();
        poSRS->exportToWkt( &pszPEString );
        const int peStrLen = static_cast<int>(strlen(pszPEString));
        if(peStrLen > 0)
        {
            char *outPeStr = static_cast<char *>(
                CPLMalloc( peStrLen + strlen("ESRI PE String = ") + 1 ) );
            strcpy(outPeStr, "ESRI PE String = ");
            strcat(outPeStr, pszPEString);
            GTIFKeySet( psGTIF, PCSCitationGeoKey, TYPE_ASCII, 0, outPeStr );
            peStrStored = true;
            CPLFree( outPeStr );
        }
        CPLFree( pszPEString );
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

        // Not completely sure we really need to imitate ArcGIS to that point
        // but that cannot hurt.
        if( nPCS == 3857 )
        {
            GTIFKeySet( psGTIF, GTCitationGeoKey, TYPE_ASCII, 0,
                        "PCS Name = WGS_1984_Web_Mercator_Auxiliary_Sphere" );
            GTIFKeySet( psGTIF, GeographicTypeGeoKey, TYPE_SHORT,
                        1, GCS_WGS_84 );
            GTIFKeySet( psGTIF, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1,
                        6378137.0 );
            GTIFKeySet( psGTIF, GeogInvFlatteningGeoKey, TYPE_DOUBLE, 1,
                        298.257223563 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Is there a false easting/northing set?  If so, write out a      */
/*      special geokey tag to indicate that GDAL has written these      */
/*      with the proper interpretation of the linear units.             */
/* -------------------------------------------------------------------- */
    double dfFE = 0.0;
    double dfFN = 0.0;

    if( (GDALGTIFKeyGetDOUBLE(psGTIF, ProjFalseEastingGeoKey, &dfFE, 0, 1)
         || GDALGTIFKeyGetDOUBLE(psGTIF, ProjFalseNorthingGeoKey, &dfFN, 0, 1)
         || GDALGTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginEastingGeoKey, &dfFE,
                                 0, 1)
         || GDALGTIFKeyGetDOUBLE(psGTIF, ProjFalseOriginNorthingGeoKey, &dfFN,
                                 0, 1))
        && (dfFE != 0.0 || dfFN != 0.0)
        && nUOMLengthCode != 9001 )
    {
        GTIFKeySet(
            psGTIF, ProjLinearUnitsInterpCorrectGeoKey,
            TYPE_SHORT, 1, static_cast<short>(1));
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

        // If linear units name is available and user defined, store it as
        // citation.
        if( !peStrStored
            && nUOMLengthCode == KvUserDefined
            && pszLinearUOMName
            && strlen(pszLinearUOMName)>0
            && CPLTestBool( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                               "YES") ) )
        {
            SetLinearUnitCitation(psGTIF, pszLinearUOMName);
        }
    }

/* -------------------------------------------------------------------- */
/*      Write angular units.                                            */
/* -------------------------------------------------------------------- */

    char* angUnitName = NULL;
    double angUnitValue = poSRS->GetAngularUnits(&angUnitName);
    if(EQUAL(angUnitName, "Degree"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Degree );
    else if (EQUAL(angUnitName, "arc-second"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Arc_Second);
    else if (EQUAL(angUnitName, "arc-minute"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Arc_Minute);
    else if (EQUAL(angUnitName, "grad"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Grad);
    else if (EQUAL(angUnitName, "gon"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Gon);
    else if (EQUAL(angUnitName, "radian"))
        GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
                   Angular_Radian);
    // else if (EQUAL(angUnitName, "microradian"))
    //    GTIFKeySet(psGTIF, GeogAngularUnitsGeoKey, TYPE_SHORT, 1,
    //               9109);
    else
    {
        // GeogCitationGeoKey may be rewritten if the gcs is user defined.
        GTIFKeySet(psGTIF, GeogCitationGeoKey, TYPE_ASCII, 0,
                   angUnitName );
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
        if( !(bWritePEString && nPCS == 3857) )
        {
            GTIFKeySet( psGTIF, GTCitationGeoKey, TYPE_ASCII, 0,
                        poSRS->GetRoot()->GetChild(0)->GetValue() );
        }
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
                 && strstr(poSRS->GetAttrValue("DATUM"), "unknown") == NULL
                 && strstr(poSRS->GetAttrValue("DATUM"), "unnamed") == NULL )

        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Couldn't translate `%s' to a GeoTIFF datum.",
                      poSRS->GetAttrValue("DATUM") );
        }

        // Always set InvFlattening if it is available.
        // So that it doesn't need to calculate from SemiMinor.
        if( dfInvFlattening != 0.0 )
            GTIFKeySet( psGTIF, GeogInvFlatteningGeoKey, TYPE_DOUBLE, 1,
                        dfInvFlattening );
        // Always set SemiMajor to keep the precision and in case of editing.
        if( dfSemiMajor != 0.0 )
            GTIFKeySet( psGTIF, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1,
                        dfSemiMajor );

        if( nGCS == KvUserDefined
            && CPLTestBool( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                               "YES") ) )
            SetGeogCSCitation(psGTIF, poSRS, angUnitName, nDatum, nSpheroid);
    }

/* -------------------------------------------------------------------- */
/*      Do we have TOWGS84 parameters?                                  */
/* -------------------------------------------------------------------- */

#if LIBGEOTIFF_VERSION >= 1310 && !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    double adfTOWGS84[7] = { 0.0 };

    if( poSRS->GetTOWGS84( adfTOWGS84 ) == OGRERR_NONE )
    {
        if( adfTOWGS84[3] == 0.0 && adfTOWGS84[4] == 0.0
            && adfTOWGS84[5] == 0.0 && adfTOWGS84[6] == 0.0 )
        {
            if( nGCS == GCS_WGS_84 && adfTOWGS84[0] == 0.0
                && adfTOWGS84[1] == 0.0 && adfTOWGS84[2] == 0.0 )
            {
                ; // Do nothing.
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
        const char *pszValue = NULL;

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
    return GTIFWktFromMemBufEx( nSize, pabyBuffer, ppszWKT, padfGeoTransform,
                                pnGCPCount, ppasGCPList, NULL, NULL );
}

CPLErr GTIFWktFromMemBufEx( int nSize, unsigned char *pabyBuffer,
                            char **ppszWKT, double *padfGeoTransform,
                            int *pnGCPCount, GDAL_GCP **ppasGCPList,
                            int *pbPixelIsPoint, char*** ppapszRPCMD )

{
    char szFilename[100] = {};

    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/wkt_from_mem_buf_%ld.tif",
              static_cast<long>( CPLGetPID() ) );

/* -------------------------------------------------------------------- */
/*      Make sure we have hooked CSVFilename().                         */
/* -------------------------------------------------------------------- */
    GTiffOneTimeInit();  // For RPC tag.
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Create a memory file from the buffer.                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFileFromMemBuffer( szFilename, pabyBuffer, nSize, FALSE );
    if( fp == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "rc", fp );

    if( hTIFF == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        VSIUnlink( szFilename );
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the projection definition.                                  */
/* -------------------------------------------------------------------- */
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;
    short nRasterType = 0;

    GTIF *hGTIF = GTIFNew(hTIFF);

    if( hGTIF != NULL && GDALGTIFKeyGetSHORT(hGTIF, GTRasterTypeGeoKey,
                                             &nRasterType, 0, 1 ) == 1
        && nRasterType == static_cast<short>( RasterPixelIsPoint ) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
                                            "FALSE") );
    }
    if( pbPixelIsPoint )
        *pbPixelIsPoint = bPixelIsPoint;
    if( ppapszRPCMD )
        *ppapszRPCMD = NULL;

#if LIBGEOTIFF_VERSION >= 1410
    GTIFDefn *psGTIFDefn = GTIFAllocDefn();
#else
    GTIFDefn *psGTIFDefn = static_cast<GTIFDefn *>(
        CPLCalloc(1, sizeof(GTIFDefn)) );
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
    double *padfTiePoints = NULL;
    double *padfScale = NULL;
    double *padfMatrix = NULL;
    int16 nCount = 0;

    padfGeoTransform[0] = 0.0;
    padfGeoTransform[1] = 1.0;
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = 0.0;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = 1.0;

    *pnGCPCount = 0;
    *ppasGCPList = NULL;

    if( TIFFGetField(hTIFF, TIFFTAG_GEOPIXELSCALE, &nCount, &padfScale )
        && nCount >= 2 )
    {
        padfGeoTransform[1] = padfScale[0];
        padfGeoTransform[5] = -std::abs(padfScale[1]);

        if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
        {
            padfGeoTransform[0] =
                padfTiePoints[3] - padfTiePoints[0] * padfGeoTransform[1];
            padfGeoTransform[3] =
                padfTiePoints[4] - padfTiePoints[1] * padfGeoTransform[5];

            // Adjust for pixel is point in transform.
            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                padfGeoTransform[0] -=
                    padfGeoTransform[1] * 0.5 + padfGeoTransform[2] * 0.5;
                padfGeoTransform[3] -=
                    padfGeoTransform[4] * 0.5 + padfGeoTransform[5] * 0.5;
            }
        }
    }
    else if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
             && nCount >= 6 )
    {
        *pnGCPCount = nCount / 6;
        *ppasGCPList = static_cast<GDAL_GCP *>(
            CPLCalloc(sizeof(GDAL_GCP), *pnGCPCount) );

        for( int iGCP = 0; iGCP < *pnGCPCount; iGCP++ )
        {
            char szID[32] = {};
            GDAL_GCP *psGCP = *ppasGCPList + iGCP;

            snprintf( szID, sizeof(szID), "%d", iGCP + 1 );
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
/*      Read RPC                                                        */
/* -------------------------------------------------------------------- */
    if( ppapszRPCMD != NULL )
    {
        *ppapszRPCMD = GTiffDatasetReadRPCTag( hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    XTIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    VSIUnlink( szFilename );

    if( *ppszWKT == NULL )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                         GTIFMemBufFromWkt()                          */
/************************************************************************/

CPLErr GTIFMemBufFromWkt( const char *pszWKT, const double *padfGeoTransform,
                          int nGCPCount, const GDAL_GCP *pasGCPList,
                          int *pnSize, unsigned char **ppabyBuffer )
{
    return GTIFMemBufFromWktEx(pszWKT, padfGeoTransform,
                               nGCPCount,pasGCPList,
                               pnSize, ppabyBuffer, FALSE, NULL);
}

CPLErr GTIFMemBufFromWktEx( const char *pszWKT, const double *padfGeoTransform,
                            int nGCPCount, const GDAL_GCP *pasGCPList,
                            int *pnSize, unsigned char **ppabyBuffer,
                            int bPixelIsPoint, char** papszRPCMD )

{
    char szFilename[100] = {};

    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/wkt_from_mem_buf_%ld.tif",
              static_cast<long>( CPLGetPID() ) );

/* -------------------------------------------------------------------- */
/*      Make sure we have hooked CSVFilename().                         */
/* -------------------------------------------------------------------- */
    GTiffOneTimeInit();  // For RPC tag.
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    VSILFILE* fpL = VSIFOpenL( szFilename, "w" );
    if( fpL == NULL )
        return CE_Failure;

    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "w", fpL );

    if( hTIFF == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
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

    bool bPointGeoIgnore = false;
    if( bPixelIsPoint )
    {
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption( "GTIFF_POINT_GEO_IGNORE",
                                             "FALSE") );
    }

    GTIF *hGTIF = NULL;
    if( pszWKT != NULL || bPixelIsPoint )
    {
        hGTIF = GTIFNew(hTIFF);
        if( pszWKT != NULL )
            GTIFSetFromOGISDefn( hGTIF, pszWKT );

        if( bPixelIsPoint )
        {
            GTIFKeySet(hGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys( hGTIF );
        GTIFFree( hGTIF );
    }

/* -------------------------------------------------------------------- */
/*      Set the geotransform, or GCPs.                                  */
/* -------------------------------------------------------------------- */

    if( padfGeoTransform[0] != 0.0 || padfGeoTransform[1] != 1.0
        || padfGeoTransform[2] != 0.0 || padfGeoTransform[3] != 0.0
        || padfGeoTransform[4] != 0.0 || std::abs(padfGeoTransform[5]) != 1.0 )
    {

        if( padfGeoTransform[2] == 0.0 && padfGeoTransform[4] == 0.0 )
        {
            double adfPixelScale[3] = {
                padfGeoTransform[1],
                fabs(padfGeoTransform[5]),
                0.0
            };

            TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );

            double adfTiePoints[6] = {
                0.0, 0.0, 0.0, padfGeoTransform[0], padfGeoTransform[3], 0.0 };

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfTiePoints[3] +=
                    padfGeoTransform[1] * 0.5 + padfGeoTransform[2] * 0.5;
                adfTiePoints[4] +=
                    padfGeoTransform[4] * 0.5 + padfGeoTransform[5] * 0.5;
            }

            TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
        }
        else
        {
            double adfMatrix[16] = { 0.0 };

            adfMatrix[0] = padfGeoTransform[1];
            adfMatrix[1] = padfGeoTransform[2];
            adfMatrix[3] = padfGeoTransform[0];
            adfMatrix[4] = padfGeoTransform[4];
            adfMatrix[5] = padfGeoTransform[5];
            adfMatrix[7] = padfGeoTransform[3];
            adfMatrix[15] = 1.0;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfMatrix[3] +=
                    padfGeoTransform[1] * 0.5 + padfGeoTransform[2] * 0.5;
                adfMatrix[7] +=
                    padfGeoTransform[4] * 0.5 + padfGeoTransform[5] * 0.5;
            }

            TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( nGCPCount > 0 )
    {
        double *padfTiePoints = static_cast<double *>(
            CPLMalloc(6*sizeof(double)*nGCPCount) );

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
/*      Write RPC                                                       */
/* -------------------------------------------------------------------- */
    if( papszRPCMD != NULL )
    {
        GTiffDatasetWriteRPCTag( hTIFF, papszRPCMD );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return the created memory buffer.                   */
/* -------------------------------------------------------------------- */
    GByte bySmallImage = 0;

    TIFFWriteEncodedStrip( hTIFF, 0, (char *) &bySmallImage, 1 );
    TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTIFMemBufFromWkt");
    TIFFWriteDirectory( hTIFF );

    XTIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));

/* -------------------------------------------------------------------- */
/*      Read back from the memory buffer.  It would be preferable       */
/*      to be able to "steal" the memory buffer, but there isn't        */
/*      currently any support for this.                                 */
/* -------------------------------------------------------------------- */
    GUIntBig nBigLength = 0;

    *ppabyBuffer = VSIGetMemFileBuffer( szFilename, &nBigLength, TRUE );
    *pnSize = static_cast<int>( nBigLength );

    return CE_None;
}
