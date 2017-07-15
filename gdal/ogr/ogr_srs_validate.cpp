/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of the OGRSpatialReference::Validate() method and
 *           related infrastructure.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_srs_api.h"

#include <cstdlib>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "osr_cs_wkt.h"

CPL_CVSID("$Id$")

// Why would fipszone and zone be parameters when they relate to a composite
// projection which renders down into a non-zoned projection?

static const char * const papszParameters[] =
{
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_ORIGIN,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    SRS_PP_AZIMUTH,
    SRS_PP_LONGITUDE_OF_POINT_1,
    SRS_PP_LATITUDE_OF_POINT_1,
    SRS_PP_LONGITUDE_OF_POINT_2,
    SRS_PP_LATITUDE_OF_POINT_2,
    SRS_PP_LONGITUDE_OF_POINT_3,
    SRS_PP_LATITUDE_OF_POINT_3,
    SRS_PP_LANDSAT_NUMBER,
    SRS_PP_PATH_NUMBER,
    SRS_PP_PERSPECTIVE_POINT_HEIGHT,
    SRS_PP_FIPSZONE,
    SRS_PP_ZONE,
    SRS_PP_RECTIFIED_GRID_ANGLE,
    SRS_PP_SATELLITE_HEIGHT,
    SRS_PP_PSEUDO_STD_PARALLEL_1,
    SRS_PP_LATITUDE_OF_1ST_POINT,
    SRS_PP_LONGITUDE_OF_1ST_POINT,
    SRS_PP_LATITUDE_OF_2ND_POINT,
    SRS_PP_LONGITUDE_OF_2ND_POINT,
    SRS_PP_PEG_POINT_LATITUDE,   // For SCH.
    SRS_PP_PEG_POINT_LONGITUDE,  // For SCH.
    SRS_PP_PEG_POINT_HEADING,    // For SCH.
    SRS_PP_PEG_POINT_HEIGHT,     // For SCH.
    NULL
};

// The following projection lists are incomplete.  They will likely
// change after the CT RPF response.  Examples show alternate forms with
// underscores instead of spaces.  Should we use the EPSG names were available?
// Plate-Caree has an accent in the spec!

static const char * const papszProjectionSupported[] =
{
    SRS_PT_CASSINI_SOLDNER,
    SRS_PT_BONNE,
    SRS_PT_EQUIDISTANT_CONIC,
    SRS_PT_EQUIRECTANGULAR,
    SRS_PT_ECKERT_I,
    SRS_PT_ECKERT_II,
    SRS_PT_ECKERT_III,
    SRS_PT_ECKERT_IV,
    SRS_PT_ECKERT_V,
    SRS_PT_ECKERT_VI,
    SRS_PT_MERCATOR_1SP,
    SRS_PT_MERCATOR_2SP,
    SRS_PT_MOLLWEIDE,
    SRS_PT_ROBINSON,
    SRS_PT_ALBERS_CONIC_EQUAL_AREA,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM,
    SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA,
    SRS_PT_TRANSVERSE_MERCATOR,
    SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED,
    SRS_PT_OBLIQUE_STEREOGRAPHIC,
    SRS_PT_POLAR_STEREOGRAPHIC,
    SRS_PT_HOTINE_OBLIQUE_MERCATOR,
    SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN,
    SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER,
    SRS_PT_LABORDE_OBLIQUE_MERCATOR,
    SRS_PT_SWISS_OBLIQUE_CYLINDRICAL,
    SRS_PT_AZIMUTHAL_EQUIDISTANT,
    SRS_PT_MILLER_CYLINDRICAL,
    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PT_SINUSOIDAL,
    SRS_PT_STEREOGRAPHIC,
    SRS_PT_GNOMONIC,
    SRS_PT_GALL_STEREOGRAPHIC,
    SRS_PT_ORTHOGRAPHIC,
    SRS_PT_POLYCONIC,
    SRS_PT_VANDERGRINTEN,
    SRS_PT_GEOSTATIONARY_SATELLITE,
    SRS_PT_TWO_POINT_EQUIDISTANT,
    SRS_PT_IMW_POLYCONIC,
    SRS_PT_WAGNER_I,
    SRS_PT_WAGNER_II,
    SRS_PT_WAGNER_III,
    SRS_PT_WAGNER_IV,
    SRS_PT_WAGNER_V,
    SRS_PT_WAGNER_VI,
    SRS_PT_WAGNER_VII,
    SRS_PT_QSC,
    SRS_PT_SCH,
    SRS_PT_GAUSSSCHREIBERTMERCATOR,
    SRS_PT_KROVAK,
    SRS_PT_CYLINDRICAL_EQUAL_AREA,
    SRS_PT_GOODE_HOMOLOSINE,
    SRS_PT_IGH,
    NULL
};

