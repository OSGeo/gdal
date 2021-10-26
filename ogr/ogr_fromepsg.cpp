/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate an OGRSpatialReference object based on an EPSG
 *           PROJCS, or GEOGCS code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <cctype>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <limits>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_proj_p.h"
#include "ogr_spatialref.h"

#include "proj.h"

CPL_CVSID("$Id$")

extern void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue );

/************************************************************************/
/*                         OSRGetEllipsoidInfo()                        */
/************************************************************************/

/**
 * Fetch info about an ellipsoid.
 *
 * This helper function will return ellipsoid parameters corresponding to EPSG
 * code provided. Axes are always returned in meters.  Semi major computed
 * based on inverse flattening where that is provided.
 *
 * @param nCode EPSG code of the requested ellipsoid
 *
 * @param ppszName pointer to string where ellipsoid name will be returned. It
 * is caller responsibility to free this string after using with CPLFree().
 *
 * @param pdfSemiMajor pointer to variable where semi major axis will be
 * returned.
 *
 * @param pdfInvFlattening pointer to variable where inverse flattening will
 * be returned.
 *
 * @return OGRERR_NONE on success or an error code in case of failure.
 **/

OGRErr
OSRGetEllipsoidInfo( int nCode, char ** ppszName,
                     double * pdfSemiMajor, double * pdfInvFlattening )

