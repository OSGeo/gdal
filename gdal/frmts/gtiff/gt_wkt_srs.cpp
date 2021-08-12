/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Implements translation between GeoTIFF normalized projection
 *           definitions and OpenGIS WKT SRS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include <mutex>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
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
#include "ogr_proj_p.h"
#include "tiff.h"
#include "tiffio.h"
#include "tifvsi.h"
#include "xtiffio.h"

#include "proj.h"

CPL_CVSID("$Id$")

static const geokey_t ProjLinearUnitsInterpCorrectGeoKey =
    static_cast<geokey_t>(3059);

#ifndef CT_HotineObliqueMercatorAzimuthCenter
#  define CT_HotineObliqueMercatorAzimuthCenter 9815
#endif

#if !defined(GTIFAtof)
#  define GTIFAtof CPLAtof
#endif

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
  nullptr
};

// Older libgeotiff's won't list this.
#ifndef CT_CylindricalEqualArea
# define CT_CylindricalEqualArea 28
#endif

#if LIBGEOTIFF_VERSION < 1700
constexpr geokey_t CoordinateEpochGeoKey = static_cast<geokey_t>(5120);
#endif

/************************************************************************/
/*                       LibgeotiffOneTimeInit()                        */
/************************************************************************/

static std::mutex oDeleteMutex;

void LibgeotiffOneTimeInit()
{
    std::lock_guard<std::mutex> oLock(oDeleteMutex);

    static bool bOneTimeInitDone = false;

    if( bOneTimeInitDone )
        return;

    bOneTimeInitDone = true;

    // This isn't thread-safe, so better do it now
    XTIFFInitialize();
}

/************************************************************************/
/*                       GTIFToCPLRecyleString()                        */
/*                                                                      */
/*      This changes a string from the libgeotiff heap to the GDAL      */
/*      heap.                                                           */
/************************************************************************/

static void GTIFToCPLRecycleString( char **ppszTarget )

{
    if( *ppszTarget == nullptr )
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
    for( int i = 0; papszDatumEquiv[i] != nullptr; i += 2 )
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
    if( strstr(pszCitation,"IMAGINE GeoTIFF") == nullptr )
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

#if LIBGEOTIFF_VERSION < 1600

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
    if( !GTIFKeyInfo(hGTIF, key, nullptr, &tagtype) )
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
                         int szStrMaxLen )
{
    return GDALGTIFKeyGet( hGTIF, key, szStr, 0, szStrMaxLen, TYPE_ASCII );
}

/************************************************************************/
/*                       GDALGTIFKeyGetSHORT()                          */
/************************************************************************/

int GDALGTIFKeyGetSHORT( GTIF *hGTIF, geokey_t key,
                         unsigned short* pnVal,
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

#endif

/************************************************************************/
/*                      GTIFGetOGISDefnAsOSR()                          */
/************************************************************************/

OGRSpatialReferenceH GTIFGetOGISDefnAsOSR( GTIF *hGTIF, GTIFDefn * psDefn )

{
    OGRSpatialReference oSRS;

    LibgeotiffOneTimeInit();

#if LIBGEOTIFF_VERSION >= 1600
    void* projContext = GTIFGetPROJContext(hGTIF, FALSE, nullptr);
#endif

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
        char szPeStr[2400] = { '\0' };

        /** check if there is a pe string citation key **/
        if( GDALGTIFKeyGetASCII( hGTIF, PCSCitationGeoKey, szPeStr,
                                 sizeof(szPeStr) ) &&
            strstr(szPeStr, "ESRI PE String = " ) )
        {
            const char* pszWKT = szPeStr + strlen("ESRI PE String = ");
            oSRS.importFromWkt(pszWKT);

            if( strstr( pszWKT,
                        "PROJCS[\"WGS_1984_Web_Mercator_Auxiliary_Sphere\"" ) )
            {
                oSRS.SetExtension(
                    "PROJCS", "PROJ4",
                    "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 "
                    "+x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null "
                    "+wktext  +no_defs" );  // TODO(schwehr): Why 2 spaces?
            }

            return OGRSpatialReference::ToHandle(oSRS.Clone());
        }
        else
        {
            char *pszUnitsName = nullptr;
            char szPCSName[300] = { '\0' };
            int nKeyCount = 0;
            int anVersion[3] = { 0 };

            GTIFDirectoryInfo( hGTIF, anVersion, &nKeyCount );

            if( nKeyCount > 0 ) // Use LOCAL_CS if we have any geokeys at all.
            {
                // Handle citation.
                strcpy( szPCSName, "unnamed" );
                if( !GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szPCSName,
                                          sizeof(szPCSName) ) )
                    GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szPCSName,
                                         sizeof(szPCSName) );

                GTIFCleanupImagineNames( szPCSName );
                oSRS.SetLocalCS( szPCSName );

                // Handle units
                if( psDefn->UOMLength != KvUserDefined )
                {
#if LIBGEOTIFF_VERSION >= 1600
                    GTIFGetUOMLengthInfoEx( projContext,
#else
                    GTIFGetUOMLengthInfo(
#endif
                        psDefn->UOMLength, &pszUnitsName, nullptr );
                }

                if( pszUnitsName != nullptr )
                {
                    char szUOMLength[12];
                    snprintf(szUOMLength, sizeof(szUOMLength),
                             "%d", psDefn->UOMLength );
                    oSRS.SetTargetLinearUnits(
                        nullptr, pszUnitsName, psDefn->UOMLengthInMeters,
                        "EPSG", szUOMLength);
                }
                else
                    oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

                GTIFFreeMemory( pszUnitsName );
            }
            return OGRSpatialReference::ToHandle(oSRS.Clone());
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
                                  sizeof(szName) ) )
            GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szName,
                                 sizeof(szName) );

        oSRS.SetGeocCS( szName );

        char *pszUnitsName = nullptr;

        if( psDefn->UOMLength != KvUserDefined )
        {
#if LIBGEOTIFF_VERSION >= 1600
            GTIFGetUOMLengthInfoEx( projContext,
#else
            GTIFGetUOMLengthInfo(
#endif
                psDefn->UOMLength, &pszUnitsName, nullptr );
        }

        if( pszUnitsName != nullptr )
        {
            char szUOMLength[12];
                    snprintf(szUOMLength, sizeof(szUOMLength),
                             "%d", psDefn->UOMLength );
            oSRS.SetTargetLinearUnits( nullptr,
                pszUnitsName, psDefn->UOMLengthInMeters, "EPSG", szUOMLength);
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

/* -------------------------------------------------------------------- */
/*      #3901: If folks have broken GeoTIFF files generated with        */
/*      older versions of GDAL+libgeotiff, then they may need a         */
/*      hack to allow them to be read properly.  This is that           */
/*      hack.  We basically try to undue the conversion applied by      */
/*      libgeotiff to meters (or above) to simulate the old             */
/*      behavior.                                                       */
/* -------------------------------------------------------------------- */
    unsigned short bLinearUnitsMarkedCorrect = FALSE;

    GDALGTIFKeyGetSHORT(hGTIF, ProjLinearUnitsInterpCorrectGeoKey,
               &bLinearUnitsMarkedCorrect, 0, 1);

    if( EQUAL(pszLinearUnits,"BROKEN")
        && psDefn->Projection == KvUserDefined
        && !bLinearUnitsMarkedCorrect )
    {
        for( int iParam = 0; iParam < psDefn->nParms; iParam++ )
        {
            switch( psDefn->ProjParmId[iParam] )
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
                    psDefn->ProjParm[iParam] /= psDefn->UOMLengthInMeters;
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
            char *pszPCSName = nullptr;

#if LIBGEOTIFF_VERSION >= 1600
            GTIFGetPCSInfoEx( projContext,
#else
            GTIFGetPCSInfo(
#endif
                psDefn->PCS, &pszPCSName, nullptr, nullptr, nullptr );

            oSRS.SetProjCS( pszPCSName ? pszPCSName : "unnamed" );
            if ( pszPCSName )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetLinearUnits("unknown", 1.0);
        }
        else
        {
            bool bTryGTCitationGeoKey = true;
            if( GDALGTIFKeyGetASCII( hGTIF, PCSCitationGeoKey,
                                              szCTString,
                                              sizeof(szCTString)) )
            {
                bTryGTCitationGeoKey = false;
                if (!SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                                       PCSCitationGeoKey, &oSRS,
                                       &linearUnitIsSet) )
                {
                    if( !STARTS_WITH_CI(szCTString, "LUnits = ") )
                    {
                        oSRS.SetProjCS( szCTString );
                        oSRS.SetLinearUnits("unknown", 1.0);
                    }
                    else
                    {
                        bTryGTCitationGeoKey = true;
                    }
                }
            }

            if( bTryGTCitationGeoKey )
            {
                if( GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                                         sizeof(szCTString) ) &&
                    !SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                                       GTCitationGeoKey, &oSRS,
                                       &linearUnitIsSet ) )
                {
                    oSRS.SetNode( "PROJCS", szCTString );
                    oSRS.SetLinearUnits("unknown", 1.0);
                }
                else
                {
                    oSRS.SetNode( "PROJCS", "unnamed" );
                    oSRS.SetLinearUnits("unknown", 1.0);
                }
            }
        }

        /* Handle ESRI/Erdas style state plane and UTM in citation key */
        if( CheckCitationKeyForStatePlaneUTM( hGTIF, psDefn, &oSRS,
                                              &linearUnitIsSet ) )
        {
            return OGRSpatialReference::ToHandle(oSRS.Clone());
        }

        /* Handle ESRI PE string in citation */
        szCTString[0] = '\0';
        if( GDALGTIFKeyGetASCII( hGTIF, GTCitationGeoKey, szCTString,
                                 sizeof(szCTString) ) )
            SetCitationToSRS( hGTIF, szCTString, sizeof(szCTString),
                              GTCitationGeoKey, &oSRS, &linearUnitIsSet );
    }

