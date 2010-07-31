/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of the OGRSpatialReference::Validate() method and
 *           related infrastructure.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_spatialref.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/* why would fipszone and zone be paramers when they relate to a composite
   projection which renders done into a non-zoned projection? */

static const char *papszParameters[] =
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
    NULL
};

// the following projection lists are incomplete.  they will likely
// change after the CT RPF response.  Examples show alternate forms with
// underscores instead of spaces.  Should we use the EPSG names were available?
// Plate-Caree has an accent in the spec!

static const char *papszProjectionSupported[] =
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
    SRS_PT_LABORDE_OBLIQUE_MERCATOR,
    SRS_PT_SWISS_OBLIQUE_CYLINDRICAL,
    SRS_PT_AZIMUTHAL_EQUIDISTANT,
    SRS_PT_MILLER_CYLINDRICAL,
    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PT_SINUSOIDAL,
    SRS_PT_STEREOGRAPHIC,
    SRS_PT_GNOMONIC,
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
    SRS_PT_GAUSSSCHREIBERTMERCATOR,
    NULL
};

static const char *papszProjectionUnsupported[] =
{
    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PT_TUNISIA_MINING_GRID,
    NULL
};

/*
** List of supported projections with the PARAMETERS[] acceptable for each.
*/
static const char *papszProjWithParms[] = {

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

    SRS_PT_ECKERT_IV,
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

    SRS_PT_GAUSSSCHREIBERTMERCATOR,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    NULL,

    NULL
};

static const char *papszAliasGroupList[] = {
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
        CPLDebug( "OGRSpatialReference::Validate",
                  "No root pointer.\n" );
        return OGRERR_CORRUPT_DATA;
    }
    
    if( !EQUAL(poRoot->GetValue(),"GEOGCS")
        && !EQUAL(poRoot->GetValue(),"PROJCS")
        && !EQUAL(poRoot->GetValue(),"LOCAL_CS")
        && !EQUAL(poRoot->GetValue(),"GEOCCS") )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "Unrecognised root node `%s'\n",
                  poRoot->GetValue() );
        return OGRERR_CORRUPT_DATA;
    }
    
/* -------------------------------------------------------------------- */
/*      For a PROJCS, validate subparameters (other than GEOGCS).       */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(),"PROJCS") )
    {
        OGR_SRSNode     *poNode;
        int             i;

        for( i = 1; i < poRoot->GetChildCount(); i++ )
        {
            poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(),"GEOGCS") )
            {
                /* validated elsewhere */
            }
            else if( EQUAL(poNode->GetValue(),"UNIT") )
            {
                if( poNode->GetChildCount() != 2 
                    && poNode->GetChildCount() != 3 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                           "UNIT has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CPLAtof(poNode->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "UNIT does not appear to have meaningful"
                              "coefficient (%s).\n",
                              poNode->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"PARAMETER") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PARAMETER has wrong number of children (%d),"
                              "not 2 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( (char **)papszParameters,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognised PARAMETER `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
            }
            else if( EQUAL(poNode->GetValue(),"PROJECTION") )
            {
                if( poNode->GetChildCount() != 1 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PROJECTION has wrong number of children (%d),"
                              "not 1 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( (char **)papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1
                      && CSLFindString( (char **)papszProjectionUnsupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognised PROJECTION `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
                else if( CSLFindString( (char **)papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unsupported, but recognised PROJECTION `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
            }
            else if( EQUAL(poNode->GetValue(),"AUTHORITY") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "AUTHORITY has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"AXIS") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "AXIS has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"EXTENSION") )
            {
                // We do not try to control the sub-organization of 
                // EXTENSION nodes.
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for PROJCS `%s'.\n",
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
        OGR_SRSNode     *poNode;
        int             i;

        for( i = 1; i < poGEOGCS->GetChildCount(); i++ )
        {
            poNode = poGEOGCS->GetChild(i);

            if( EQUAL(poNode->GetValue(),"DATUM") )
            {
                /* validated elsewhere */
            }
            else if( EQUAL(poNode->GetValue(),"PRIMEM") )
            {
                if( poNode->GetChildCount() < 2 
                    || poNode->GetChildCount() > 3 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PRIMEM has wrong number of children (%d),"
                              "not 2 or 3 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"UNIT") )
            {
                if( poNode->GetChildCount() != 2 
                    && poNode->GetChildCount() != 3 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "UNIT has wrong number of children (%d), not 2 or 3.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CPLAtof(poNode->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "UNIT does not appear to have meaningful"
                              "coefficient (%s).\n",
                              poNode->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"AXIS") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "AXIS has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"AUTHORITY") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "AUTHORITY has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for GEOGCS `%s'.\n",
                          poNode->GetValue() );
                
                return OGRERR_CORRUPT_DATA;
            }
        }

        if( poGEOGCS->GetNode("DATUM") == NULL )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No DATUM child in GEOGCS.\n" );
            
            return OGRERR_CORRUPT_DATA;
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate DATUM/SPHEROID.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDATUM = poRoot->GetNode( "DATUM" );

    if( poDATUM != NULL )
    {
        OGR_SRSNode     *poSPHEROID;
        int             bGotSpheroid = FALSE;
        int             i;

        if( poDATUM->GetChildCount() == 0 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "DATUM has no children." );
            
            return OGRERR_CORRUPT_DATA;
        }

        for( i = 1; i < poDATUM->GetChildCount(); i++ )
        {
            OGR_SRSNode *poNode;
            poNode = poDATUM->GetChild(i);

            if( EQUAL(poNode->GetValue(),"SPHEROID") )
            {
                poSPHEROID = poDATUM->GetChild(1);
                bGotSpheroid = TRUE;

                if( poSPHEROID->GetChildCount() != 3 
                    && poSPHEROID->GetChildCount() != 4 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "SPHEROID has wrong number of children (%d),"
                              "not 3 or 4 as expected.\n",
                              poSPHEROID->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CPLAtof(poSPHEROID->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "SPHEROID semi-major axis is zero (%s)!\n",
                              poSPHEROID->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"AUTHORITY") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                       "AUTHORITY has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"TOWGS84") )
            {
                if( poNode->GetChildCount() != 3 
                    && poNode->GetChildCount() != 7)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                   "TOWGS84 has wrong number of children (%d), not 3 or 7.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for DATUM `%s'.\n",
                          poNode->GetValue() );
                
                return OGRERR_CORRUPT_DATA;
            }
        }

        if( !bGotSpheroid )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "No SPHEROID child in DATUM.\n" );
            
            return OGRERR_CORRUPT_DATA;
        }
    }        

