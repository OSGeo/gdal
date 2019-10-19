/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from ESRI .prj definitions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
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
#include "ogr_spatialref.h"
#include "ogr_srs_esri_names.h"

#include <cmath>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/* -------------------------------------------------------------------- */
/*      Table relating USGS and ESRI state plane zones.                 */
/* -------------------------------------------------------------------- */
constexpr int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0,
 5400,    0
};

/************************************************************************/
/*                           ESRIToUSGSZone()                           */
/*                                                                      */
/*      Convert ESRI style state plane zones to USGS style state        */
/*      plane zones.                                                    */
/************************************************************************/

static int ESRIToUSGSZone( int nESRIZone )

{
    // anUsgsEsriZones is a series of ints where 2 consecutive integers
    // are used to map from USGS to ESRI state plane zones.
    // TODO(schwehr): Would be better as a std::map.
    const int nPairs = sizeof(anUsgsEsriZones) / (2 * sizeof(int));

    for( int i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nESRIZone )
            return anUsgsEsriZones[i*2];
    }

    return 0;
}

/************************************************************************/
/*                         OSRImportFromESRI()                          */
/************************************************************************/

/**
 * \brief Import coordinate system from ESRI .prj format(s).
 *
 * This function is the same as the C++ method
 * OGRSpatialReference::importFromESRI().
 */
OGRErr OSRImportFromESRI( OGRSpatialReferenceH hSRS, char **papszPrj )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromESRI", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        importFromESRI( papszPrj );
}

/************************************************************************/
/*                              OSR_GDV()                               */
/*                                                                      */
/*      Fetch a particular parameter out of the parameter list, or      */
/*      the indicated default if it isn't available.  This is a         */
/*      helper function for importFromESRI().                           */
/************************************************************************/

static double OSR_GDV( char **papszNV, const char * pszField,
                       double dfDefaultValue )