/* ==================================================================== */
/*      Read keys related to vertical component.                        */
/* ==================================================================== */
    unsigned short verticalCSType = 0;
    unsigned short verticalDatum =  0;
    unsigned short verticalUnits =  0;

    GDALGTIFKeyGetSHORT( hGTIF, VerticalCSTypeGeoKey, &verticalCSType, 0, 1 );
    GDALGTIFKeyGetSHORT( hGTIF, VerticalDatumGeoKey, &verticalDatum, 0, 1 );
    GDALGTIFKeyGetSHORT( hGTIF, VerticalUnitsGeoKey, &verticalUnits, 0, 1 );

    if( verticalCSType != 0 || verticalDatum != 0 || verticalUnits != 0 )
    {
        int versions[3];
        GTIFDirectoryInfo(hGTIF, versions, nullptr);
        // GeoTIFF 1.0
        if( versions[0] == 1 && versions[1]== 1 && versions[2] == 0 )
        {
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
                && verticalDatum == 0 )
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
        }

/* -------------------------------------------------------------------- */
/*      Somewhat similarly, codes 5001 to 5033 were treated as          */
/*      vertical coordinate systems based on ellipsoidal heights.       */
/*      We use the corresponding geodetic datum as the vertical         */
/*      datum and clear the vertical coordinate system code since       */
/*      there isn't one in EPSG.                                        */
/* -------------------------------------------------------------------- */
        if( (verticalCSType >= 5001 && verticalCSType <= 5033)
            && verticalDatum == 0 )
        {
            verticalDatum = verticalCSType + 1000;
            verticalCSType = 0;
        }
    }