/* -------------------------------------------------------------------- */
/*      If this is projected, try to validate the detailed set of       */
/*      parameters used for the projection.                             */
/* -------------------------------------------------------------------- */
    OGRErr  eErr;

    eErr = ValidateProjection();
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Final check.                                                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(),"GEOCCS") )
        return OGRERR_UNSUPPORTED_SRS;

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
    VALIDATE_POINTER1( hSRS, "OSRValidate", CE_Failure );

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
 * @return TRUE if both strings are aliases according to the AliasGroupList, FALSE otherwise
 */
int OGRSpatialReference::IsAliasFor( const char *pszParm1, 
                                     const char *pszParm2 )

{
    int         iGroup;

/* -------------------------------------------------------------------- */
/*      Look for a group containing pszParm1.                           */
/* -------------------------------------------------------------------- */
    for( iGroup = 0; papszAliasGroupList[iGroup] != NULL; iGroup++ )
    {
        int     i;

        for( i = iGroup; papszAliasGroupList[i] != NULL; i++ )
        {
            if( EQUAL(pszParm1,papszAliasGroupList[i]) )
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
        if( EQUAL(papszAliasGroupList[iGroup++],pszParm2) )
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
OGRErr OGRSpatialReference::ValidateProjection()

{
    OGR_SRSNode *poPROJCS = poRoot->GetNode( "PROJCS" );

    if( poPROJCS == NULL  )
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
    const char *pszProjection;
    int        iOffset;
    
    pszProjection = poPROJCS->GetNode("PROJECTION")->GetChild(0)->GetValue();

    for( iOffset = 0; 
         papszProjWithParms[iOffset] != NULL
             && !EQUAL(papszProjWithParms[iOffset],pszProjection); )
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
    int iNode;

    for( iNode = 0; iNode < poPROJCS->GetChildCount(); iNode++ )
    {
        OGR_SRSNode *poParm = poPROJCS->GetChild(iNode);
        int          i;
        const char  *pszParmName;

        if( !EQUAL(poParm->GetValue(),"PARAMETER") )
            continue;

        pszParmName = poParm->GetChild(0)->GetValue();

        for( i = iOffset; papszProjWithParms[i] != NULL; i++ )
        {
            if( EQUAL(papszProjWithParms[i],pszParmName) )
                break;
        }

        /* This parameter is not an exact match, is it an alias? */
        if( papszProjWithParms[i] == NULL )
        {
            for( i = iOffset; papszProjWithParms[i] != NULL; i++ )
            {
                if( IsAliasFor(papszProjWithParms[i],pszParmName) )
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