{
    if( papszNV == nullptr || papszNV[0] == nullptr )
        return dfDefaultValue;

    if( STARTS_WITH_CI(pszField, "PARAM_") )
    {
        int iLine = 0;  // Used after for loop.
        for( ;
             papszNV[iLine] != nullptr &&
                 !STARTS_WITH_CI(papszNV[iLine], "Paramet");
             iLine++ ) {}

        for( int nOffset = atoi(pszField+6);
             papszNV[iLine] != nullptr && nOffset > 0;
             iLine++ )
        {
            if( strlen(papszNV[iLine]) > 0 )
                nOffset--;
        }

        while( papszNV[iLine] != nullptr && strlen(papszNV[iLine]) == 0 )
            iLine++;

        if( papszNV[iLine] != nullptr )
        {
            char * const pszLine = papszNV[iLine];

            // Trim comments.
            for( int i=0; pszLine[i] != '\0'; i++ )
            {
                if( pszLine[i] == '/' && pszLine[i+1] == '*' )
                    pszLine[i] = '\0';
            }

            double dfValue = 0.0;
            char **papszTokens = CSLTokenizeString(papszNV[iLine]);
            if( CSLCount(papszTokens) == 3 )
            {
                // http://agdcftp1.wr.usgs.gov/pub/projects/lcc/akcan_lcc/akcan.tar.gz
                // contains weird values for the second. Ignore it and
                // the result looks correct.
                double dfSecond = CPLAtof(papszTokens[2]);
                if( dfSecond < 0.0 || dfSecond >= 60.0 )
                    dfSecond = 0.0;

                dfValue = std::abs(CPLAtof(papszTokens[0]))
                    + CPLAtof(papszTokens[1]) / 60.0
                    + dfSecond / 3600.0;

                if( CPLAtof(papszTokens[0]) < 0.0 )
                    dfValue *= -1;
            }
            else if( CSLCount(papszTokens) > 0 )
            {
                dfValue = CPLAtof(papszTokens[0]);
            }
            else
            {
                dfValue = dfDefaultValue;
            }

            CSLDestroy( papszTokens );

            return dfValue;
        }

        return dfDefaultValue;
    }

    int iLine = 0;  // Used after for loop.
    for( ;
         papszNV[iLine] != nullptr &&
             !EQUALN(papszNV[iLine], pszField, strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == nullptr )
        return dfDefaultValue;

    return CPLAtof( papszNV[iLine] + strlen(pszField) );
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static CPLString OSR_GDS( char **papszNV, const char * pszField,
                          const char *pszDefaultValue )

{
    if( papszNV == nullptr || papszNV[0] == nullptr )
        return pszDefaultValue;

    int iLine = 0;  // Used after for loop.
    for( ;
         papszNV[iLine] != nullptr &&
             !EQUALN(papszNV[iLine], pszField, strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == nullptr )
        return pszDefaultValue;

    char **papszTokens = CSLTokenizeString(papszNV[iLine]);

    CPLString osResult =
        CSLCount(papszTokens) > 1 ? papszTokens[1] : pszDefaultValue;

    CSLDestroy( papszTokens );
    return osResult;
}

/************************************************************************/
/*                          importFromESRI()                            */
/************************************************************************/

/**
 * \brief Import coordinate system from ESRI .prj format(s).
 *
 * This function will read the text loaded from an ESRI .prj file, and
 * translate it into an OGRSpatialReference definition.  This should support
 * many (but by no means all) old style (Arc/Info 7.x) .prj files, as well
 * as the newer pseudo-OGC WKT .prj files.  Note that new style .prj files
 * are in OGC WKT format, but require some manipulation to correct datum
 * names, and units on some projection parameters.  This is addressed within
 * importFromESRI() by an automatic call to morphFromESRI().
 *
 * Currently only GEOGRAPHIC, UTM, STATEPLANE, GREATBRITIAN_GRID, ALBERS,
 * EQUIDISTANT_CONIC, TRANSVERSE (mercator), POLAR, MERCATOR and POLYCONIC
 * projections are supported from old style files.
 *
 * At this time there is no equivalent exportToESRI() method.  Writing old
 * style .prj files is not supported by OGRSpatialReference. However the
 * morphToESRI() and exportToWkt() methods can be used to generate output
 * suitable to write to new style (Arc 8) .prj files.
 *
 * This function is the equivalent of the C function OSRImportFromESRI().
 *
 * @param papszPrj NULL terminated list of strings containing the definition.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 */

OGRErr OGRSpatialReference::importFromESRI( char **papszPrj )

{
    if( papszPrj == nullptr || papszPrj[0] == nullptr )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      ArcGIS and related products now use a variant of Well Known     */
/*      Text.  Try to recognize this and ingest it.  WKT is usually     */
/*      all on one line, but we will accept multi-line formats and      */
/*      concatenate.                                                    */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(papszPrj[0], "GEOGCS")
        || STARTS_WITH_CI(papszPrj[0], "PROJCS")
        || STARTS_WITH_CI(papszPrj[0], "LOCAL_CS")
        // Also accept COMPD_CS, even if it is unclear that it is valid
        // traditional ESRI WKT. But people might use such PRJ file
        // See https://github.com/OSGeo/gdal/issues/1881
        || STARTS_WITH_CI(papszPrj[0], "COMPD_CS") )
    {
        std::string osWKT(papszPrj[0]);
        for( int i = 1; papszPrj[i] != nullptr; i++ )
        {
            osWKT += papszPrj[i];
        }
        return importFromWkt( osWKT.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    CPLString osProj = OSR_GDS( papszPrj, "Projection", "" );
    bool bDatumApplied = false;

    if( EQUAL(osProj, "") )
    {
        CPLDebug( "OGR_ESRI", "Can't find Projection" );
        return OGRERR_CORRUPT_DATA;
    }
    else if( EQUAL(osProj, "GEOGRAPHIC") )
    {
        // Nothing to do.
    }
    else if( EQUAL(osProj, "utm") )
    {
        const double dfOsrGdv = OSR_GDV(papszPrj, "zone", 0.0);
        if( dfOsrGdv > 0 && dfOsrGdv < 61 )
        {
            const double dfYShift = OSR_GDV(papszPrj, "Yshift", 0.0);

            SetUTM(static_cast<int>(dfOsrGdv), dfYShift == 0.0);
        }
        else
        {
            const double dfCentralMeridian = OSR_GDV(papszPrj, "PARAM_1", 0.0);
            const double dfRefLat = OSR_GDV(papszPrj, "PARAM_2", 0.0);
            if( dfCentralMeridian >= -180.0 && dfCentralMeridian <= 180.0 )
            {
                const int nZone = static_cast<int>(
                    (dfCentralMeridian + 183.0) / 6.0 + 0.0000001 );
                SetUTM( nZone, dfRefLat >= 0.0 );
            }
        }
    }
    else if( EQUAL(osProj, "STATEPLANE") )
    {
        const double dfZone = OSR_GDV(papszPrj, "zone", 0.0);

        if( dfZone < std::numeric_limits<int>::min() ||
            dfZone > std::numeric_limits<int>::max() ||
            CPLIsNan(dfZone) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "zone out of range: %f", dfZone);
            return OGRERR_CORRUPT_DATA;
        }

        int nZone = static_cast<int>( dfZone );

        if( nZone != 0 )
            nZone = ESRIToUSGSZone( nZone );
        else
        {
            const double dfFipszone = OSR_GDV(papszPrj, "fipszone", 0.0);

            if( dfFipszone < std::numeric_limits<int>::min() ||
                dfFipszone > std::numeric_limits<int>::max() ||
                CPLIsNan(dfFipszone) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "fipszone out of range: %f", dfFipszone);
                return OGRERR_CORRUPT_DATA;
            }

            nZone = static_cast<int>( dfFipszone );
        }

        if( nZone != 0 )
        {
            bDatumApplied = true;
            if( EQUAL(OSR_GDS( papszPrj, "Datum", "NAD83"), "NAD27") )
                SetStatePlane( nZone, FALSE );
            else
                SetStatePlane( nZone, TRUE );
        }
    }
    else if( EQUAL(osProj, "GREATBRITIAN_GRID")
             || EQUAL(osProj, "GREATBRITAIN_GRID") )
    {
        const char *pszWkt =
            "PROJCS[\"OSGB 1936 / British National Grid\","
            "GEOGCS[\"OSGB 1936\",DATUM[\"OSGB_1936\","
            "SPHEROID[\"Airy 1830\",6377563.396,299.3249646]],"
            "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
            "PROJECTION[\"Transverse_Mercator\"],"
            "PARAMETER[\"latitude_of_origin\",49],"
            "PARAMETER[\"central_meridian\",-2],"
            "PARAMETER[\"scale_factor\",0.999601272],"
            "PARAMETER[\"false_easting\",400000],"
            "PARAMETER[\"false_northing\",-100000],UNIT[\"metre\",1]]";

        bDatumApplied = true;
        importFromWkt( pszWkt );
    }
    else if( EQUAL(osProj, "ALBERS") )
    {
        SetACEA( OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
    }
    else if( EQUAL(osProj, "LAMBERT") )
    {
        SetLCC( OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
    }
    else if( EQUAL(osProj, "LAMBERT_AZIMUTHAL") )
    {
        SetLAEA( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                 OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, "EQUIDISTANT_CONIC") )
    {
        const double dfStdPCount = OSR_GDV( papszPrj, "PARAM_1", 0.0 );
        // TODO(schwehr): What is a reasonable range for StdPCount?
        if( dfStdPCount < 0 || dfStdPCount > std::numeric_limits<int>::max() ||
            CPLIsNan(dfStdPCount) )
        {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "StdPCount out of range: %lf", dfStdPCount);
                return OGRERR_CORRUPT_DATA;
        }
        const int nStdPCount = static_cast<int>(dfStdPCount);

        if( nStdPCount == 1 )
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
        }
        else
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ),
                   OSR_GDV( papszPrj, "PARAM_7", 0.0 ) );
        }
    }
    else if( EQUAL(osProj, "TRANSVERSE") )
    {
        SetTM( OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_4", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_5", 0.0 ) );
    }
    else if( EQUAL(osProj, "POLAR") )
    {
        SetPS( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
               1.0,
               OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
               OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, "MERCATOR") )
    {
        SetMercator2SP( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                     0.0,
                     OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                     OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                     OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else if( EQUAL(osProj, SRS_PT_MERCATOR_AUXILIARY_SPHERE) )
    {
       // This is EPSG:3875 Pseudo Mercator. We might as well import it from
       // the EPSG spec.
       importFromEPSG(3857);
       bDatumApplied = true;
    }
    else if( EQUAL(osProj, "POLYCONIC") )
    {
        SetPolyconic( OSR_GDV( papszPrj, "PARAM_2", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_1", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_3", 0.0 ),
                      OSR_GDV( papszPrj, "PARAM_4", 0.0 ) );
    }
    else
    {
        CPLDebug( "OGR_ESRI", "Unsupported projection: %s", osProj.c_str() );
        SetLocalCS( osProj );
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */
    if( !IsLocal() && !bDatumApplied )
    {
        const CPLString osDatum = OSR_GDS( papszPrj, "Datum", "");

        if( EQUAL(osDatum, "NAD27") || EQUAL(osDatum, "NAD83")
            || EQUAL(osDatum, "WGS84") || EQUAL(osDatum, "WGS72") )
        {
            SetWellKnownGeogCS( osDatum );
        }
        else if( EQUAL( osDatum, "EUR" )
                 || EQUAL( osDatum, "ED50" ) )
        {
            SetWellKnownGeogCS( "EPSG:4230" );
        }
        else if( EQUAL( osDatum, "GDA94" ) )
        {
            SetWellKnownGeogCS( "EPSG:4283" );
        }
        else
        {
            CPLString osSpheroid = OSR_GDS( papszPrj, "Spheroid", "");

            if( EQUAL(osSpheroid, "INT1909")
                || EQUAL(osSpheroid, "INTERNATIONAL1909") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4022 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "AIRY") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4001 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "CLARKE1866") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4008 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "GRS80") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4019 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "KRASOVSKY")
                     || EQUAL(osSpheroid, "KRASSOVSKY")
                     || EQUAL(osSpheroid, "KRASSOWSKY") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4024 );
                CopyGeogCSFrom( &oGCS );
            }
            else if( EQUAL(osSpheroid, "Bessel") )
            {
                OGRSpatialReference oGCS;
                oGCS.importFromEPSG( 4004 );
                CopyGeogCSFrom( &oGCS );
            }
            else
            {
                bool bFoundParameters = false;
                for( int iLine = 0; papszPrj[iLine] != nullptr; iLine++ )
                {
                    if( STARTS_WITH_CI(papszPrj[iLine], "Parameters") )
                    {
                        char** papszTokens =
                            CSLTokenizeString(papszPrj[iLine] +
                                              strlen("Parameters"));
                        if( CSLCount(papszTokens) == 2 )
                        {
                            OGRSpatialReference oGCS;
                            const double dfSemiMajor = CPLAtof(papszTokens[0]);
                            const double dfSemiMinor = CPLAtof(papszTokens[1]);
                            const double dfInvFlattening =
                                OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
                            oGCS.SetGeogCS( "unknown", "unknown", "unknown",
                                            dfSemiMajor, dfInvFlattening );
                            CopyGeogCSFrom( &oGCS );
                            bFoundParameters = true;
                        }
                        CSLDestroy(papszTokens);
                        break;
                    }
                }
                if( !bFoundParameters )
                {
                    // If unknown, default to WGS84 so there is something there.
                    SetWellKnownGeogCS( "WGS84" );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Linear units translation                                        */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
    {
        const double dfOldUnits = GetLinearUnits();
        const CPLString osValue = OSR_GDS( papszPrj, "Units", "" );
        CPLString osOldAuth;
        {
            const char* pszOldAuth = GetAuthorityCode(nullptr);
            if( pszOldAuth )
                osOldAuth = pszOldAuth;
        }

        if( EQUAL(osValue, "" ) )
            SetLinearUnitsAndUpdateParameters( SRS_UL_METER, 1.0 );
        else if( EQUAL(osValue, "FEET") )
            SetLinearUnitsAndUpdateParameters( SRS_UL_US_FOOT,
                                               CPLAtof(SRS_UL_US_FOOT_CONV) );
        else if( CPLAtof(osValue) != 0.0 )
            SetLinearUnitsAndUpdateParameters( "user-defined",
                                               1.0 / CPLAtof(osValue) );
        else
            SetLinearUnitsAndUpdateParameters( osValue, 1.0 );

        // Reinstall authority if linear units value has not changed (bug #1697)
        const double dfNewUnits = GetLinearUnits();
        if( IsProjected() && !osOldAuth.empty() && dfOldUnits != 0.0
            && std::abs( dfNewUnits / dfOldUnits - 1) < 1e-8 )
        {
            SetAuthority("PROJCS", "EPSG", atoi(osOldAuth));
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       FindCodeFromDict()                             */
/*                                                                      */
/*      Find the code from a dict file.                                 */
/************************************************************************/
static int FindCodeFromDict( const char* pszDictFile, const char* CSName,
                             char* code )
{
/* -------------------------------------------------------------------- */
/*      Find and open file.                                             */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CPLFindFile( "gdal", pszDictFile );
    if( pszFilename == nullptr )
        return OGRERR_UNSUPPORTED_SRS;

    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == nullptr )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    OGRErr eErr = OGRERR_UNSUPPORTED_SRS;
    const char *pszLine = nullptr;

    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        if( pszLine[0] == '#' )
            continue;

        if( strstr(pszLine, CSName) )
        {
            const char* pComma = strchr(pszLine, ',');
            if( pComma )
            {
                strncpy( code, pszLine, pComma - pszLine);
                code[pComma - pszLine] = '\0';
                eErr = OGRERR_NONE;
            }
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

    return eErr;
}

/************************************************************************/
/*                    ImportFromESRIStatePlaneWKT()                     */
/*                                                                      */
/*      Search a ESRI State Plane WKT and import it.                    */
/************************************************************************/

OGRErr OGRSpatialReference::ImportFromESRIStatePlaneWKT(
    int code, const char* datumName, const char* unitsName,
    int pcsCode, const char* csName )
{
    // If the CS name is known.
    if( code == 0 && !datumName && !unitsName && pcsCode == 32767 && csName )
    {
        char codeS[10] = {};
        if( FindCodeFromDict( "esri_StatePlane_extra.wkt", csName, codeS )
            != OGRERR_NONE )
            return OGRERR_FAILURE;
        return importFromDict( "esri_StatePlane_extra.wkt", codeS);
    }

    int searchCode = -1;
    if( unitsName == nullptr )
        unitsName = "";

    // Find state plane prj str by pcs code only.
    if( code == 0 && !datumName && pcsCode != 32767 )
    {
        int unitCode = 1;
        if( EQUAL(unitsName, "international_feet") )
            unitCode = 3;
        else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
            unitCode = 2;

        for( int i = 0; statePlanePcsCodeToZoneCode[i] != 0; i += 2 )
        {
            if( pcsCode == statePlanePcsCodeToZoneCode[i] )
            {
                searchCode = statePlanePcsCodeToZoneCode[i+1];
                const int unitIndex = searchCode % 10;
                if( (unitCode == 1 && !(unitIndex == 0 || unitIndex == 1))
                    || (unitCode == 2 && !(unitIndex == 2 || unitIndex == 3 ||
                                           unitIndex == 4 ))
                    || (unitCode == 3 && !(unitIndex == 5 || unitIndex == 6 )) )
                {
                    searchCode -= unitIndex;
                    switch( unitIndex )
                    {
                      case 0:
                      case 3:
                      case 5:
                        if( unitCode == 2 )
                            searchCode += 3;
                        else if( unitCode == 3 )
                            searchCode += 5;
                        break;
                      case 1:
                      case 2:
                      case 6:
                        if( unitCode == 1 )
                            searchCode += 1;
                        if( unitCode == 2 )
                            searchCode += 2;
                        else if( unitCode == 3 )
                            searchCode += 6;
                        break;
                      case 4:
                        // FIXME? The following cond is not possible:
                        // if( unitCode == 2 )
                        //     searchCode += 4;
                        break;
                    }
                }
                break;
            }
        }
    }
    else // Find state plane prj str by all inputs.
    {
        if( code < 0 || code > INT_MAX / 10 )
            return OGRERR_FAILURE;

        // Need to have a special EPSG-ESRI zone code mapping first.
        for( int i = 0; statePlaneZoneMapping[i] != 0; i += 3 )
        {
            if( code == statePlaneZoneMapping[i]
                && (statePlaneZoneMapping[i+1] == -1 ||
                    pcsCode == statePlaneZoneMapping[i+1]))
            {
                code = statePlaneZoneMapping[i+2];
                break;
            }
        }
        searchCode = code * 10;
        if( !datumName )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "datumName is NULL.");
            return OGRERR_FAILURE;
        }
        if( EQUAL(datumName, "HARN") )
        {
            if( EQUAL(unitsName, "international_feet") )
                searchCode += 5;
            else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
                searchCode += 3;
        }
        else if( strstr(datumName, "NAD") && strstr(datumName, "83") )
        {
            if( EQUAL(unitsName, "meters") )
                searchCode += 1;
            else if( EQUAL(unitsName, "international_feet") )
                searchCode += 6;
            else if( strstr(unitsName, "feet") || strstr(unitsName, "foot") )
                searchCode += 2;
        }
        else if( strstr(datumName, "NAD") && strstr(datumName, "27") &&
                 !EQUAL(unitsName, "meters") )
        {
            searchCode += 4;
        }
        else
            searchCode = -1;
    }
    if( searchCode > 0 )
    {
        char codeS[20] = {};
        snprintf(codeS, sizeof(codeS), "%d", static_cast<int>(searchCode));
        return importFromDict( "esri_StatePlane_extra.wkt", codeS);
    }
    return OGRERR_FAILURE;
}