/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char *pszGeogName = nullptr;
    char *pszDatumName = nullptr;
    char *pszPMName = nullptr;
    char *pszSpheroidName = nullptr;
    char *pszAngularUnits = nullptr;
    char szGCSName[512] = { '\0' };

    if( !
#if LIBGEOTIFF_VERSION >= 1600
        GTIFGetGCSInfoEx( projContext,
#else
        GTIFGetGCSInfo(
#endif
            psDefn->GCS, &pszGeogName, nullptr, nullptr, nullptr )
        && GDALGTIFKeyGetASCII( hGTIF, GeogCitationGeoKey, szGCSName,
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
#if LIBGEOTIFF_VERSION >= 1600
        GTIFGetDatumInfoEx( projContext,
#else
        GTIFGetDatumInfo(
#endif
            psDefn->Datum, &pszDatumName, nullptr );
        GTIFToCPLRecycleString( &pszDatumName );
    }

    double dfSemiMajor = 0.0;
    double dfInvFlattening = 0.0;
    if( !pszSpheroidName )
    {
#if LIBGEOTIFF_VERSION >= 1600
        GTIFGetEllipsoidInfoEx( projContext,
#else
        GTIFGetEllipsoidInfo(
#endif
            psDefn->Ellipsoid, &pszSpheroidName, nullptr, nullptr );
        GTIFToCPLRecycleString( &pszSpheroidName );
    }
    else
    {
        CPL_IGNORE_RET_VAL(
            GDALGTIFKeyGetDOUBLE( hGTIF, GeogSemiMajorAxisGeoKey,
                              &(psDefn->SemiMajor), 0, 1 ));
        CPL_IGNORE_RET_VAL(
            GDALGTIFKeyGetDOUBLE( hGTIF, GeogInvFlatteningGeoKey,
                              &dfInvFlattening, 0, 1 ));
        if( std::isinf(dfInvFlattening) )
        {
            // Deal with the non-nominal case of
            // https://github.com/OSGeo/PROJ/issues/2317
            dfInvFlattening = 0;
        }
    }
    if( !pszPMName )
    {
#if LIBGEOTIFF_VERSION >= 1600
        GTIFGetPMInfoEx( projContext,
#else
        GTIFGetPMInfo(
#endif
            psDefn->PM, &pszPMName, nullptr );
        GTIFToCPLRecycleString( &pszPMName );
    }
    else
    {
        CPL_IGNORE_RET_VAL(
            GDALGTIFKeyGetDOUBLE( hGTIF, GeogPrimeMeridianLongGeoKey,
                              &(psDefn->PMLongToGreenwich), 0, 1 ));
    }

    if( !pszAngularUnits )
    {
#if LIBGEOTIFF_VERSION >= 1600
        GTIFGetUOMAngleInfoEx( projContext,
#else
        GTIFGetUOMAngleInfo(
#endif
            psDefn->UOMAngle, &pszAngularUnits, &psDefn->UOMAngleInDegrees );
        if( pszAngularUnits == nullptr )
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
            psDefn->UOMAngleInDegrees = dfRadians / CPLAtof(SRS_UA_DEGREE_CONV);
        }
    }

    if( pszDatumName != nullptr )
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

    bool bGeog3DCRS = false;
    bool bSetDatumEllipsoid = true;
    {
        int nGCS = psDefn->GCS;
        if( nGCS != KvUserDefined && nGCS > 0 )
        {
            oSRS.SetAuthority( "GEOGCS", "EPSG", nGCS );

            int nVertSRSCode = verticalCSType;
            if( verticalDatum == 6030 && nGCS == 4326 ) // DatumE_WGS84
            {
                nVertSRSCode = 4979;
            }

            // Try to reconstruct a Geographic3D CRS from the
            // GeodeticCRSGeoKey and the VerticalGeoKey, when they are consistent
            if( nVertSRSCode > 0 && nVertSRSCode != KvUserDefined )
            {
                OGRSpatialReference oTmpSRS;
                OGRSpatialReference oTmpVertSRS;
                if( oTmpSRS.importFromEPSG(nGCS) == OGRERR_NONE &&
                    oTmpSRS.IsGeographic() && oTmpSRS.GetAxesCount() == 2 &&
                    oTmpVertSRS.importFromEPSG(nVertSRSCode) == OGRERR_NONE &&
                    oTmpVertSRS.IsGeographic() && oTmpVertSRS.GetAxesCount() == 3 )
                {
                    const char* pszTmpCode = oTmpSRS.GetAuthorityCode( "GEOGCS|DATUM" );
                    const char* pszTmpVertCode = oTmpVertSRS.GetAuthorityCode( "GEOGCS|DATUM" );
                    if( pszTmpCode && pszTmpVertCode &&
                        atoi(pszTmpCode) == atoi(pszTmpVertCode) )
                    {
                        verticalCSType = 0;
                        verticalDatum = 0;
                        verticalUnits = 0;
                        oSRS.CopyGeogCSFrom(&oTmpVertSRS);
                        bSetDatumEllipsoid = false;
                        bGeog3DCRS = true;
                    }
                }
            }
        }
    }
    if( bSetDatumEllipsoid )
    {
        if( psDefn->Datum != KvUserDefined )
            oSRS.SetAuthority( "DATUM", "EPSG", psDefn->Datum );

        if( psDefn->Ellipsoid != KvUserDefined )
            oSRS.SetAuthority( "SPHEROID", "EPSG", psDefn->Ellipsoid );
    }

    CPLFree( pszGeogName );
    CPLFree( pszDatumName );
    CPLFree( pszSpheroidName );
    CPLFree( pszPMName );
    CPLFree( pszAngularUnits );

/* -------------------------------------------------------------------- */
/*      Set projection units if not yet done                            */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected && !linearUnitIsSet )
    {
        char *pszUnitsName = nullptr;

        if( psDefn->UOMLength != KvUserDefined )
        {
#if LIBGEOTIFF_VERSION >= 1600
            GTIFGetUOMLengthInfoEx( projContext,
#else
            GTIFGetUOMLengthInfo(
#endif
                psDefn->UOMLength, &pszUnitsName, nullptr );
        }

        if( pszUnitsName != nullptr )
        {
            char szUOMLength[12];
            snprintf(szUOMLength, sizeof(szUOMLength),
                        "%d", psDefn->UOMLength );
            oSRS.SetTargetLinearUnits( nullptr,
                pszUnitsName, psDefn->UOMLengthInMeters, "EPSG", szUOMLength);
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
    unsigned short tmp = 0;
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
            const char* pszUnitsName = nullptr;
            double dfUOMLengthInMeters = oSRS.GetLinearUnits( &pszUnitsName );
            // Non exact comparison, as there's a slight difference between
            // the evaluation of US Survey foot hardcoded in geo_normalize.c to
            // 12.0 / 39.37, and the corresponding value returned by
            // PROJ >= 6.0.0 and <= 7.0.0 for EPSG:9003
            if( fabs(dfUOMLengthInMeters - oSRSTmp.GetLinearUnits(nullptr)) >
                    1e-15 * dfUOMLengthInMeters )
            {
                CPLDebug( "GTiff", "Modify EPSG:%d to have %s linear units...",
                          psDefn->PCS,
                          pszUnitsName ? pszUnitsName : "unknown" );

                const char* pszUnitAuthorityCode =
                    oSRS.GetAuthorityCode( "PROJCS|UNIT" );
                const char* pszUnitAuthorityName =
                    oSRS.GetAuthorityName( "PROJCS|UNIT" );

                if( pszUnitsName )
                    oSRSTmp.SetLinearUnitsAndUpdateParameters(
                        pszUnitsName, dfUOMLengthInMeters,
                        pszUnitAuthorityCode, pszUnitAuthorityName);
            }

            if( bGeog3DCRS )
            {
                oSRSTmp.CopyGeogCSFrom(&oSRS);
                oSRSTmp.UpdateCoordinateSystemFromGeogCRS();
            }
            oSRS = oSRSTmp;
        }
    }

#if !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    if( psDefn->TOWGS84Count > 0 &&
        bGotFromEPSG  &&
        CPLTestBool(CPLGetConfigOption("OSR_STRIP_TOWGS84", "YES")) )
    {
        CPLDebug("OSR", "TOWGS84 information has been removed. "
                 "It can be kept by setting the OSR_STRIP_TOWGS84 "
                 "configuration option to NO");
    }
    else if( psDefn->TOWGS84Count > 0 &&
        (!bGotFromEPSG ||
         !CPLTestBool(CPLGetConfigOption("OSR_STRIP_TOWGS84", "YES"))) )
    {
        if( bGotFromEPSG )
        {
            double adfTOWGS84[7] = { 0.0 };
            oSRS.GetTOWGS84( adfTOWGS84 );
            bool bSame = true;
            for( int i = 0; i < 7; i++ )
            {
                if( fabs(adfTOWGS84[i] - psDefn->TOWGS84[i]) > 1e-5 )
                {
                    bSame = false;
                    break;
                }
            }
            if( !bSame )
            {
                CPLDebug( "GTiff",
                          "Modify EPSG:%d to have "
                          "TOWGS84=%f,%f,%f,%f,%f,%f,%f "
                          "coming from GeogTOWGS84GeoKey, instead of "
                          "%f,%f,%f,%f,%f,%f,%f coming from EPSG",
                          psDefn->PCS,
                          psDefn->TOWGS84[0],
                          psDefn->TOWGS84[1],
                          psDefn->TOWGS84[2],
                          psDefn->TOWGS84[3],
                          psDefn->TOWGS84[4],
                          psDefn->TOWGS84[5],
                          psDefn->TOWGS84[6],
                          adfTOWGS84[0],
                          adfTOWGS84[1],
                          adfTOWGS84[2],
                          adfTOWGS84[3],
                          adfTOWGS84[4],
                          adfTOWGS84[5],
                          adfTOWGS84[6] );
            }
        }

        oSRS.SetTOWGS84( psDefn->TOWGS84[0],
                         psDefn->TOWGS84[1],
                         psDefn->TOWGS84[2],
                         psDefn->TOWGS84[3],
                         psDefn->TOWGS84[4],
                         psDefn->TOWGS84[5],
                         psDefn->TOWGS84[6] );
    }
#endif

/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected && !bGotFromEPSG )
    {
/* -------------------------------------------------------------------- */
/*      Make a local copy of params, and convert back into the           */
/*      angular units of the GEOGCS and the linear units of the         */
/*      projection.                                                     */
/* -------------------------------------------------------------------- */
        double adfParam[10] = { 0.0 };
        int i = 0;  // Used after for.

        for( ; i < std::min(10, psDefn->nParms); i++ )
            adfParam[i] = psDefn->ProjParm[i];

        for( ; i < 10; i++ )
            adfParam[i] = 0.0;

/* -------------------------------------------------------------------- */
/*      Translation the fundamental projection.                         */
/* -------------------------------------------------------------------- */
        switch( psDefn->CTProjection )
        {
          case CT_TransverseMercator:
            oSRS.SetTM( adfParam[0], adfParam[1],
                        adfParam[4],
                        adfParam[5], adfParam[6] );
            break;

          case CT_TransvMercator_SouthOriented:
            oSRS.SetTMSO( adfParam[0], adfParam[1],
                          adfParam[4],
                          adfParam[5], adfParam[6] );
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
                oSRS.SetMercator2SP( adfParam[2],
                                     adfParam[0], adfParam[1],
                                     adfParam[5], adfParam[6]);
            }
            else
                oSRS.SetMercator( adfParam[0], adfParam[1],
                                  adfParam[4],
                                  adfParam[5], adfParam[6] );

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
            oSRS.SetOS( adfParam[0], adfParam[1],
                        adfParam[4],
                        adfParam[5], adfParam[6] );
            break;

          case CT_Stereographic:
            oSRS.SetStereographic( adfParam[0], adfParam[1],
                        adfParam[4],
                        adfParam[5], adfParam[6] );
            break;

          case CT_ObliqueMercator:  // Hotine.
            oSRS.SetHOM( adfParam[0], adfParam[1],
                         adfParam[2], adfParam[3],
                         adfParam[4],
                         adfParam[5], adfParam[6] );
            break;

          case CT_HotineObliqueMercatorAzimuthCenter:
            oSRS.SetHOMAC( adfParam[0], adfParam[1],
                           adfParam[2], adfParam[3],
                           adfParam[4],
                           adfParam[5], adfParam[6] );
            break;

          case CT_ObliqueMercator_Laborde:
            oSRS.SetLOM( adfParam[0], adfParam[1],
                         adfParam[2],
                         adfParam[4],
                         adfParam[5], adfParam[6] );
            break;

          case CT_EquidistantConic:
            oSRS.SetEC( adfParam[0], adfParam[1],
                        adfParam[2], adfParam[3],
                        adfParam[5], adfParam[6] );
            break;

          case CT_CassiniSoldner:
            oSRS.SetCS( adfParam[0], adfParam[1],
                        adfParam[5], adfParam[6] );
            break;

          case CT_Polyconic:
            oSRS.SetPolyconic( adfParam[0], adfParam[1],
                               adfParam[5], adfParam[6] );
            break;

          case CT_AzimuthalEquidistant:
            oSRS.SetAE( adfParam[0], adfParam[1],
                        adfParam[5], adfParam[6] );
            break;

          case CT_MillerCylindrical:
            oSRS.SetMC( adfParam[0], adfParam[1],
                        adfParam[5], adfParam[6] );
            break;

          case CT_Equirectangular:
            oSRS.SetEquirectangular2( adfParam[0], adfParam[1],
                                      adfParam[2],
                                      adfParam[5], adfParam[6] );
            break;

          case CT_Gnomonic:
            oSRS.SetGnomonic( adfParam[0], adfParam[1],
                              adfParam[5], adfParam[6] );
            break;

          case CT_LambertAzimEqualArea:
            oSRS.SetLAEA( adfParam[0], adfParam[1],
                          adfParam[5], adfParam[6] );
            break;

          case CT_Orthographic:
            oSRS.SetOrthographic( adfParam[0], adfParam[1],
                                  adfParam[5], adfParam[6] );
            break;

          case CT_Robinson:
            oSRS.SetRobinson( adfParam[1],
                              adfParam[5], adfParam[6] );
            break;

          case CT_Sinusoidal:
            oSRS.SetSinusoidal( adfParam[1],
                                adfParam[5], adfParam[6] );
            break;

          case CT_VanDerGrinten:
            oSRS.SetVDG( adfParam[1],
                         adfParam[5], adfParam[6] );
            break;

          case CT_PolarStereographic:
            oSRS.SetPS( adfParam[0], adfParam[1],
                        adfParam[4],
                        adfParam[5], adfParam[6] );
            break;

          case CT_LambertConfConic_2SP:
            oSRS.SetLCC( adfParam[2], adfParam[3],
                         adfParam[0], adfParam[1],
                         adfParam[5], adfParam[6] );
            break;

          case CT_LambertConfConic_1SP:
            oSRS.SetLCC1SP( adfParam[0], adfParam[1],
                            adfParam[4],
                            adfParam[5], adfParam[6] );
            break;

          case CT_AlbersEqualArea:
            oSRS.SetACEA( adfParam[0], adfParam[1],
                          adfParam[2], adfParam[3],
                          adfParam[5], adfParam[6] );
            break;

          case CT_NewZealandMapGrid:
            oSRS.SetNZMG( adfParam[0], adfParam[1],
                          adfParam[5], adfParam[6] );
            break;

          case CT_CylindricalEqualArea:
            oSRS.SetCEA( adfParam[0], adfParam[1],
                         adfParam[5], adfParam[6] );
            break;
          default:
            if( oSRS.IsProjected() )
            {
                const char* pszName = oSRS.GetName();
                std::string osName( pszName ? pszName : "unnamed" );
                oSRS.Clear();
                oSRS.SetLocalCS( osName.c_str() );
            }
            break;
        }
    }

    if( psDefn->Model == ModelTypeProjected && psDefn->PCS != KvUserDefined &&
        !bGotFromEPSG )
    {
        oSRS.SetAuthority( nullptr, "EPSG", psDefn->PCS );
    }

    if( oSRS.IsProjected() && oSRS.GetAxesCount() == 2 )
    {
        const char* pszProjCRSName = oSRS.GetAttrValue("PROJCS");
        if( pszProjCRSName )
        {
            // Hack to be able to read properly what we have written for
            // ESRI:102113 (ESRI ancient WebMercator).
            if( EQUAL(pszProjCRSName, "WGS_1984_Web_Mercator") )
                oSRS.SetFromUserInput("ESRI:102113");
            // And for EPSG:900913
            else if( EQUAL( pszProjCRSName,
                            "Google Maps Global Mercator" ) )
                oSRS.importFromEPSG(900913);
        }
    }

/* ==================================================================== */
/*      Handle vertical coordinate system information if we have it.    */
/* ==================================================================== */
    bool bNeedManualVertCS = false;
    char citation[2048] = { '\0' };

    // See https://github.com/OSGeo/gdal/pull/4197
    if( verticalCSType > KvUserDefined ||
        verticalDatum > KvUserDefined ||
        verticalUnits > KvUserDefined )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "At least one of VerticalCSTypeGeoKey, VerticalDatumGeoKey or "
                 "VerticalUnitsGeoKey has a value in the private user range. "
                 "Ignoring vertical information.");
        verticalCSType = 0;
        verticalDatum = 0;
        verticalUnits = 0;
    }

    if( (verticalCSType != 0 || verticalDatum != 0 || verticalUnits != 0)
        && (oSRS.IsGeographic() || oSRS.IsProjected() || oSRS.IsLocal()) )
    {
        if( GDALGTIFKeyGetASCII( hGTIF, VerticalCitationGeoKey, citation,
                                  sizeof(citation) ) )
        {
            if( STARTS_WITH_CI(citation, "VCS Name = ") )
            {
                memmove(citation, citation + strlen("VCS Name = "),
                        strlen(citation + strlen("VCS Name = ")) + 1);
                char* pszPipeChar = strchr(citation, '|');
                if( pszPipeChar )
                    *pszPipeChar = '\0';
            }
        }
        else
        {
            strcpy( citation, "unknown" );
        }

        OGRSpatialReference oVertSRS;
        bool bCanBuildCompoundCRS = true;
        if( verticalCSType != KvUserDefined && verticalCSType > 0 )
        {
            if( !(oVertSRS.importFromEPSG( verticalCSType ) == OGRERR_NONE &&
                  oVertSRS.IsVertical() ) )
            {
                bCanBuildCompoundCRS = false;
            }
        }

        if( bCanBuildCompoundCRS )
        {
            const std::string osHorizontalName = oSRS.GetName();
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
                                     sizeof(szCTString) ) &&
                strstr( szCTString, " = " ) == nullptr )
            {
                oSRS.SetNode( "COMPD_CS", szCTString );
            }
            else
            {
                oSRS.SetNode( "COMPD_CS", (osHorizontalName + " + " + citation).c_str() );
            }

            oSRS.GetRoot()->AddChild( poOldRoot );