static const char * const papszProjectionUnsupported[] =
{
    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PT_TUNISIA_MINING_GRID,
    NULL
};

// List of supported projections with the PARAMETERS[] acceptable for each.
static const char * const papszProjWithParms[] = {

    SRS_PT_TRANSVERSE_MERCATOR,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_TUNISIA_MINING_GRID,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ALBERS_CONIC_EQUAL_AREA,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_AZIMUTHAL_EQUIDISTANT,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_BONNE,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_CYLINDRICAL_EQUAL_AREA,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_CASSINI_SOLDNER,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_EQUIDISTANT_CONIC,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_I,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_II,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_III,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_IV,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_V,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ECKERT_VI,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_EQUIRECTANGULAR,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_GALL_STEREOGRAPHIC,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_GNOMONIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_HOTINE_OBLIQUE_MERCATOR,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_AZIMUTH,
    SRS_PP_RECTIFIED_GRID_ANGLE,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_AZIMUTH,
    SRS_PP_RECTIFIED_GRID_ANGLE,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LATITUDE_OF_POINT_1,
    SRS_PP_LONGITUDE_OF_POINT_1,
    SRS_PP_LATITUDE_OF_POINT_2,
    SRS_PP_LONGITUDE_OF_POINT_2
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_MILLER_CYLINDRICAL,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_MERCATOR_1SP,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_MERCATOR_2SP,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_MOLLWEIDE,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ORTHOGRAPHIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_POLYCONIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_POLAR_STEREOGRAPHIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_ROBINSON,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_SINUSOIDAL,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_STEREOGRAPHIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_SWISS_OBLIQUE_CYLINDRICAL,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_OBLIQUE_STEREOGRAPHIC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_VANDERGRINTEN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_GEOSTATIONARY_SATELLITE,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SATELLITE_HEIGHT,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_KROVAK,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_AZIMUTH,
    SRS_PP_PSEUDO_STD_PARALLEL_1,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_TWO_POINT_EQUIDISTANT,
    SRS_PP_LATITUDE_OF_1ST_POINT,
    SRS_PP_LONGITUDE_OF_1ST_POINT,
    SRS_PP_LATITUDE_OF_2ND_POINT,
    SRS_PP_LONGITUDE_OF_2ND_POINT,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_IMW_POLYCONIC,
    SRS_PP_LATITUDE_OF_1ST_POINT,
    SRS_PP_LATITUDE_OF_2ND_POINT,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_I,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_II,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_III,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_IV,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_V,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_VI,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_WAGNER_VII,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_QSC,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    NULL,

    SRS_PT_SCH,
    SRS_PP_PEG_POINT_LATITUDE,
    SRS_PP_PEG_POINT_LONGITUDE,
    SRS_PP_PEG_POINT_HEADING,
    SRS_PP_PEG_POINT_HEIGHT,
    NULL,

    SRS_PT_GAUSSSCHREIBERTMERCATOR,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_GOODE_HOMOLOSINE,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    SRS_PT_IGH,
    NULL,

    NULL
};

static const char * const papszAliasGroupList[] = {
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_LATITUDE_OF_CENTER,
    NULL,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_ORIGIN,
    NULL,
    NULL
};