{
    CPLString osCode;
    osCode.Printf("%d", nCode);
    auto ellipsoid = proj_create_from_database(OSRGetProjTLSContext(),
                                             "EPSG",
                                             osCode.c_str(),
                                             PJ_CATEGORY_ELLIPSOID,
                                             false,
                                             nullptr);
    if( !ellipsoid )
    {
        return OGRERR_UNSUPPORTED_SRS;
    }

    if( ppszName )
    {
        *ppszName = CPLStrdup(proj_get_name(ellipsoid));
    }
    proj_ellipsoid_get_parameters(OSRGetProjTLSContext(),
                                      ellipsoid,
                                      pdfSemiMajor,
                                      nullptr,
                                      nullptr,
                                      pdfInvFlattening);
    proj_destroy(ellipsoid);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetStatePlane()                            */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This will attempt to generate a complete definition of a state plane
 * zone based on generating the entire SRS from the EPSG tables.  If the
 * EPSG tables are unavailable, it will produce a stubbed LOCAL_CS definition
 * and return OGRERR_FAILURE.
 *
 * This method is the same as the C function OSRSetStatePlaneWithUnits().
 *
 * @param nZone State plane zone number, in the USGS numbering scheme (as
 * distinct from the Arc/Info and Erdas numbering scheme.
 *
 * @param bNAD83 TRUE if the NAD83 zone definition should be used or FALSE
 * if the NAD27 zone definition should be used.
 *
 * @param pszOverrideUnitName Linear unit name to apply overriding the
 * legal definition for this zone.
 *
 * @param dfOverrideUnit Linear unit conversion factor to apply overriding
 * the legal definition for this zone.
 *
 * @return OGRERR_NONE on success, or OGRERR_FAILURE on failure, mostly likely
 * due to the EPSG tables not being accessible.
 */

OGRErr OGRSpatialReference::SetStatePlane( int nZone, int bNAD83,
                                           const char *pszOverrideUnitName,
                                           double dfOverrideUnit )

{

/* -------------------------------------------------------------------- */
/*      Get the index id from stateplane.csv.                           */
/* -------------------------------------------------------------------- */

    if( !bNAD83 && nZone > INT_MAX - 10000 )
        return OGRERR_FAILURE;

    const int nAdjustedId = bNAD83 ? nZone : nZone + 10000;

/* -------------------------------------------------------------------- */
/*      Turn this into a PCS code.  We assume there will only be one    */
/*      PCS corresponding to each Proj_ code since the proj code        */
/*      already effectively indicates NAD27 or NAD83.                   */
/* -------------------------------------------------------------------- */
    char szID[32] = {};
    snprintf( szID, sizeof(szID), "%d", nAdjustedId );
    const int nPCSCode =
        atoi( CSVGetField( CSVFilename( "stateplane.csv" ),
                           "ID", szID, CC_Integer,
                           "EPSG_PCS_CODE" ) );
    if( nPCSCode < 1 )
    {
        static bool bFailureReported = false;

        if( !bFailureReported )
        {
            bFailureReported = true;
            CPLError( CE_Warning, CPLE_OpenFailed,
                      "Unable to find state plane zone in stateplane.csv, "
                      "likely because the GDAL data files cannot be found.  "
                      "Using incomplete definition of state plane zone." );
        }

        Clear();
        if( bNAD83 )
        {
            char szName[128] = {};
            snprintf( szName, sizeof(szName),
                      "State Plane Zone %d / NAD83", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_METER, 1.0 );
        }
        else
        {
            char szName[128] = {};
            snprintf( szName, sizeof(szName),
                      "State Plane Zone %d / NAD27", nZone );
            SetLocalCS( szName );
            SetLinearUnits( SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV) );
        }

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Define based on a full EPSG definition of the zone.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr = importFromEPSG( nPCSCode );

    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Apply units override if required.                               */
/*                                                                      */
/*      We will need to adjust the linear projection parameter to       */
/*      match the provided units, and clear the authority code.         */
/* -------------------------------------------------------------------- */
    if( pszOverrideUnitName != nullptr && dfOverrideUnit != 0.0
        && fabs(dfOverrideUnit - GetLinearUnits()) > 0.0000000001 )
    {
        const double dfFalseEasting = GetNormProjParm( SRS_PP_FALSE_EASTING );
        const double dfFalseNorthing = GetNormProjParm( SRS_PP_FALSE_NORTHING);

        SetLinearUnits( pszOverrideUnitName, dfOverrideUnit );

        SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
        SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

        OGR_SRSNode * const poPROJCS = GetAttrNode( "PROJCS" );
        if( poPROJCS != nullptr && poPROJCS->FindChild( "AUTHORITY" ) != -1 )
        {
            poPROJCS->DestroyChild( poPROJCS->FindChild( "AUTHORITY" ) );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetStatePlane()                          */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This function is the same as OGRSpatialReference::SetStatePlane().
 */

OGRErr OSRSetStatePlane( OGRSpatialReferenceH hSRS, int nZone, int bNAD83 )

{
    VALIDATE_POINTER1( hSRS, "OSRSetStatePlane", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetStatePlane( nZone, bNAD83 );
}

/************************************************************************/
/*                     OSRSetStatePlaneWithUnits()                      */
/************************************************************************/

/**
 * \brief Set State Plane projection definition.
 *
 * This function is the same as OGRSpatialReference::SetStatePlane().
 */

OGRErr OSRSetStatePlaneWithUnits( OGRSpatialReferenceH hSRS,
                                  int nZone, int bNAD83,
                                  const char *pszOverrideUnitName,
                                  double dfOverrideUnit )

{
    VALIDATE_POINTER1( hSRS, "OSRSetStatePlaneWithUnits", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetStatePlane( nZone, bNAD83,
                       pszOverrideUnitName,
                       dfOverrideUnit );
}

/************************************************************************/
/*                          AutoIdentifyEPSG()                          */
/************************************************************************/

/**
 * \brief Set EPSG authority info if possible.
 *
 * This method inspects a WKT definition, and adds EPSG authority nodes
 * where an aspect of the coordinate system can be easily and safely
 * corresponded with an EPSG identifier.  In practice, this method will
 * evolve over time.  In theory it can add authority nodes for any object
 * (i.e. spheroid, datum, GEOGCS, units, and PROJCS) that could have an
 * authority node.  Mostly this is useful to inserting appropriate
 * PROJCS codes for common formulations (like UTM n WGS84).
 *
 * If it success the OGRSpatialReference is updated in place, and the
 * method return OGRERR_NONE.  If the method fails to identify the
 * general coordinate system OGRERR_UNSUPPORTED_SRS is returned but no
 * error message is posted via CPLError().
 *
 * This method is the same as the C function OSRAutoIdentifyEPSG().
 *
 * Since GDAL 2.3, the FindMatches() method can also be used for improved
 * matching by researching the EPSG catalog.
 *
 * @return OGRERR_NONE or OGRERR_UNSUPPORTED_SRS.
 */

OGRErr OGRSpatialReference::AutoIdentifyEPSG()

{
/* -------------------------------------------------------------------- */
/*      Do we have a GEOGCS node, but no authority?  If so, try         */
/*      guessing it.                                                    */
/* -------------------------------------------------------------------- */
    if( (IsProjected() || IsGeographic())
        && GetAuthorityCode( "GEOGCS" ) == nullptr )
    {
        const int nGCS = GetEPSGGeogCS();
        if( nGCS != -1 )
            SetAuthority( "GEOGCS", "EPSG", nGCS );
    }

    if( IsProjected() && GetAuthorityCode( "PROJCS") == nullptr )
    {
        const char *pszProjection = GetAttrValue( "PROJECTION" );

/* -------------------------------------------------------------------- */
/*      Is this a UTM coordinate system with a common GEOGCS?           */
/* -------------------------------------------------------------------- */
        int nZone = 0;
        int bNorth = FALSE;
        if( (nZone = GetUTMZone( &bNorth )) != 0 )
        {
            const char *pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
            const char *pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );

            if( pszAuthName == nullptr || pszAuthCode == nullptr )
            {
                // Don't exactly recognise datum.
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4326 )
            {
                // WGS84
                if( bNorth )
                    SetAuthority( "PROJCS", "EPSG", 32600 + nZone );
                else
                    SetAuthority( "PROJCS", "EPSG", 32700 + nZone );
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4267
                    && nZone >= 3 && nZone <= 22 && bNorth )
            {
                SetAuthority( "PROJCS", "EPSG", 26700 + nZone ); // NAD27
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4269
                    && nZone >= 3 && nZone <= 23 && bNorth )
            {
                SetAuthority( "PROJCS", "EPSG", 26900 + nZone ); // NAD83
            }
            else if( EQUAL(pszAuthName, "EPSG") && atoi(pszAuthCode) == 4322 )
            { // WGS72
                if( bNorth )
                    SetAuthority( "PROJCS", "EPSG", 32200 + nZone );
                else
                    SetAuthority( "PROJCS", "EPSG", 32300 + nZone );
            }
        }

/* -------------------------------------------------------------------- */
/*      Is this a Polar Stereographic system on WGS 84 ?                */
/* -------------------------------------------------------------------- */
        else if ( pszProjection != nullptr &&
                  EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
        {
            const char *pszAuthName = GetAuthorityName( "PROJCS|GEOGCS" );
            const char *pszAuthCode = GetAuthorityCode( "PROJCS|GEOGCS" );
            const double dfLatOrigin = GetNormProjParm(
                                            SRS_PP_LATITUDE_OF_ORIGIN, 0.0 );

            if( pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG") &&
                pszAuthCode != nullptr && atoi(pszAuthCode) == 4326 &&
                fabs( fabs(dfLatOrigin ) - 71.0 ) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0 )) < 1e-15 &&
                fabs(GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) - 1.0) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 )) < 1e-15 &&
                fabs(GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 )) < 1e-15 &&
                fabs(GetLinearUnits() - 1.0) < 1e-15 )
            {
                if( dfLatOrigin > 0 )
                    // Arctic Polar Stereographic
                    SetAuthority( "PROJCS", "EPSG", 3995 );
                else
                    // Antarctic Polar Stereographic
                    SetAuthority( "PROJCS", "EPSG", 3031 );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Return.                                                         */
/* -------------------------------------------------------------------- */
    if( IsProjected() && GetAuthorityCode("PROJCS") != nullptr )
        return OGRERR_NONE;

    if( IsGeographic() && GetAuthorityCode("GEOGCS") != nullptr )
        return OGRERR_NONE;

    return OGRERR_UNSUPPORTED_SRS;
}

/************************************************************************/
/*                        OSRAutoIdentifyEPSG()                         */
/************************************************************************/

/**
 * \brief Set EPSG authority info if possible.
 *
 * This function is the same as OGRSpatialReference::AutoIdentifyEPSG().
 *
 * Since GDAL 2.3, the OSRFindMatches() function can also be used for improved
 * matching by researching the EPSG catalog.
 *
 */

OGRErr OSRAutoIdentifyEPSG( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRAutoIdentifyEPSG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->AutoIdentifyEPSG();
}