/* -------------------------------------------------------------------- */
/*      If we have the vertical cs, try to look it up, and use the      */
/*      definition provided by that.                                    */
/* -------------------------------------------------------------------- */
            bNeedManualVertCS = true;

            if( !oVertSRS.IsEmpty() )
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
/* -------------------------------------------------------------------- */
/*      Setup VERT_CS with citation if present.                         */
/* -------------------------------------------------------------------- */
        oSRS.SetNode( "COMPD_CS|VERT_CS", citation );

/* -------------------------------------------------------------------- */
/*      Setup the vertical datum.                                       */
/* -------------------------------------------------------------------- */
        std::string osVDatumName = "unknown";
        const char *pszVDatumType = "2005"; // CS_VD_GeoidModelDerived

        if( verticalDatum > 0 && verticalDatum != KvUserDefined )
        {
            char szCode[12];
            snprintf(szCode, sizeof(szCode), "%d", verticalDatum);
            auto ctx = static_cast<PJ_CONTEXT*>(
                GTIFGetPROJContext(hGTIF, true, nullptr));
            auto datum = proj_create_from_database(
                ctx, "EPSG", szCode, PJ_CATEGORY_DATUM, 0, nullptr);
            if( datum )
            {
                const char* pszName = proj_get_name(datum);
                if( pszName )
                {
                    osVDatumName = pszName;
                }
                proj_destroy(datum);
            }
        }

        oSRS.SetNode( "COMPD_CS|VERT_CS|VERT_DATUM", osVDatumName.c_str() );
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
            char szCode[12];
            snprintf(szCode, sizeof(szCode), "%d", verticalUnits);
            auto ctx = static_cast<PJ_CONTEXT*>(
                GTIFGetPROJContext(hGTIF, true, nullptr));
            const char* pszName = nullptr;
            double dfInMeters = 0.0;
            if( proj_uom_get_info_from_database(
                ctx, "EPSG", szCode, &pszName, &dfInMeters, nullptr) )
            {
                if( pszName )
                    oSRS.SetNode( "COMPD_CS|VERT_CS|UNIT", pszName );

                char szInMeters[128] = {};
                CPLsnprintf( szInMeters, sizeof(szInMeters),
                             "%.16g", dfInMeters );
                oSRS.GetAttrNode( "COMPD_CS|VERT_CS|UNIT" )
                    ->AddChild( new OGR_SRSNode( szInMeters ) );
            }

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
    }

    // Hack for tiff_read.py:test_tiff_grads so as to normalize angular
    // parameters to grad
    if( psDefn->UOMAngleInDegrees != 1.0 )
    {
        char *pszWKT = nullptr;
        const char* const apszOptions[] = {
            "FORMAT=WKT1", "ADD_TOWGS84_ON_EXPORT_TO_WKT1=NO", nullptr };
        if( oSRS.exportToWkt( &pszWKT, apszOptions ) == OGRERR_NONE )
        {
            oSRS.importFromWkt(pszWKT);
        }
        CPLFree(pszWKT);
    }

    oSRS.StripTOWGS84IfKnownDatumAndAllowed();

    double dfCoordinateEpoch = 0.0;
    if( GDALGTIFKeyGetDOUBLE(hGTIF, CoordinateEpochGeoKey, &dfCoordinateEpoch,
                             0, 1) )
    {
        oSRS.SetCoordinateEpoch(dfCoordinateEpoch);
    }

    return OGRSpatialReference::ToHandle(oSRS.Clone());
}