/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * \brief Validate SRS tokens.
 *
 * This method attempts to verify that the spatial reference system is
 * well formed, and consists of known tokens.  The validation is not
 * comprehensive.
 *
 * This method is the same as the C function OSRValidate().
 *
 * @return OGRERR_NONE if all is fine, OGRERR_CORRUPT_DATA if the SRS is
 * not well formed, and OGRERR_UNSUPPORTED_SRS if the SRS is well formed,
 * but contains non-standard PROJECTION[] values.
 */

OGRErr OGRSpatialReference::Validate()

{
/* -------------------------------------------------------------------- */
/*      Validate root node.                                             */
/* -------------------------------------------------------------------- */
    if( poRoot == NULL )
    {
        CPLDebug( "OGRSpatialReference::Validate", "No root pointer." );
        return OGRERR_CORRUPT_DATA;
    }

    OGRErr eErr = Validate(poRoot);

    // Even if hand-validation has succeeded, try a more formal validation
    // using the CT spec grammar.
    static int bUseCTGrammar = -1;
    if( bUseCTGrammar < 0 )
        bUseCTGrammar =
            CPLTestBool(CPLGetConfigOption("OSR_USE_CT_GRAMMAR", "TRUE"));

    if( eErr == OGRERR_NONE && bUseCTGrammar )
    {
        char* pszWKT = NULL;
        exportToWkt(&pszWKT);

        osr_cs_wkt_parse_context sContext;
        sContext.pszInput = pszWKT;
        sContext.pszLastSuccess = pszWKT;
        sContext.pszNext = pszWKT;
        sContext.szErrorMsg[0] = '\0';

        if( osr_cs_wkt_parse(&sContext) != 0 )
        {
            CPLDebug( "OGRSpatialReference::Validate", "%s",
                      sContext.szErrorMsg );
            eErr = OGRERR_CORRUPT_DATA;
        }

        CPLFree(pszWKT);
    }
    return eErr;
}

OGRErr OGRSpatialReference::Validate(OGR_SRSNode *poRoot)
{
    if( !EQUAL(poRoot->GetValue(), "GEOGCS")
        && !EQUAL(poRoot->GetValue(), "PROJCS")
        && !EQUAL(poRoot->GetValue(), "LOCAL_CS")
        && !EQUAL(poRoot->GetValue(), "GEOCCS")
        && !EQUAL(poRoot->GetValue(), "VERT_CS")
        && !EQUAL(poRoot->GetValue(), "COMPD_CS"))
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "Unrecognized root node `%s'",
                  poRoot->GetValue() );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      For a COMPD_CS, validate subparameters and head & tail cs       */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(), "COMPD_CS") )
    {
        for( int i = 1; i < poRoot->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(), "GEOGCS") ||
                EQUAL(poNode->GetValue(), "PROJCS") ||
                EQUAL(poNode->GetValue(), "LOCAL_CS") ||
                EQUAL(poNode->GetValue(), "GEOCCS") ||
                EQUAL(poNode->GetValue(), "VERT_CS") ||
                EQUAL(poNode->GetValue(), "COMPD_CS") )
            {
                OGRErr eErr = Validate(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "EXTENSION") )
            {
                // We do not try to control the sub-organization of
                // EXTENSION nodes.
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for COMPD_CS `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Validate VERT_CS                                                */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(), "VERT_CS") )
    {
        bool bGotVertDatum = false;
        bool bGotUnit = false;
        int nCountAxis = 0;

        for( int i = 1; i < poRoot->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(), "VERT_DATUM") )
            {
                OGRErr eErr = ValidateVertDatum(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
                bGotVertDatum = true;
            }
            else if( EQUAL(poNode->GetValue(), "UNIT") )
            {
                OGRErr eErr = ValidateUnit(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
                bGotUnit = true;
            }
            else if( EQUAL(poNode->GetValue(), "AXIS") )
            {
                OGRErr eErr = ValidateAxis(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
                nCountAxis++;
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for VERT_CS `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }

        if( !bGotVertDatum )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No VERT_DATUM child in VERT_CS." );

            return OGRERR_CORRUPT_DATA;
        }

        if( !bGotUnit )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No UNIT child in VERT_CS." );

            return OGRERR_CORRUPT_DATA;
        }

        if( nCountAxis > 1 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "Too many AXIS children in VERT_CS." );

            return OGRERR_CORRUPT_DATA;
        }
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Validate GEOCCS                                                 */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(), "GEOCCS") )
    {
        bool bGotDatum = false;
        bool bGotPrimeM = false;
        bool bGotUnit = false;
        int nCountAxis = 0;

        for( int i = 1; i < poRoot->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(), "DATUM") )
            {
                bGotDatum = true;
            }
            else if( EQUAL(poNode->GetValue(), "PRIMEM") )
            {
                bGotPrimeM = true;

                if( poNode->GetChildCount() < 2
                    || poNode->GetChildCount() > 3 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PRIMEM has wrong number of children (%d), "
                              "not 2 or 3 as expected.",
                              poNode->GetChildCount() );

                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(), "UNIT") )
            {
                OGRErr eErr = ValidateUnit(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
                bGotUnit = true;
            }
            else if( EQUAL(poNode->GetValue(), "AXIS") )
            {
                OGRErr eErr = ValidateAxis(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
                nCountAxis++;
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for GEOCCS `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }

        if( !bGotDatum )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No DATUM child in GEOCCS." );

            return OGRERR_CORRUPT_DATA;
        }

        if( !bGotPrimeM )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No PRIMEM child in GEOCCS." );

            return OGRERR_CORRUPT_DATA;
        }

        if( !bGotUnit )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No UNIT child in GEOCCS." );

            return OGRERR_CORRUPT_DATA;
        }

        if( nCountAxis != 0 && nCountAxis != 3 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "Wrong number of AXIS children in GEOCCS." );

            return OGRERR_CORRUPT_DATA;
        }
    }

/* -------------------------------------------------------------------- */
/*      For a PROJCS, validate subparameters (other than GEOGCS).       */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(), "PROJCS") )
    {
        for( int i = 1; i < poRoot->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(), "GEOGCS") )
            {
                // Validated elsewhere.
            }
            else if( EQUAL(poNode->GetValue(), "UNIT") )
            {
                OGRErr eErr = ValidateUnit(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "PARAMETER") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PARAMETER has wrong number of children (%d), "
                              "not 2 as expected.",
                              poNode->GetChildCount() );

                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( (char **)papszParameters,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognized PARAMETER `%s'.",
                              poNode->GetChild(0)->GetValue() );

                    return OGRERR_UNSUPPORTED_SRS;
                }
            }
            else if( EQUAL(poNode->GetValue(), "PROJECTION") )
            {
                if( poNode->GetChildCount() != 1 &&
                    poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PROJECTION has wrong number of children (%d), "
                              "not 1 or 2 as expected.",
                              poNode->GetChildCount() );

                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1
                      && CSLFindString( papszProjectionUnsupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognized PROJECTION `%s'.",
                              poNode->GetChild(0)->GetValue() );

                    return OGRERR_UNSUPPORTED_SRS;
                }
                else if( CSLFindString( papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unsupported, but recognized PROJECTION `%s'.",
                              poNode->GetChild(0)->GetValue() );

                    return OGRERR_UNSUPPORTED_SRS;
                }

                if( poNode->GetChildCount() == 2 )
                {
                    poNode = poNode->GetChild(1);
                    if( EQUAL(poNode->GetValue(), "AUTHORITY") )
                    {
                        const OGRErr eErr = ValidateAuthority(poNode);
                        if( eErr != OGRERR_NONE )
                            return eErr;
                    }
                    else
                    {
                        CPLDebug( "OGRSpatialReference::Validate",
                                  "Unexpected child for PROJECTION `%s'.",
                                  poNode->GetValue() );

                        return OGRERR_CORRUPT_DATA;
                    }
                }
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "AXIS") )
            {
                OGRErr eErr = ValidateAxis(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "EXTENSION") )
            {
                // We do not try to control the sub-organization of
                // EXTENSION nodes.
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for PROJCS `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate GEOGCS if found.                                       */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poGEOGCS = poRoot->GetNode( "GEOGCS" );

    if( poGEOGCS != NULL )
    {
        for( int i = 1; i < poGEOGCS->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poGEOGCS->GetChild(i);

            if( EQUAL(poNode->GetValue(), "DATUM") )
            {
                // Validated elsewhere.
            }
            else if( EQUAL(poNode->GetValue(), "PRIMEM") )
            {
                if( poNode->GetChildCount() < 2
                    || poNode->GetChildCount() > 3 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PRIMEM has wrong number of children (%d), "
                              "not 2 or 3 as expected.",
                              poNode->GetChildCount() );

                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(), "UNIT") )
            {
                const OGRErr eErr = ValidateUnit(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "AXIS") )
            {
                const OGRErr eErr = ValidateAxis(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "EXTENSION") )
            {
                // We do not try to control the sub-organization of
                // EXTENSION nodes.
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for GEOGCS `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }

        if( poGEOGCS->GetNode("DATUM") == NULL )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No DATUM child in GEOGCS." );

            return OGRERR_CORRUPT_DATA;
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate DATUM/SPHEROID.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDATUM = poRoot->GetNode( "DATUM" );

    if( poDATUM != NULL )
    {
        if( poDATUM->GetChildCount() == 0 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "DATUM has no children." );

            return OGRERR_CORRUPT_DATA;
        }

        bool bGotSpheroid = false;

        for( int i = 1; i < poDATUM->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode = poDATUM->GetChild(i);

            if( EQUAL(poNode->GetValue(), "SPHEROID") )
            {
                OGR_SRSNode *poSPHEROID = poDATUM->GetChild(1);
                bGotSpheroid = true;

                if( poSPHEROID->GetChildCount() != 3
                    && poSPHEROID->GetChildCount() != 4 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "SPHEROID has wrong number of children (%d), "
                              "not 3 or 4 as expected.",
                              poSPHEROID->GetChildCount() );

                    return OGRERR_CORRUPT_DATA;
                }
                else if( CPLAtof(poSPHEROID->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "SPHEROID semi-major axis is zero (%s)!",
                              poSPHEROID->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(), "AUTHORITY") )
            {
                OGRErr eErr = ValidateAuthority(poNode);
                if( eErr != OGRERR_NONE )
                    return eErr;
            }
            else if( EQUAL(poNode->GetValue(), "TOWGS84") )
            {
                if( poNode->GetChildCount() != 3
                    && poNode->GetChildCount() != 7)
                {
                    CPLDebug("OGRSpatialReference::Validate",
                             "TOWGS84 has wrong number of children (%d), "
                             "not 3 or 7.",
                             poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(), "EXTENSION") )
            {
                // We do not try to control the sub-organization of
                // EXTENSION nodes.
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for DATUM `%s'.",
                          poNode->GetValue() );

                return OGRERR_CORRUPT_DATA;
            }
        }

        if( !bGotSpheroid )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No SPHEROID child in DATUM." );

            return OGRERR_CORRUPT_DATA;
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is projected, try to validate the detailed set of       */
/*      parameters used for the projection.                             */
/* -------------------------------------------------------------------- */
    const OGRErr eErr = ValidateProjection(poRoot);
    if( eErr != OGRERR_NONE )
        return eErr;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRValidate()                             */
/************************************************************************/
/**
 * \brief Validate SRS tokens.
 *
 * This function is the same as the C++ method OGRSpatialReference::Validate().
 */
OGRErr OSRValidate( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRValidate", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->Validate();
}

/************************************************************************/
/*                             IsAliasFor()                             */
/************************************************************************/

/**
 * \brief Return whether the first string passed in an acceptable alias for the
 * second string according to the AliasGroupList
 *
 * @param pszParm1 first string
 * @param pszParm2 second string
 *
 * @return TRUE if both strings are aliases according to the
 * AliasGroupList, FALSE otherwise
 */
int OGRSpatialReference::IsAliasFor( const char *pszParm1,
                                     const char *pszParm2 )

{
/* -------------------------------------------------------------------- */
/*      Look for a group containing pszParm1.                           */
/* -------------------------------------------------------------------- */
    int iGroup = 0;  // Used after for.
    for( ; papszAliasGroupList[iGroup] != NULL; iGroup++ )
    {
        int i = iGroup;  // Used after for.

        for( ; papszAliasGroupList[i] != NULL; i++ )
        {
            if( EQUAL(pszParm1, papszAliasGroupList[i]) )
                break;
        }

        if( papszAliasGroupList[i] == NULL )
            iGroup = i;
        else
            break;
    }

/* -------------------------------------------------------------------- */
/*      Does this group also contain pszParm2?                          */
/* -------------------------------------------------------------------- */
    while( papszAliasGroupList[iGroup] != NULL )
    {
        if( EQUAL(papszAliasGroupList[iGroup++], pszParm2) )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                         ValidateProjection()                         */
/************************************************************************/

/**
 * \brief Validate the current PROJECTION's arguments.
 *
 * @return OGRERR_NONE if the PROJECTION's arguments validate, an error code
 *         otherwise
 */
OGRErr OGRSpatialReference::ValidateProjection(OGR_SRSNode *poRoot)
{
    OGR_SRSNode *poPROJCS = poRoot->GetNode( "PROJCS" );

    if( poPROJCS == NULL )
        return OGRERR_NONE;

    if( poPROJCS->GetNode( "PROJECTION" ) == NULL )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "PROJCS does not have PROJECTION subnode." );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Find the matching group in the proj and parms table.            */
/* -------------------------------------------------------------------- */
    const char *pszProjection =
        poPROJCS->GetNode("PROJECTION")->GetChild(0)->GetValue();

    int iOffset = 0;  // Used after for.
    for( ;
         papszProjWithParms[iOffset] != NULL
             && !EQUAL(papszProjWithParms[iOffset], pszProjection); )
    {
        while( papszProjWithParms[iOffset] != NULL )
            iOffset++;
        iOffset++;
    }

    if( papszProjWithParms[iOffset] == NULL )
        return OGRERR_UNSUPPORTED_SRS;

    iOffset++;

/* -------------------------------------------------------------------- */
/*      Check all parameters, and verify they are in the permitted      */
/*      list.                                                           */
/* -------------------------------------------------------------------- */
    for( int iNode = 0; iNode < poPROJCS->GetChildCount(); iNode++ )
    {
        OGR_SRSNode *poParm = poPROJCS->GetChild(iNode);

        if( !EQUAL(poParm->GetValue(), "PARAMETER") )
            continue;

        OGR_SRSNode *poParmNameNode = poParm->GetChild(0);
        if( poParmNameNode == NULL )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "Parameter name for PROJECTION %s is corrupt.",
                       pszProjection );
            return OGRERR_CORRUPT_DATA;
        }
        const char *pszParmName = poParmNameNode->GetValue();

        int i = iOffset;  // Used after for.
        for( ; papszProjWithParms[i] != NULL; i++ )
        {
            if( EQUAL(papszProjWithParms[i], pszParmName) )
                break;
        }

        // This parameter is not an exact match, is it an alias?
        if( papszProjWithParms[i] == NULL )
        {
            for( i = iOffset; papszProjWithParms[i] != NULL; i++ )
            {
                if( IsAliasFor(papszProjWithParms[i], pszParmName) )
                    break;
            }

            if( papszProjWithParms[i] == NULL )
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "PARAMETER %s for PROJECTION %s is not permitted.",
                          pszParmName, pszProjection );
                return OGRERR_CORRUPT_DATA;
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "PARAMETER %s for PROJECTION %s is an alias for %s.",
                          pszParmName, pszProjection,
                          papszProjWithParms[i] );
                return OGRERR_CORRUPT_DATA;
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         ValidateVertDatum()                          */
/************************************************************************/

/**
 * \brief Validate the current VERT_DATUM's arguments.
 *
 * @return OGRERR_NONE if the VERT_DATUM's arguments validate, an error code
 *         otherwise
 */
OGRErr OGRSpatialReference::ValidateVertDatum( OGR_SRSNode *poRoot )
{
    if( !EQUAL(poRoot->GetValue(), "VERT_DATUM") )
        return OGRERR_NONE;

    if( poRoot->GetChildCount() < 2 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "Invalid number of children : %d", poRoot->GetChildCount() );
        return OGRERR_CORRUPT_DATA;
    }

    if( atoi(poRoot->GetChild(1)->GetValue()) == 0 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "Invalid value for datum type (%s) : must be a number",
                  poRoot->GetChild(1)->GetValue());
        return OGRERR_CORRUPT_DATA;
    }

    for( int i = 2; i < poRoot->GetChildCount(); i++ )
    {
        OGR_SRSNode *poNode = poRoot->GetChild(i);

        if( EQUAL(poNode->GetValue(), "AUTHORITY") )
        {
            OGRErr eErr = ValidateAuthority(poNode);
            if( eErr != OGRERR_NONE )
                return eErr;
        }
        else if( EQUAL(poNode->GetValue(), "EXTENSION") )
        {
            // We do not try to control the sub-organization of
            // EXTENSION nodes.
        }
        else
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "Unexpected child for VERT_DATUM `%s'.",
                      poNode->GetValue() );

            return OGRERR_CORRUPT_DATA;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         ValidateAuthority()                          */
/************************************************************************/

/**
 * \brief Validate the current AUTHORITY's arguments.
 *
 * @return OGRERR_NONE if the AUTHORITY's arguments validate, an error code
 *         otherwise
 */
OGRErr OGRSpatialReference::ValidateAuthority( OGR_SRSNode *poRoot )
{
    if( !EQUAL(poRoot->GetValue(), "AUTHORITY") )
        return OGRERR_NONE;

    if( poRoot->GetChildCount() != 2 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "AUTHORITY has wrong number of children (%d), not 2.",
                  poRoot->GetChildCount() );
        return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ValidateAxis()                             */
/************************************************************************/

/**
 * \brief Validate the current AXIS's arguments.
 *
 * @return OGRERR_NONE if the AXIS's arguments validate, an error code
 *         otherwise
 */
OGRErr OGRSpatialReference::ValidateAxis( OGR_SRSNode *poRoot )
{
    if( !EQUAL(poRoot->GetValue(), "AXIS") )
        return OGRERR_NONE;

    if( poRoot->GetChildCount() != 2 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                    "AXIS has wrong number of children (%d), not 2.",
                    poRoot->GetChildCount() );
        return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ValidateUnit()                             */
/************************************************************************/

/**
 * \brief Validate the current UNIT's arguments.
 *
 * @return OGRERR_NONE if the UNIT's arguments validate, an error code
 *         otherwise
 */
OGRErr OGRSpatialReference::ValidateUnit( OGR_SRSNode *poRoot )
{
    if( !EQUAL(poRoot->GetValue(), "UNIT") )
        return OGRERR_NONE;

    if( poRoot->GetChildCount() != 2
        && poRoot->GetChildCount() != 3 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "UNIT has wrong number of children (%d), not 2.",
                  poRoot->GetChildCount() );
        return OGRERR_CORRUPT_DATA;
    }
    else if( CPLAtof(poRoot->GetChild(1)->GetValue()) == 0.0 )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "UNIT does not appear to have meaningful"
                  "coefficient (%s).",
                  poRoot->GetChild(1)->GetValue() );
        return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}