/************************************************************************/
/*                          GTIFGetOGISDefn()                           */
/************************************************************************/

char *GTIFGetOGISDefn( GTIF *hGTIF, GTIFDefn * psDefn )
{
    OGRSpatialReferenceH hSRS = GTIFGetOGISDefnAsOSR( hGTIF, psDefn );

    char *pszWKT = nullptr;
    if( hSRS &&
        OGRSpatialReference::FromHandle(hSRS)->exportToWkt( &pszWKT ) == OGRERR_NONE )
    {
        OSRDestroySpatialReference(hSRS);
        return pszWKT;
    }
    CPLFree(pszWKT);
    OSRDestroySpatialReference(hSRS);

    return nullptr;
}

/************************************************************************/
/*                     OGCDatumName2EPSGDatumCode()                     */
/************************************************************************/

static int OGCDatumName2EPSGDatumCode( GTIF * psGTIF,
                                       const char * pszOGCName )

{
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

    /* Search in database */
    auto ctx = static_cast<PJ_CONTEXT*>(
        GTIFGetPROJContext(psGTIF, true, nullptr));
    const PJ_TYPE searchType = PJ_TYPE_GEODETIC_REFERENCE_FRAME;
    auto list = proj_create_from_name(ctx, "EPSG", pszOGCName,
                                          &searchType, 1,
                                          true, /* approximate match */
                                          10,
                                          nullptr);
    if( list )
    {
        const auto listSize = proj_list_get_count(list);
        for( int i = 0; nReturn == KvUserDefined && i < listSize; i++ )
        {
            auto datum = proj_list_get(ctx, list, i);
            if( datum )
            {
                const char* pszDatumName = proj_get_name(datum);
                if( pszDatumName )
                {
                    char* pszTmp = CPLStrdup(pszDatumName);
                    WKTMassageDatum(&pszTmp);
                    if( EQUAL(pszTmp, pszOGCName) )
                    {
                        const char* pszCode = proj_get_id_code(datum, 0);
                        if( pszCode )
                        {
                            nReturn = atoi(pszCode);
                        }
                    }
                    CPLFree(pszTmp);
                }
            }
            proj_destroy(datum);
        }
        proj_list_destroy(list);
    }

    return nReturn;
}

/************************************************************************/
/*                        GTIFSetFromOGISDefn()                         */
/*                                                                      */
/*      Write GeoTIFF projection tags from an OGC WKT definition.       */
/************************************************************************/

int GTIFSetFromOGISDefn( GTIF * psGTIF, const char *pszOGCWKT )

{
/* -------------------------------------------------------------------- */
/*      Create an OGRSpatialReference object corresponding to the       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */

    OGRSpatialReference oSRS;
    if( oSRS.importFromWkt(pszOGCWKT) != OGRERR_NONE )
    {
        return FALSE;
    }
    return GTIFSetFromOGISDefnEx(psGTIF, OGRSpatialReference::ToHandle(&oSRS) ,
                                 GEOTIFF_KEYS_STANDARD,
                                 GEOTIFF_VERSION_1_0);
}

int GTIFSetFromOGISDefnEx( GTIF * psGTIF, OGRSpatialReferenceH hSRS,
                           GTIFFKeysFlavorEnum eFlavor,
                           GeoTIFFVersionEnum eVersion )
{
    std::map<geokey_t, std::string> oMapAsciiKeys;

    GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);

    const OGRSpatialReference *poSRS = OGRSpatialReference::FromHandle(hSRS);

/* -------------------------------------------------------------------- */
/*      Set version number.                                             */
/* -------------------------------------------------------------------- */
    if( eVersion == GEOTIFF_VERSION_AUTO)
    {
        if( poSRS->IsCompound() ||
            (poSRS->IsGeographic() && poSRS->GetAxesCount() == 3) )
        {
            eVersion = GEOTIFF_VERSION_1_1;
        }
        else
        {
            eVersion = GEOTIFF_VERSION_1_0;
        }
    }
    CPLAssert(eVersion == GEOTIFF_VERSION_1_0 || eVersion == GEOTIFF_VERSION_1_1);
    if( eVersion >= GEOTIFF_VERSION_1_1 )
    {
#if LIBGEOTIFF_VERSION >= 1600
        GTIFSetVersionNumbers(psGTIF,
                              GEOTIFF_SPEC_1_1_VERSION,
                              GEOTIFF_SPEC_1_1_KEY_REVISION,
                              GEOTIFF_SPEC_1_1_MINOR_REVISION);
#else
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Setting GeoTIFF 1.1 requires libgeotiff >= 1.6. Key values "
                 "will be written as GeoTIFF 1.1, but the version number "
                 "will be seen as 1.0, which might confuse GeoTIFF readers");
#endif
    }

/* -------------------------------------------------------------------- */
/*      Get the ellipsoid definition.                                   */
/* -------------------------------------------------------------------- */
    short nSpheroid = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID") != nullptr
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM|SPHEROID"),
                 "EPSG"))
    {
        nSpheroid = static_cast<short>(
            atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS|DATUM|SPHEROID")) );
    }
    else if( poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID") != nullptr
             && EQUAL(poSRS->GetAuthorityName("GEOGCS|DATUM|SPHEROID"),"EPSG"))
    {
        nSpheroid = static_cast<short>(
            atoi(poSRS->GetAuthorityCode("GEOGCS|DATUM|SPHEROID")) );
    }

    OGRErr eErr = OGRERR_NONE;
    double dfSemiMajor = 0;
    double dfInvFlattening = 0;
    if( !poSRS->IsLocal() )
    {
        dfSemiMajor = poSRS->GetSemiMajor( &eErr );
        dfInvFlattening = poSRS->GetInvFlattening( &eErr );
        if( eErr != OGRERR_NONE )
        {
            dfSemiMajor = 0.0;
            dfInvFlattening = 0.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the Datum so we can special case a few PCS codes.           */
/* -------------------------------------------------------------------- */
    int nDatum = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM") != nullptr
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS|DATUM"),"EPSG") )
        nDatum = atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS|DATUM"));
    else if( poSRS->GetAuthorityName("GEOGCS|DATUM") != nullptr
             && EQUAL(poSRS->GetAuthorityName("GEOGCS|DATUM"),"EPSG") )
        nDatum = atoi(poSRS->GetAuthorityCode("GEOGCS|DATUM"));
    else if( poSRS->GetAttrValue("DATUM") != nullptr )
        nDatum = OGCDatumName2EPSGDatumCode( psGTIF,
                                             poSRS->GetAttrValue("DATUM") );

/* -------------------------------------------------------------------- */
/*      Get the GCS if possible.                                        */
/* -------------------------------------------------------------------- */
    int nGCS = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS|GEOGCS") != nullptr
        && EQUAL(poSRS->GetAuthorityName("PROJCS|GEOGCS"),"EPSG") )
        nGCS = atoi(poSRS->GetAuthorityCode("PROJCS|GEOGCS"));
    else if( poSRS->GetAuthorityName("GEOGCS") != nullptr
             && EQUAL(poSRS->GetAuthorityName("GEOGCS"),"EPSG") )
        nGCS = atoi(poSRS->GetAuthorityCode("GEOGCS"));

    int nVerticalCSKeyValue = 0;
    bool hasEllipsoidHeight = !poSRS->IsCompound() &&
            poSRS->IsGeographic() && poSRS->GetAxesCount() == 3;
    if( nGCS == 4937 && eVersion >= GEOTIFF_VERSION_1_1 )
    {
        // Workaround a bug of PROJ 6.3.0
        // See https://github.com/OSGeo/PROJ/pull/1880
        // EPSG:4937 = ETRS89 3D
        hasEllipsoidHeight = true;
        nVerticalCSKeyValue = nGCS;
        nGCS = 4258; // ETRS89 2D
    }
    else if( nGCS != KvUserDefined )
    {
        OGRSpatialReference oGeogCRS;
        if( oGeogCRS.importFromEPSG(nGCS) == OGRERR_NONE &&
            oGeogCRS.IsGeographic() &&
            oGeogCRS.GetAxesCount() == 3 )
        {
            hasEllipsoidHeight = true;
            if( eVersion >= GEOTIFF_VERSION_1_1 )
            {
                const auto candidate_nVerticalCSKeyValue = nGCS;
                nGCS = KvUserDefined;

                // In case of a geographic 3D CRS, find the corresponding
                // geographic 2D CRS
                auto ctx = static_cast<PJ_CONTEXT*>(
                        GTIFGetPROJContext(psGTIF, true, nullptr));
                const auto type = PJ_TYPE_GEOGRAPHIC_2D_CRS;
                auto list = proj_create_from_name(ctx, "EPSG",
                                                oGeogCRS.GetName(),
                                                &type,
                                                1,
                                                false, // exact match
                                                1, // result set limit size,
                                                nullptr);
                if( list && proj_list_get_count(list) == 1 )
                {
                    auto crs2D = proj_list_get(ctx, list, 0);
                    if( crs2D )
                    {
                        const char* pszCode = proj_get_id_code(crs2D, 0);
                        if( pszCode )
                        {
                            nVerticalCSKeyValue = candidate_nVerticalCSKeyValue;
                            nGCS = atoi(pszCode);
                        }
                        proj_destroy(crs2D);
                    }
                }
                proj_list_destroy(list);
            }
        }
    }

    // Deprecated way of encoding ellipsoidal height
    if( hasEllipsoidHeight && nVerticalCSKeyValue == 0 )
    {
        if( nGCS == 4979 || nDatum == 6326 || nSpheroid == 7030 )
        {
            nVerticalCSKeyValue = 5030; // WGS_84_ellipsoid
            if( nGCS == 4979 || nDatum == 6326 )
            {
                nGCS = 4326;
            }
        }
        else if( nDatum >= 6001 && nDatum <= 6033 )
        {
            nVerticalCSKeyValue = nDatum - 1000;
        }
        else if( nSpheroid >= 7001 && nSpheroid <= 7033 )
        {
            nVerticalCSKeyValue = nSpheroid - 2000;
        }
    }

    if( nGCS > 32767 )
        nGCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get the linear units.                                           */
/* -------------------------------------------------------------------- */
    const char *pszLinearUOMNameTmp = nullptr;
    const double dfLinearUOM = poSRS->GetLinearUnits( &pszLinearUOMNameTmp );
    const std::string osLinearUOMName(pszLinearUOMNameTmp ? pszLinearUOMNameTmp : "");
    int nUOMLengthCode = 9001;  // Meters.

    if( poSRS->GetAuthorityName("PROJCS|UNIT") != nullptr
        && EQUAL(poSRS->GetAuthorityName("PROJCS|UNIT"),"EPSG")
        && poSRS->GetAttrNode( "PROJCS|UNIT" ) !=
        poSRS->GetAttrNode("GEOGCS|UNIT") )
        nUOMLengthCode = atoi(poSRS->GetAuthorityCode("PROJCS|UNIT"));
    else if( EQUAL(osLinearUOMName.c_str(),SRS_UL_FOOT)
        || fabs(dfLinearUOM - GTIFAtof(SRS_UL_FOOT_CONV)) < 0.0000001 )
        nUOMLengthCode = 9002;  // International foot.
    else if( EQUAL(osLinearUOMName.c_str(),SRS_UL_US_FOOT) ||
             std::abs(dfLinearUOM - GTIFAtof(SRS_UL_US_FOOT_CONV)) <
             0.0000001 )
        nUOMLengthCode = 9003;  // US survey foot.
    else if( fabs(dfLinearUOM - 1.0) > 0.00000001 )
        nUOMLengthCode = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get some authority values.                                      */
/* -------------------------------------------------------------------- */
    int nPCS = KvUserDefined;

    if( poSRS->GetAuthorityName("PROJCS") != nullptr
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
    bool bUnknownProjection = false;

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
    else if( pszProjection == nullptr )
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
            const int nProjection = nZone + (bNorth ? 16000 : 16100);
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

    else if( EQUAL(pszProjection,
                   "Laborde_Oblique_Mercator") )
    {
        GTIFKeySet(psGTIF, GTModelTypeGeoKey, TYPE_SHORT, 1,
                   ModelTypeProjected);
        GTIFKeySet(psGTIF, ProjectedCSTypeGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );
        GTIFKeySet(psGTIF, ProjectionGeoKey, TYPE_SHORT, 1,
                   KvUserDefined );

        GTIFKeySet(psGTIF, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                   CT_ObliqueMercator_Laborde );

        GTIFKeySet(psGTIF, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, 0.0 ) );

        GTIFKeySet(psGTIF, ProjAzimuthAngleGeoKey, TYPE_DOUBLE, 1,
                   poSRS->GetNormProjParm( SRS_PP_AZIMUTH, 0.0 ) );

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
        bUnknownProjection = true;
    }

    // Note that VERTCS is an ESRI "spelling" of VERT_CS so we assume if
    // we find it that we should try to treat this as a PE string.
    if( eFlavor == GEOTIFF_KEYS_ESRI_PE ||
        poSRS->GetAttrValue("VERTCS") != nullptr )
    {
        bWritePEString = true;
    }

    if( nPCS == KvUserDefined )
    {
        const char* pszPROJ4Ext = poSRS->GetExtension("PROJCS", "PROJ4", nullptr);
        if( pszPROJ4Ext && strstr(pszPROJ4Ext, "+proj=merc +a=6378137 +b=6378137") )
        {
            bWritePEString = true;
        }
    }

    bWritePEString &=
        CPLTestBool( CPLGetConfigOption("GTIFF_ESRI_CITATION", "YES") );

    bool peStrStored = false;

    if( bWritePEString )
    {
        // Anything we can't map, store as an ESRI PE string with a citation key.
        char *pszPEString = nullptr;
        // We cheat a bit, but if we have a custom_proj4, do not morph to ESRI
        // so as to keep the EXTENSION PROJ4 node
        const char* const apszOptionsDefault[] = { nullptr };
        const char* const apszOptionsEsri[] = { "FORMAT=WKT1_ESRI", nullptr };
        const char* const * papszOptions = apszOptionsDefault;
        if( !(bUnknownProjection &&
              poSRS->GetExtension("PROJCS", "PROJ4", nullptr) != nullptr) )
        {
            papszOptions = apszOptionsEsri;
        }
        poSRS->exportToWkt( &pszPEString, papszOptions );
        const int peStrLen = static_cast<int>(strlen(pszPEString));
        if(peStrLen > 0)
        {
            char *outPeStr = static_cast<char *>(
                CPLMalloc( peStrLen + strlen("ESRI PE String = ") + 1 ) );
            strcpy(outPeStr, "ESRI PE String = ");
            strcat(outPeStr, pszPEString);
            oMapAsciiKeys[PCSCitationGeoKey] = outPeStr;
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
            oMapAsciiKeys[GTCitationGeoKey] =
                "PCS Name = WGS_1984_Web_Mercator_Auxiliary_Sphere";
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

    if( eVersion == GEOTIFF_VERSION_1_0 &&
        (GDALGTIFKeyGetDOUBLE(psGTIF, ProjFalseEastingGeoKey, &dfFE, 0, 1)
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
    else if( !poSRS->IsGeographic() &&
             (nPCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0 ) )
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
            && !osLinearUOMName.empty()
            && CPLTestBool( CPLGetConfigOption("GTIFF_ESRI_CITATION",
                                               "YES") ) )
        {
            SetLinearUnitCitation(oMapAsciiKeys, osLinearUOMName.c_str());
        }
    }

/* -------------------------------------------------------------------- */
/*      Write angular units.                                            */
/* -------------------------------------------------------------------- */

    const char* angUnitName = "";
    if( nGCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0 )
    {
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
            oMapAsciiKeys[GeogCitationGeoKey] = angUnitName;
            GTIFKeySet(psGTIF, GeogAngularUnitSizeGeoKey, TYPE_DOUBLE, 1,
                    angUnitValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to write a citation from the main coordinate system         */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    if( poSRS->GetName() != nullptr
        && ((poSRS->IsProjected() && (nPCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0)) ||
            poSRS->IsCompound() || poSRS->IsLocal() ||
            (poSRS->IsGeocentric() && (nGCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0))) )
    {
        if( !(bWritePEString && nPCS == 3857) )
        {
            oMapAsciiKeys[GTCitationGeoKey] = poSRS->GetName();
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to write a GCS citation.                                    */
/* -------------------------------------------------------------------- */
    if( nGCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0 )
    {
        const OGR_SRSNode *poGCS = poSRS->GetAttrNode( "GEOGCS" );

        if( poGCS != nullptr && poGCS->GetChild(0) != nullptr )
        {
            oMapAsciiKeys[GeogCitationGeoKey] = poGCS->GetChild(0)->GetValue();
        }
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
        else if( poSRS->GetAttrValue("DATUM") != nullptr
                 && strstr(poSRS->GetAttrValue("DATUM"), "unknown") == nullptr
                 && strstr(poSRS->GetAttrValue("DATUM"), "unnamed") == nullptr )

        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Couldn't translate `%s' to a GeoTIFF datum.",
                      poSRS->GetAttrValue("DATUM") );
        }

        if( nGCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0 )
        {
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
            {
                SetGeogCSCitation(psGTIF, oMapAsciiKeys,
                                poSRS, angUnitName, nDatum, nSpheroid);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have TOWGS84 parameters?                                  */
/* -------------------------------------------------------------------- */
#if !defined(GEO_NORMALIZE_DISABLE_TOWGS84)
    double adfTOWGS84[7] = { 0.0 };

    if( (nGCS == KvUserDefined || eVersion == GEOTIFF_VERSION_1_0 ) &&
        poSRS->GetTOWGS84( adfTOWGS84 ) == OGRERR_NONE )
    {
        // If we are writing a SRS with a EPSG code, and that the EPSG code
        // of the current SRS object and the one coming from the EPSG code
        // are the same, then by default, do not write them.
        bool bUseReferenceTOWGS84 = false;
        const char* pszAuthName = poSRS->GetAuthorityName(nullptr);
        const char* pszAuthCode = poSRS->GetAuthorityCode(nullptr);
        if( pszAuthName && EQUAL(pszAuthName, "EPSG") && pszAuthCode )
        {
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            OGRSpatialReference oRefSRS;
            double adfRefTOWGS84[7] = { 0.0 };
            if( oRefSRS.importFromEPSG(atoi(pszAuthCode)) == OGRERR_NONE )
            {
                oRefSRS.AddGuessedTOWGS84();
                if( oRefSRS.GetTOWGS84(adfRefTOWGS84) == OGRERR_NONE &&
                    memcmp(adfRefTOWGS84, adfTOWGS84, sizeof(adfTOWGS84)) == 0 )
                {
                    bUseReferenceTOWGS84 = true;
                }
            }
        }
        const char* pszWriteTOWGS84 =
            CPLGetConfigOption("GTIFF_WRITE_TOWGS84", "AUTO");
        if( (EQUAL(pszWriteTOWGS84, "YES") || EQUAL(pszWriteTOWGS84, "TRUE") ||
             EQUAL(pszWriteTOWGS84, "ON")) ||
            (!bUseReferenceTOWGS84 && EQUAL(pszWriteTOWGS84, "AUTO") ) )
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
    }
#endif

/* -------------------------------------------------------------------- */
/*      Do we have vertical information to set?                         */
/* -------------------------------------------------------------------- */
    if( poSRS->GetAttrValue( "COMPD_CS|VERT_CS" ) != nullptr )
    {
        bool bGotVertCSCode = false;
        const char *pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS" );
        if( pszValue && atoi(pszValue) )
        {
            bGotVertCSCode = true;
            GTIFKeySet( psGTIF, VerticalCSTypeGeoKey, TYPE_SHORT, 1,
                        atoi(pszValue) );
        }

        if( eVersion == GEOTIFF_VERSION_1_0 || !bGotVertCSCode )
        {
            oMapAsciiKeys[VerticalCitationGeoKey] =
                        poSRS->GetAttrValue( "COMPD_CS|VERT_CS" );

            pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS|VERT_DATUM" );
            if( pszValue && atoi(pszValue) )
                GTIFKeySet( psGTIF, VerticalDatumGeoKey, TYPE_SHORT, 1,
                            atoi(pszValue) );

            pszValue = poSRS->GetAuthorityCode( "COMPD_CS|VERT_CS|UNIT" );
            if( pszValue && atoi(pszValue) )
                GTIFKeySet( psGTIF, VerticalUnitsGeoKey, TYPE_SHORT, 1,
                            atoi(pszValue) );
        }
    }
    else if( eVersion >= GEOTIFF_VERSION_1_1 && nVerticalCSKeyValue != 0 )
    {
        GTIFKeySet( psGTIF, VerticalCSTypeGeoKey, TYPE_SHORT, 1, nVerticalCSKeyValue );
    }

    const double dfCoordinateEpoch = poSRS->GetCoordinateEpoch();
    if( dfCoordinateEpoch > 0 )
    {
        GTIFKeySet(psGTIF, CoordinateEpochGeoKey, TYPE_DOUBLE, 1,
                   dfCoordinateEpoch );
    }

/* -------------------------------------------------------------------- */
/*      Write all ascii keys                                            */
/* -------------------------------------------------------------------- */
    for( const auto& oIter: oMapAsciiKeys )
    {
        GTIFKeySet( psGTIF, oIter.first, TYPE_ASCII, 0, oIter.second.c_str() );
    }

    return TRUE;
}

/************************************************************************/
/*                         GTIFWktFromMemBuf()                          */
/************************************************************************/

CPLErr GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer,
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList )
{
    OGRSpatialReferenceH hSRS = nullptr;
    if( ppszWKT )
        *ppszWKT = nullptr;
    CPLErr eErr = GTIFWktFromMemBufEx( nSize, pabyBuffer, &hSRS, padfGeoTransform,
                                pnGCPCount, ppasGCPList, nullptr, nullptr );
    if( eErr == CE_None )
    {
        if( hSRS && ppszWKT )
        {
            OSRExportToWkt(hSRS, ppszWKT);
        }
    }
    OSRDestroySpatialReference(hSRS);
    return eErr;
}

CPLErr GTIFWktFromMemBufEx( int nSize, unsigned char *pabyBuffer,
                            OGRSpatialReferenceH* phSRS, double *padfGeoTransform,
                            int *pnGCPCount, GDAL_GCP **ppasGCPList,
                            int *pbPixelIsPoint, char*** ppapszRPCMD )

{
    char szFilename[100] = {};

    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/wkt_from_mem_buf_%ld.tif",
              static_cast<long>( CPLGetPID() ) );

/* -------------------------------------------------------------------- */
/*      Initialization of libtiff and libgeotiff.                       */
/* -------------------------------------------------------------------- */
    GTiffOneTimeInit();  // For RPC tag.
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Create a memory file from the buffer.                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFileFromMemBuffer( szFilename, pabyBuffer, nSize, FALSE );
    if( fp == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "rc", fp );

    if( hTIFF == nullptr )
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
    unsigned short nRasterType = 0;

    GTIF *hGTIF = GTIFNew(hTIFF);

    if( hGTIF != nullptr && GDALGTIFKeyGetSHORT(hGTIF, GTRasterTypeGeoKey,
                                             &nRasterType, 0, 1 ) == 1
        && nRasterType == static_cast<unsigned short>( RasterPixelIsPoint ) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
                                            "FALSE") );
    }
    if( pbPixelIsPoint )
        *pbPixelIsPoint = bPixelIsPoint;
    if( ppapszRPCMD )
        *ppapszRPCMD = nullptr;

    if( phSRS )
    {
        *phSRS = nullptr;
        if( hGTIF != nullptr )
        {
            GTIFDefn *psGTIFDefn = GTIFAllocDefn();
            if( GTIFGetDefn( hGTIF, psGTIFDefn) )
            {
                *phSRS = GTIFGetOGISDefnAsOSR( hGTIF, psGTIFDefn );
            }
            GTIFFreeDefn(psGTIFDefn);
        }
    }
    if( hGTIF )
        GTIFFree( hGTIF );

/* -------------------------------------------------------------------- */
/*      Get geotransform or tiepoints.                                  */
/* -------------------------------------------------------------------- */
    double *padfTiePoints = nullptr;
    double *padfScale = nullptr;
    double *padfMatrix = nullptr;
    int16_t nCount = 0;

    padfGeoTransform[0] = 0.0;
    padfGeoTransform[1] = 1.0;
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = 0.0;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = 1.0;

    *pnGCPCount = 0;
    *ppasGCPList = nullptr;

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
    if( ppapszRPCMD != nullptr )
    {
        *ppapszRPCMD = GTiffDatasetReadRPCTag( hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    XTIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    VSIUnlink( szFilename );

    if( phSRS && *phSRS == nullptr )
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
    OGRSpatialReference oSRS;
    if( pszWKT != nullptr )
        oSRS.importFromWkt(pszWKT);
    return GTIFMemBufFromSRS(OGRSpatialReference::ToHandle(&oSRS),
                             padfGeoTransform,
                             nGCPCount,pasGCPList,
                             pnSize, ppabyBuffer, FALSE, nullptr);
}

CPLErr GTIFMemBufFromSRS( OGRSpatialReferenceH hSRS, const double *padfGeoTransform,
                            int nGCPCount, const GDAL_GCP *pasGCPList,
                            int *pnSize, unsigned char **ppabyBuffer,
                            int bPixelIsPoint, char** papszRPCMD )

{
    char szFilename[100] = {};

    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/wkt_from_mem_buf_%ld.tif",
              static_cast<long>( CPLGetPID() ) );

/* -------------------------------------------------------------------- */
/*      Initialization of libtiff and libgeotiff.                       */
/* -------------------------------------------------------------------- */
    GTiffOneTimeInit();  // For RPC tag.
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    VSILFILE* fpL = VSIFOpenL( szFilename, "w" );
    if( fpL == nullptr )
        return CE_Failure;

    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "w", fpL );

    if( hTIFF == nullptr )
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

    GTIF *hGTIF = nullptr;
    if( hSRS != nullptr || bPixelIsPoint )
    {
        hGTIF = GTIFNew(hTIFF);
        if( hSRS != nullptr )
            GTIFSetFromOGISDefnEx( hGTIF, hSRS,
                                   GEOTIFF_KEYS_STANDARD,
                                   GEOTIFF_VERSION_1_0 );

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
    if( papszRPCMD != nullptr )
    {
        GTiffDatasetWriteRPCTag( hTIFF, papszRPCMD );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return the created memory buffer.                   */
/* -------------------------------------------------------------------- */
    GByte bySmallImage = 0;

    TIFFWriteEncodedStrip( hTIFF, 0, reinterpret_cast<char *>(&bySmallImage), 1 );
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
